// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"

#include <map>

#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_data_source.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/print_messages.h"
#include "content/public/browser/web_contents.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"
#include "ui/gfx/rect.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::WebContents;
using printing::PageSizeMargins;

namespace {

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

}  // namespace

PrintPreviewUI::PrintPreviewUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui),
      initial_preview_start_time_(base::TimeTicks::Now()),
      id_(g_print_preview_ui_id_map.Get().Add(this)),
      handler_(NULL),
      source_is_modifiable_(true),
      tab_closed_(false) {
  // Set up the chrome://print/ data source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, new PrintPreviewDataSource());

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
void PrintPreviewUI::SetSourceIsModifiable(WebContents* print_preview_tab,
                                           bool source_is_modifiable) {
  if (!print_preview_tab || !print_preview_tab->GetWebUI())
    return;
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      print_preview_tab->GetWebUI()->GetController());
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

void PrintPreviewUI::OnPrintPreviewTabClosed() {
  WebContents* preview_tab = web_ui()->GetWebContents();
  printing::BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewTab(preview_tab))
    return;
  OnClosePrintPreviewTab();
}

void PrintPreviewUI::OnInitiatorTabClosed() {
  WebContents* preview_tab = web_ui()->GetWebContents();
  printing::BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewTab(preview_tab))
    web_ui()->CallJavascriptFunction("cancelPendingPrintRequest");
  else
    OnClosePrintPreviewTab();
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

void PrintPreviewUI::OnTabDestroyed() {
  handler_->OnTabDestroyed();
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

void PrintPreviewUI::OnHidePreviewTab() {
  WebContents* preview_tab = web_ui()->GetWebContents();
  printing::BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewTab(preview_tab))
    return;

  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;
  delegate->ReleaseWebContentsOnDialogClose();
  background_printing_manager->OwnPrintPreviewTab(preview_tab);
  OnClosePrintPreviewTab();
}

void PrintPreviewUI::OnClosePrintPreviewTab() {
  if (tab_closed_)
    return;
  tab_closed_ = true;
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
