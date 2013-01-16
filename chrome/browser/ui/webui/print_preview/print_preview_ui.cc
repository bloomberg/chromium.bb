// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"

#include <map>

#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/print_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::WebContents;
using printing::PageSizeMargins;

namespace {

#if defined(OS_MACOSX)
// U+0028 U+21E7 U+2318 U+0050 U+0029 in UTF8
const char kAdvancedPrintShortcut[] = "\x28\xE2\x8c\xA5\xE2\x8C\x98\x50\x29";
#elif defined(OS_WIN)
const char kAdvancedPrintShortcut[] = "(Ctrl+Shift+P)";
#else
const char kAdvancedPrintShortcut[] = "(Shift+Ctrl+P)";
#endif

// Thread-safe wrapper around a std::map to keep track of mappings from
// PrintPreviewUI IDs to most recent print preview request IDs.
class PrintPreviewRequestIdMapWithLock {
 public:
  PrintPreviewRequestIdMapWithLock() {}
  ~PrintPreviewRequestIdMapWithLock() {}

  // Gets the value for |preview_id|.
  // Returns true and sets |out_value| on success.
  bool Get(int32 preview_id, int* out_value) {
    base::AutoLock lock(lock_);
    PrintPreviewRequestIdMap::const_iterator it = map_.find(preview_id);
    if (it == map_.end())
      return false;
    *out_value = it->second;
    return true;
  }

  // Sets the |value| for |preview_id|.
  void Set(int32 preview_id, int value) {
    base::AutoLock lock(lock_);
    map_[preview_id] = value;
  }

  // Erases the entry for |preview_id|.
  void Erase(int32 preview_id) {
    base::AutoLock lock(lock_);
    map_.erase(preview_id);
  }

 private:
  // Mapping from PrintPreviewUI ID to print preview request ID.
  typedef std::map<int, int> PrintPreviewRequestIdMap;

  PrintPreviewRequestIdMap map_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewRequestIdMapWithLock);
};

// Written to on the UI thread, read from any thread.
base::LazyInstance<PrintPreviewRequestIdMapWithLock>
    g_print_preview_request_id_map = LAZY_INSTANCE_INITIALIZER;

// PrintPreviewUI IDMap used to avoid exposing raw pointer addresses to WebUI.
// Only accessed on the UI thread.
base::LazyInstance<IDMap<PrintPreviewUI> >
    g_print_preview_ui_id_map = LAZY_INSTANCE_INITIALIZER;

// PrintPreviewUI serves data for chrome://print requests.
//
// The format for requesting PDF data is as follows:
// chrome://print/<PrintPreviewUIID>/<PageIndex>/print.pdf
//
// Parameters (< > required):
//    <PrintPreviewUIID> = PrintPreview UI ID
//    <PageIndex> = Page index is zero-based or
//                  |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to represent
//                  a print ready PDF.
//
// Example:
//    chrome://print/123/10/print.pdf
//
// Requests to chrome://print with paths not ending in /print.pdf are used
// to return the markup or other resources for the print preview page itself.
bool HandleRequestCallback(
    const std::string& path,
    const ChromeWebUIDataSource::GotDataCallback& callback) {
  // ChromeWebUIDataSource handles most requests except for the print preview
  // data.
  if (!EndsWith(path, "/print.pdf", true))
    return false;

  // Print Preview data.
  scoped_refptr<base::RefCountedBytes> data;
  std::vector<std::string> url_substr;
  base::SplitString(path, '/', &url_substr);
  int preview_ui_id = -1;
  int page_index = 0;
  if (url_substr.size() == 3 &&
      base::StringToInt(url_substr[0], &preview_ui_id),
      base::StringToInt(url_substr[1], &page_index) &&
      preview_ui_id >= 0) {
    PrintPreviewDataService::GetInstance()->GetDataEntry(
        preview_ui_id, page_index, &data);
  }
  if (data.get()) {
    callback.Run(data);
    return true;
  }
  // Invalid request.
  scoped_refptr<base::RefCountedBytes> empty_bytes(new base::RefCountedBytes);
  callback.Run(empty_bytes);
  return true;
}

ChromeWebUIDataSource* CreatePrintPreviewUISource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIPrintHost);
#if defined(OS_CHROMEOS)
  source->AddLocalizedString("title",
                             IDS_PRINT_PREVIEW_GOOGLE_CLOUD_PRINT_TITLE);
#else
  source->AddLocalizedString("title", IDS_PRINT_PREVIEW_TITLE);
#endif
  source->AddLocalizedString("loading", IDS_PRINT_PREVIEW_LOADING);
  source->AddLocalizedString("noPlugin", IDS_PRINT_PREVIEW_NO_PLUGIN);
  source->AddLocalizedString("launchNativeDialog",
                             IDS_PRINT_PREVIEW_NATIVE_DIALOG);
  source->AddLocalizedString("previewFailed", IDS_PRINT_PREVIEW_FAILED);
  source->AddLocalizedString("invalidPrinterSettings",
                             IDS_PRINT_PREVIEW_INVALID_PRINTER_SETTINGS);
  source->AddLocalizedString("printButton", IDS_PRINT_PREVIEW_PRINT_BUTTON);
  source->AddLocalizedString("saveButton", IDS_PRINT_PREVIEW_SAVE_BUTTON);
  source->AddLocalizedString("cancelButton", IDS_PRINT_PREVIEW_CANCEL_BUTTON);
  source->AddLocalizedString("printing", IDS_PRINT_PREVIEW_PRINTING);
  source->AddLocalizedString("printingToPDFInProgress",
                             IDS_PRINT_PREVIEW_PRINTING_TO_PDF_IN_PROGRESS);
#if defined(OS_MACOSX)
  source->AddLocalizedString("openingPDFInPreview",
                             IDS_PRINT_PREVIEW_OPENING_PDF_IN_PREVIEW);
#endif
  source->AddLocalizedString("destinationLabel",
                             IDS_PRINT_PREVIEW_DESTINATION_LABEL);
  source->AddLocalizedString("copiesLabel", IDS_PRINT_PREVIEW_COPIES_LABEL);
  source->AddLocalizedString("examplePageRangeText",
                             IDS_PRINT_PREVIEW_EXAMPLE_PAGE_RANGE_TEXT);
  source->AddLocalizedString("layoutLabel", IDS_PRINT_PREVIEW_LAYOUT_LABEL);
  source->AddLocalizedString("optionAllPages",
                             IDS_PRINT_PREVIEW_OPTION_ALL_PAGES);
  source->AddLocalizedString("optionBw", IDS_PRINT_PREVIEW_OPTION_BW);
  source->AddLocalizedString("optionCollate", IDS_PRINT_PREVIEW_OPTION_COLLATE);
  source->AddLocalizedString("optionColor", IDS_PRINT_PREVIEW_OPTION_COLOR);
  source->AddLocalizedString("optionLandscape",
                             IDS_PRINT_PREVIEW_OPTION_LANDSCAPE);
  source->AddLocalizedString("optionPortrait",
                             IDS_PRINT_PREVIEW_OPTION_PORTRAIT);
  source->AddLocalizedString("optionTwoSided",
                             IDS_PRINT_PREVIEW_OPTION_TWO_SIDED);
  source->AddLocalizedString("pagesLabel", IDS_PRINT_PREVIEW_PAGES_LABEL);
  source->AddLocalizedString("pageRangeTextBox",
                             IDS_PRINT_PREVIEW_PAGE_RANGE_TEXT);
  source->AddLocalizedString("pageRangeRadio",
                             IDS_PRINT_PREVIEW_PAGE_RANGE_RADIO);
  source->AddLocalizedString("printToPDF", IDS_PRINT_PREVIEW_PRINT_TO_PDF);
  source->AddLocalizedString("printPreviewSummaryFormatShort",
                             IDS_PRINT_PREVIEW_SUMMARY_FORMAT_SHORT);
  source->AddLocalizedString("printPreviewSummaryFormatLong",
                             IDS_PRINT_PREVIEW_SUMMARY_FORMAT_LONG);
  source->AddLocalizedString("printPreviewSheetsLabelSingular",
                             IDS_PRINT_PREVIEW_SHEETS_LABEL_SINGULAR);
  source->AddLocalizedString("printPreviewSheetsLabelPlural",
                             IDS_PRINT_PREVIEW_SHEETS_LABEL_PLURAL);
  source->AddLocalizedString("printPreviewPageLabelSingular",
                             IDS_PRINT_PREVIEW_PAGE_LABEL_SINGULAR);
  source->AddLocalizedString("printPreviewPageLabelPlural",
                             IDS_PRINT_PREVIEW_PAGE_LABEL_PLURAL);
  const string16 shortcut_text(UTF8ToUTF16(kAdvancedPrintShortcut));
#if defined(OS_CHROMEOS)
  source->AddString(
      "systemDialogOption",
      l10n_util::GetStringFUTF16(
          IDS_PRINT_PREVIEW_CLOUD_DIALOG_OPTION,
          l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT),
          shortcut_text));
#else
  source->AddString(
      "systemDialogOption",
      l10n_util::GetStringFUTF16(
          IDS_PRINT_PREVIEW_SYSTEM_DIALOG_OPTION,
          shortcut_text));
#endif
  source->AddString(
      "cloudPrintDialogOption",
      l10n_util::GetStringFUTF16(
          IDS_PRINT_PREVIEW_CLOUD_DIALOG_OPTION_NO_SHORTCUT,
          l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
#if defined(OS_MACOSX)
  source->AddLocalizedString("openPdfInPreviewOption",
                             IDS_PRINT_PREVIEW_OPEN_PDF_IN_PREVIEW_APP);
#endif
  source->AddString(
      "printWithCloudPrintWait",
      l10n_util::GetStringFUTF16(
        IDS_PRINT_PREVIEW_PRINT_WITH_CLOUD_PRINT_WAIT,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
  source->AddLocalizedString("pageRangeInstruction",
                             IDS_PRINT_PREVIEW_PAGE_RANGE_INSTRUCTION);
  source->AddLocalizedString("copiesInstruction",
                             IDS_PRINT_PREVIEW_COPIES_INSTRUCTION);
  source->AddLocalizedString("incrementTitle",
                             IDS_PRINT_PREVIEW_INCREMENT_TITLE);
  source->AddLocalizedString("decrementTitle",
                             IDS_PRINT_PREVIEW_DECREMENT_TITLE);
  source->AddLocalizedString("printPagesLabel",
                             IDS_PRINT_PREVIEW_PRINT_PAGES_LABEL);
  source->AddLocalizedString("optionsLabel", IDS_PRINT_PREVIEW_OPTIONS_LABEL);
  source->AddLocalizedString("optionHeaderFooter",
                             IDS_PRINT_PREVIEW_OPTION_HEADER_FOOTER);
  source->AddLocalizedString("optionFitToPage",
                             IDS_PRINT_PREVIEW_OPTION_FIT_TO_PAGE);
  source->AddLocalizedString(
      "optionBackgroundColorsAndImages",
      IDS_PRINT_PREVIEW_OPTION_BACKGROUND_COLORS_AND_IMAGES);
  source->AddLocalizedString("marginsLabel", IDS_PRINT_PREVIEW_MARGINS_LABEL);
  source->AddLocalizedString("defaultMargins",
                             IDS_PRINT_PREVIEW_DEFAULT_MARGINS);
  source->AddLocalizedString("noMargins", IDS_PRINT_PREVIEW_NO_MARGINS);
  source->AddLocalizedString("customMargins", IDS_PRINT_PREVIEW_CUSTOM_MARGINS);
  source->AddLocalizedString("minimumMargins",
                             IDS_PRINT_PREVIEW_MINIMUM_MARGINS);
  source->AddLocalizedString("top", IDS_PRINT_PREVIEW_TOP_MARGIN_LABEL);
  source->AddLocalizedString("bottom", IDS_PRINT_PREVIEW_BOTTOM_MARGIN_LABEL);
  source->AddLocalizedString("left", IDS_PRINT_PREVIEW_LEFT_MARGIN_LABEL);
  source->AddLocalizedString("right", IDS_PRINT_PREVIEW_RIGHT_MARGIN_LABEL);
  source->AddLocalizedString("destinationSearchTitle",
                             IDS_PRINT_PREVIEW_DESTINATION_SEARCH_TITLE);
  source->AddLocalizedString("userInfo", IDS_PRINT_PREVIEW_USER_INFO);
  source->AddLocalizedString("cloudPrintPromotion",
                             IDS_PRINT_PREVIEW_CLOUD_PRINT_PROMOTION);
  source->AddLocalizedString("searchBoxPlaceholder",
                             IDS_PRINT_PREVIEW_SEARCH_BOX_PLACEHOLDER);
  source->AddLocalizedString("noDestinationsMessage",
                             IDS_PRINT_PREVIEW_NO_DESTINATIONS_MESSAGE);
  source->AddLocalizedString("showAllButtonText",
                             IDS_PRINT_PREVIEW_SHOW_ALL_BUTTON_TEXT);
  source->AddLocalizedString("destinationCount",
                             IDS_PRINT_PREVIEW_DESTINATION_COUNT);
  source->AddLocalizedString("recentDestinationsTitle",
                             IDS_PRINT_PREVIEW_RECENT_DESTINATIONS_TITLE);
  source->AddLocalizedString("localDestinationsTitle",
                             IDS_PRINT_PREVIEW_LOCAL_DESTINATIONS_TITLE);
  source->AddLocalizedString("cloudDestinationsTitle",
                             IDS_PRINT_PREVIEW_CLOUD_DESTINATIONS_TITLE);
  source->AddLocalizedString("manage", IDS_PRINT_PREVIEW_MANAGE);
  source->AddLocalizedString("setupCloudPrinters",
                             IDS_PRINT_PREVIEW_SETUP_CLOUD_PRINTERS);
  source->AddLocalizedString("changeDestination",
                             IDS_PRINT_PREVIEW_CHANGE_DESTINATION);
  source->AddLocalizedString("offlineForYear",
                             IDS_PRINT_PREVIEW_OFFLINE_FOR_YEAR);
  source->AddLocalizedString("offlineForMonth",
                             IDS_PRINT_PREVIEW_OFFLINE_FOR_MONTH);
  source->AddLocalizedString("offlineForWeek",
                             IDS_PRINT_PREVIEW_OFFLINE_FOR_WEEK);
  source->AddLocalizedString("offline", IDS_PRINT_PREVIEW_OFFLINE);
  source->AddLocalizedString("fedexTos", IDS_PRINT_PREVIEW_FEDEX_TOS);
  source->AddLocalizedString("tosCheckboxLabel",
                             IDS_PRINT_PREVIEW_TOS_CHECKBOX_LABEL);
  source->AddLocalizedString("noDestsPromoTitle",
                             IDS_PRINT_PREVIEW_NO_DESTS_PROMO_TITLE);
  source->AddLocalizedString("noDestsPromoBody",
                             IDS_PRINT_PREVIEW_NO_DESTS_PROMO_BODY);
  source->AddLocalizedString("noDestsPromoGcpDesc",
                             IDS_PRINT_PREVIEW_NO_DESTS_GCP_DESC);
  source->AddLocalizedString(
      "noDestsPromoAddPrinterButtonLabel",
      IDS_PRINT_PREVIEW_NO_DESTS_PROMO_ADD_PRINTER_BUTTON_LABEL);
  source->AddLocalizedString(
      "noDestsPromoNotNowButtonLabel",
      IDS_PRINT_PREVIEW_NO_DESTS_PROMO_NOT_NOW_BUTTON_LABEL);

  source->set_json_path("strings.js");
  source->add_resource_path("print_preview.js", IDR_PRINT_PREVIEW_JS);
  source->add_resource_path("images/printer.png",
                            IDR_PRINT_PREVIEW_IMAGES_PRINTER);
  source->add_resource_path("images/printer_shared.png",
                            IDR_PRINT_PREVIEW_IMAGES_PRINTER_SHARED);
  source->add_resource_path("images/third_party.png",
                            IDR_PRINT_PREVIEW_IMAGES_THIRD_PARTY);
  source->add_resource_path("images/third_party_fedex.png",
                            IDR_PRINT_PREVIEW_IMAGES_THIRD_PARTY_FEDEX);
  source->add_resource_path("images/google_doc.png",
                            IDR_PRINT_PREVIEW_IMAGES_GOOGLE_DOC);
  source->add_resource_path("images/pdf.png", IDR_PRINT_PREVIEW_IMAGES_PDF);
  source->add_resource_path("images/mobile.png",
                            IDR_PRINT_PREVIEW_IMAGES_MOBILE);
  source->add_resource_path("images/mobile_shared.png",
                            IDR_PRINT_PREVIEW_IMAGES_MOBILE_SHARED);
  source->set_default_resource(IDR_PRINT_PREVIEW_HTML);
  source->SetRequestFilter(base::Bind(&HandleRequestCallback));
  return source;
}

}  // namespace

PrintPreviewUI::PrintPreviewUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui),
      initial_preview_start_time_(base::TimeTicks::Now()),
      id_(g_print_preview_ui_id_map.Get().Add(this)),
      handler_(NULL),
      source_is_modifiable_(true),
      dialog_closed_(false) {
  // Set up the chrome://print/ data source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(profile,
                                          CreatePrintPreviewUISource());

  // Set up the chrome://theme/ source.
  ChromeURLDataManager::AddDataSource(profile, new ThemeSource(profile));

  // WebUI owns |handler_|.
  handler_ = new PrintPreviewHandler();
  web_ui->AddMessageHandler(handler_);

  g_print_preview_request_id_map.Get().Set(id_, -1);
}

PrintPreviewUI::~PrintPreviewUI() {
  print_preview_data_service()->RemoveEntry(id_);
  g_print_preview_request_id_map.Get().Erase(id_);
  g_print_preview_ui_id_map.Get().Remove(id_);
}

void PrintPreviewUI::GetPrintPreviewDataForIndex(
    int index,
    scoped_refptr<base::RefCountedBytes>* data) {
  print_preview_data_service()->GetDataEntry(id_, index, data);
}

void PrintPreviewUI::SetPrintPreviewDataForIndex(
    int index,
    const base::RefCountedBytes* data) {
  print_preview_data_service()->SetDataEntry(id_, index, data);
}

void PrintPreviewUI::ClearAllPreviewData() {
  print_preview_data_service()->RemoveEntry(id_);
}

int PrintPreviewUI::GetAvailableDraftPageCount() {
  return print_preview_data_service()->GetAvailableDraftPageCount(id_);
}

void PrintPreviewUI::SetInitiatorTabURLAndTitle(
    const std::string& initiator_url,
    const string16& job_title) {
  initiator_url_ = initiator_url;
  initiator_tab_title_ = job_title;
}

// static
void PrintPreviewUI::SetSourceIsModifiable(WebContents* print_preview_dialog,
                                           bool source_is_modifiable) {
  if (!print_preview_dialog || !print_preview_dialog->GetWebUI())
    return;
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      print_preview_dialog->GetWebUI()->GetController());
  print_preview_ui->source_is_modifiable_ = source_is_modifiable;
}

// static
void PrintPreviewUI::GetCurrentPrintPreviewStatus(int32 preview_ui_id,
                                                  int request_id,
                                                  bool* cancel) {
  int current_id = -1;
  if (!g_print_preview_request_id_map.Get().Get(preview_ui_id, &current_id)) {
    *cancel = true;
    return;
  }
  *cancel = (request_id != current_id);
}

int32 PrintPreviewUI::GetIDForPrintPreviewUI() const {
  return id_;
}

void PrintPreviewUI::OnPrintPreviewDialogClosed() {
  WebContents* preview_dialog = web_ui()->GetWebContents();
  printing::BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewDialog(preview_dialog))
    return;
  OnClosePrintPreviewDialog();
}

void PrintPreviewUI::OnInitiatorTabClosed() {
  WebContents* preview_dialog = web_ui()->GetWebContents();
  printing::BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewDialog(preview_dialog))
    web_ui()->CallJavascriptFunction("cancelPendingPrintRequest");
  else
    OnClosePrintPreviewDialog();
}

void PrintPreviewUI::OnPrintPreviewRequest(int request_id) {
  g_print_preview_request_id_map.Get().Set(id_, request_id);
}

void PrintPreviewUI::OnShowSystemDialog() {
  web_ui()->CallJavascriptFunction("onSystemDialogLinkClicked");
}

void PrintPreviewUI::OnDidGetPreviewPageCount(
    const PrintHostMsg_DidGetPreviewPageCount_Params& params) {
  DCHECK_GT(params.page_count, 0);
  base::FundamentalValue count(params.page_count);
  base::FundamentalValue request_id(params.preview_request_id);
  web_ui()->CallJavascriptFunction("onDidGetPreviewPageCount",
                                   count,
                                   request_id);
}

void PrintPreviewUI::OnDidGetDefaultPageLayout(
    const PageSizeMargins& page_layout, const gfx::Rect& printable_area,
    bool has_custom_page_size_style) {
  if (page_layout.margin_top < 0 || page_layout.margin_left < 0 ||
      page_layout.margin_bottom < 0 || page_layout.margin_right < 0 ||
      page_layout.content_width < 0 || page_layout.content_height < 0 ||
      printable_area.width() <= 0 || printable_area.height() <= 0) {
    NOTREACHED();
    return;
  }

  base::DictionaryValue layout;
  layout.SetDouble(printing::kSettingMarginTop, page_layout.margin_top);
  layout.SetDouble(printing::kSettingMarginLeft, page_layout.margin_left);
  layout.SetDouble(printing::kSettingMarginBottom, page_layout.margin_bottom);
  layout.SetDouble(printing::kSettingMarginRight, page_layout.margin_right);
  layout.SetDouble(printing::kSettingContentWidth, page_layout.content_width);
  layout.SetDouble(printing::kSettingContentHeight, page_layout.content_height);
  layout.SetInteger(printing::kSettingPrintableAreaX, printable_area.x());
  layout.SetInteger(printing::kSettingPrintableAreaY, printable_area.y());
  layout.SetInteger(printing::kSettingPrintableAreaWidth,
                    printable_area.width());
  layout.SetInteger(printing::kSettingPrintableAreaHeight,
                    printable_area.height());

  base::FundamentalValue has_page_size_style(has_custom_page_size_style);
  web_ui()->CallJavascriptFunction("onDidGetDefaultPageLayout", layout,
                                   has_page_size_style);
}

void PrintPreviewUI::OnDidPreviewPage(int page_number,
                                      int preview_request_id) {
  DCHECK_GE(page_number, 0);
  base::FundamentalValue number(page_number);
  base::FundamentalValue ui_identifier(id_);
  base::FundamentalValue request_id(preview_request_id);
  web_ui()->CallJavascriptFunction(
      "onDidPreviewPage", number, ui_identifier, request_id);
}

void PrintPreviewUI::OnReusePreviewData(int preview_request_id) {
  base::FundamentalValue ui_identifier(id_);
  base::FundamentalValue ui_preview_request_id(preview_request_id);
  web_ui()->CallJavascriptFunction("reloadPreviewPages", ui_identifier,
                                   ui_preview_request_id);
}

void PrintPreviewUI::OnPreviewDataIsAvailable(int expected_pages_count,
                                              int preview_request_id) {
  VLOG(1) << "Print preview request finished with "
          << expected_pages_count << " pages";

  if (!initial_preview_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PrintPreview.InitialDisplayTime",
                        base::TimeTicks::Now() - initial_preview_start_time_);
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.Initial",
                         expected_pages_count);
    initial_preview_start_time_ = base::TimeTicks();
  }
  base::FundamentalValue ui_identifier(id_);
  base::FundamentalValue ui_preview_request_id(preview_request_id);
  web_ui()->CallJavascriptFunction("updatePrintPreview", ui_identifier,
                                   ui_preview_request_id);
}

void PrintPreviewUI::OnPrintPreviewDialogDestroyed() {
  handler_->OnPrintPreviewDialogDestroyed();
}

void PrintPreviewUI::OnFileSelectionCancelled() {
  web_ui()->CallJavascriptFunction("fileSelectionCancelled");
}

void PrintPreviewUI::OnCancelPendingPreviewRequest() {
  g_print_preview_request_id_map.Get().Set(id_, -1);
}

void PrintPreviewUI::OnPrintPreviewFailed() {
  handler_->OnPrintPreviewFailed();
  web_ui()->CallJavascriptFunction("printPreviewFailed");
}

void PrintPreviewUI::OnInvalidPrinterSettings() {
  web_ui()->CallJavascriptFunction("invalidPrinterSettings");
}

PrintPreviewDataService* PrintPreviewUI::print_preview_data_service() {
  return PrintPreviewDataService::GetInstance();
}

void PrintPreviewUI::OnHidePreviewDialog() {
  WebContents* preview_dialog = web_ui()->GetWebContents();
  printing::BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewDialog(preview_dialog))
    return;

  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;
  delegate->ReleaseWebContentsOnDialogClose();
  background_printing_manager->OwnPrintPreviewDialog(preview_dialog);
  OnClosePrintPreviewDialog();
}

void PrintPreviewUI::OnClosePrintPreviewDialog() {
  if (dialog_closed_)
    return;
  dialog_closed_ = true;
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;
  delegate->GetWebDialogDelegate()->OnDialogClosed("");
  delegate->OnDialogCloseFromWebUI();
}

void PrintPreviewUI::OnReloadPrintersList() {
  web_ui()->CallJavascriptFunction("reloadPrintersList");
}

void PrintPreviewUI::OnPrintPreviewScalingDisabled() {
  web_ui()->CallJavascriptFunction("printScalingDisabledForSourcePDF");
}
