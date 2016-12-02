#include "csMain.h"

#include "plugin.h"

CLAW_CALLBACK(SearchWindowClosing);
CLAW_CALLBACK(FirstScan);
CLAW_CALLBACK(NextScan);
CLAW_CALLBACK(ScanValueTypeChanged);

int _claw_ResultClicked(Ihandle* handle, char* text, int item, int state) { _csMain->ResultClicked(text, item, state); return 0; }

csMain::csMain()
	: m_scanner(this)
{
	m_hDialog = nullptr;

	m_hButtonFirstScan = nullptr;
	m_hButtonNextScan = nullptr;

	m_hCheckHex = nullptr;
	m_hTextInput = nullptr;

	m_hFrameScanOptions = nullptr;
	m_hFloatMethod = nullptr;
	m_hCheckFastScan = nullptr;
	m_hTextFastScanAlign = nullptr;
	m_hCheckPauseWhileScanning = nullptr;

	m_hListResults = nullptr;

	m_currentScan = 0;
}

csMain::~csMain()
{
	if (m_hDialog != nullptr) {
		Close();
	}
}

SearchValueMethod csMain::MethodForType(SearchValueType type)
{
	switch (type) {
	case SVT_Char:
	case SVT_Int16:
	case SVT_Int32:
	case SVT_Int64:
		return SVM_Integer;

	case SVT_Float:
	case SVT_Double:
		return SVM_Float;
	}

	return SVM_Unknown;
}

int csMain::SearchWindowClosing()
{
	m_hDialog = nullptr;
	return IUP_CLOSE;
}

void csMain::PerformScan(bool firstScan)
{
	IupSetAttribute(m_hButtonFirstScan, "ACTIVE", "NO");
	IupSetAttribute(m_hButtonNextScan, "ACTIVE", "NO");

	m_scanner.m_inputText = IupGetAttribute(m_hTextInput, "VALUE");
	m_scanner.m_inputIsHex = !strcmp(IupGetAttribute(m_hCheckHex, "VALUE"), "ON");
	m_scanner.m_pauseWhileScanning = DbgIsRunning() && !strcmp(IupGetAttribute(m_hCheckPauseWhileScanning, "VALUE"), "ON");

	if (!strcmp(IupGetAttribute(m_hCheckFastScan, "VALUE"), "ON")) {
		sscanf(IupGetAttribute(m_hTextFastScanAlign, "VALUE"), "%d", &m_scanner.m_scanStep);

		if (m_scanner.m_scanStep < 1) {
			m_scanner.m_scanStep = 1;
		}
	}

	m_scanner.m_currentScanFloatTruncate = !strcmp(IupGetAttribute(m_hFloatMethod, "VALUE"), "trunc");
	m_scanner.m_currentScanFloatRound = !strcmp(IupGetAttribute(m_hFloatMethod, "VALUE"), "round");
	m_scanner.m_currentScanFloatRound2 = !strcmp(IupGetAttribute(m_hFloatMethod, "VALUE"), "round2");

	m_scanner.PerformScan(firstScan);

	IupSetAttribute(m_hListResults, "REMOVEITEM", "ALL");
	IupSetAttribute(m_hListResults, "AUTOREDRAW", "NO");

	int numResults = m_scanner.m_results.Count();
	for (int i = 0; i < numResults; i++) {
		SearchResult &result = m_scanner.m_results[i];

		ptr_t pointer = result.m_base + result.m_offset;

		s::String strLine = s::strPrintF("%p", pointer);

		//TODO: Can we up the performance on this?
		if (numResults < 20) {
			char moduleName[MAX_MODULE_SIZE];
			if (DbgGetModuleAt(pointer, moduleName)) {
				strLine += s::strPrintF(" (%s)", moduleName);
			}

			char label[MAX_LABEL_SIZE];
			if (DbgGetLabelAt(pointer, SEG_DEFAULT, label)) {
				strLine += s::strPrintF(" %s", label);
			}
		}

		IupSetAttribute(m_hListResults, "APPENDITEM", strLine);
	}

	IupSetAttribute(m_hListResults, "AUTOREDRAW", "YES");
	IupRedraw(m_hListResults, 1);

	IupSetAttribute(m_hButtonFirstScan, "ACTIVE", "YES");
	IupSetAttribute(m_hButtonNextScan, "ACTIVE", "YES");
}

int csMain::FirstScan()
{
	if (m_currentScan > 0) {
		m_currentScan = 0;

		IupSetAttribute(m_hListResults, "REMOVEITEM", "ALL");

		IupSetAttribute(m_hButtonFirstScan, "TITLE", "First Scan");

		IupSetAttribute(m_hComboValueType, "ACTIVE", "YES");
		IupSetAttribute(m_hButtonNextScan, "ACTIVE", "NO");
		return 0;
	}

	m_currentScan = 1;

	IupSetAttribute(m_hButtonFirstScan, "TITLE", "New Scan");

	PerformScan(true);

	IupSetAttribute(m_hComboValueType, "ACTIVE", "NO");
	IupSetAttribute(m_hButtonNextScan, "ACTIVE", "YES");

	return 0;
}

int csMain::NextScan()
{
	m_currentScan++;

	PerformScan(false);

	return 0;
}

void csMain::ResultClicked(char* text, int item, int state)
{
	int index = item - 1;

	if (index == -1 || state == 0) {
		return;
	}

	SearchResult &result = m_scanner.m_results[index];

	GuiDumpAt(result.m_base + result.m_offset);
}

int csMain::ScanValueTypeChanged()
{
	m_scanner.m_currentScanValueType = (SearchValueType)IupGetInt(m_hComboValueType, "VALUE");
	m_scanner.m_currentScanValueMethod = MethodForType(m_scanner.m_currentScanValueType);

	IupSetAttribute(m_hCheckHex, "ACTIVE", m_scanner.m_currentScanValueMethod == SVM_Integer ? "YES" : "NO");
	IupSetAttribute(m_hFloatMethod, "ACTIVE", m_scanner.m_currentScanValueMethod == SVM_Float ? "YES" : "NO");

	return 0;
}

void csMain::Open()
{
	if (m_hDialog != nullptr) {
		return;
	}

	m_hButtonFirstScan = IupButton("First Scan", "ScanFirst");
	m_hButtonNextScan = IupButton("Next Scan", "ScanNext");
	Ihandle* hButtons = IupSetAttributes(IupHbox(m_hButtonFirstScan, m_hButtonNextScan, nullptr), "MARGIN=0x0, GAP=5");

	IupSetAttribute(m_hButtonNextScan, "ACTIVE", "NO");

	CLAW_SETCALLBACK(m_hButtonFirstScan, "ACTION", FirstScan);
	CLAW_SETCALLBACK(m_hButtonNextScan, "ACTION", NextScan);

	m_hCheckHex = IupToggle("Hex", nullptr);
	m_hTextInput = IupSetAttributes(IupText(nullptr), "EXPAND=HORIZONTAL");
	Ihandle* hInput = IupSetAttributes(IupHbox(m_hCheckHex, m_hTextInput, nullptr), "MARGIN=0x0, GAP=5");

	m_hComboValueType = IupList(nullptr);
	IupSetAttribute(m_hComboValueType, "DROPDOWN", "YES");
	IupSetAttribute(m_hComboValueType, "1", "Byte");
	IupSetAttribute(m_hComboValueType, "2", "2 Bytes");
	IupSetAttribute(m_hComboValueType, "3", "4 Bytes");
	IupSetAttribute(m_hComboValueType, "4", "8 Bytes");
	IupSetAttribute(m_hComboValueType, "5", "Float");
	IupSetAttribute(m_hComboValueType, "6", "Double");
	IupSetAttribute(m_hComboValueType, "VALUE", "3");
	Ihandle* hValueType = IupSetAttributes(IupHbox(m_hComboValueType, nullptr), "VISIBLEITEMS=10, EXPAND=YES, MARGIN=0x0, GAP=5");
	CLAW_SETCALLBACK(m_hComboValueType, "ACTION", ScanValueTypeChanged);

	m_hCheckFastScan = IupToggle("Fast Scan", nullptr);
	IupSetAttribute(m_hCheckFastScan, "VALUE", "ON");

	m_hTextFastScanAlign = IupText(nullptr);
	IupSetAttribute(m_hTextFastScanAlign, "VALUE", "4");

	Ihandle* radioFloatTruncated = IupToggle("Truncated", nullptr);
	Ihandle* radioFloatRounded = IupToggle("Rounded", nullptr);
	Ihandle* radioFloatRoundedExtreme = IupToggle("Rounded (Extreme)", nullptr);

	IupSetHandle("trunc", radioFloatTruncated);
	IupSetHandle("round", radioFloatRounded);
	IupSetHandle("round2", radioFloatRoundedExtreme);

	m_hFloatMethod = IupRadio(IupSetAttributes(IupVbox(radioFloatTruncated, radioFloatRounded, radioFloatRoundedExtreme, nullptr), "MARGIN=0x0, GAP=5"));
	IupSetAttribute(m_hFloatMethod, "ACTIVE", "NO");

	Ihandle* hFastScan = IupSetAttributes(IupHbox(m_hCheckFastScan, m_hTextFastScanAlign, nullptr), "MARGIN=0x0, GAP=5");

	m_hCheckPauseWhileScanning = IupToggle("Pause while scanning", nullptr);

	m_hFrameScanOptions = IupFrame(IupVbox(
		m_hFloatMethod,
		hFastScan,
		m_hCheckPauseWhileScanning,
		nullptr)
	);
	IupSetAttribute(m_hFrameScanOptions, "TITLE", "Scan Options");
	IupSetAttribute(m_hFrameScanOptions, "EXPAND", "YES");

	Ihandle* vControls = IupSetAttributes(IupVbox(
		hButtons,
		hInput,
		hValueType,
		m_hFrameScanOptions,
		nullptr), "MARGIN=10x0, GAP=5, EXPAND=HORIZONTAL");

	m_hListResults = IupList(nullptr);
	IupSetAttribute(m_hListResults, "FONT", "Consolas, 9");
	IupSetAttribute(m_hListResults, "EXPAND", "YES");
	IupSetAttribute(m_hListResults, "1", nullptr);
	IupSetCallback(m_hListResults, "ACTION", (Icallback)_claw_ResultClicked);

	m_hDialog = IupDialog(IupSetAttributes(IupHbox(m_hListResults, vControls, nullptr), "PADDING=4x4, MARGIN=10x10"));

	IupSetAttribute(m_hDialog, "TITLE", "ClawSearch");
	IupSetAttribute(m_hDialog, "SIZE", "500x200");

	IupSetAttribute(m_hDialog, "NATIVEPARENT", (char*)GuiGetWindowHandle());
	CLAW_SETCALLBACK(m_hDialog, "CLOSE_CB", SearchWindowClosing);
	IupShowXY(m_hDialog, IUP_CENTERPARENT, IUP_CENTERPARENT);
}

void csMain::Close()
{
	if (m_hDialog == nullptr) {
		return;
	}
	IupDestroy(m_hDialog);
	m_hDialog = nullptr;
}

csMain* _csMain = nullptr;

void OpenSearch()
{
	if (_csMain == nullptr) {
		_csMain = new csMain;
	}

	_csMain->Open();
}

void CloseSearch()
{
	if (_csMain == nullptr) {
		return;
	}

	_csMain->Close();
	delete _csMain;
}
