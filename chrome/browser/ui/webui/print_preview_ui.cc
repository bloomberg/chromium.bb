// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview_ui.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview_data_source.h"
#include "chrome/browser/ui/webui/print_preview_handler.h"
#include "chrome/common/print_messages.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"

using printing::PageSizeMargins;

namespace {

// Thread-safe wrapper around a std::map to keep track of mappings from
// PrintPreviewUI addresses to most recent print preview request ids.
class PrintPreviewRequestIdMapWithLock {
 public:
  PrintPreviewRequestIdMapWithLock() {}
  ~PrintPreviewRequestIdMapWithLock() {}

  // Get the value for |addr|. Returns true and sets |out_value| on success.
  bool Get(const std::string& addr, int* out_value) {
    base::AutoLock lock(lock_);
    PrintPreviewRequestIdMap::const_iterator it = map_.find(addr);
    if (it == map_.end())
      return false;
    *out_value = it->second;
    return true;
  }

  // Sets the |value| for |addr|.
  void Set(const std::string& addr, int value) {
    base::AutoLock lock(lock_);
    map_[addr] = value;
  }

  // Erase the entry for |addr|.
  void Erase(const std::string& addr) {
    base::AutoLock lock(lock_);
    map_.erase(addr);
  }

 private:
  typedef std::map<std::string, int> PrintPreviewRequestIdMap;

  PrintPreviewRequestIdMap map_;
  base::Lock lock_;
};

// Written to on the UI thread, read from any thread.
base::LazyInstance<PrintPreviewRequestIdMapWithLock>
    g_print_preview_request_id_map(base::LINKER_INITIALIZED);

}  // namespace

PrintPreviewUI::PrintPreviewUI(TabContents* contents)
    : ChromeWebUI(contents),
      initial_preview_start_time_(base::TimeTicks::Now()) {
  // WebUI owns |handler_|.
  handler_ = new PrintPreviewHandler();
  AddMessageHandler(handler_->Attach(this));

  // Set up the chrome://print/ data source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(
      new PrintPreviewDataSource());

  preview_ui_addr_str_ = GetPrintPreviewUIAddress();

  g_print_preview_request_id_map.Get().Set(preview_ui_addr_str_, -1);
}

PrintPreviewUI::~PrintPreviewUI() {
  print_preview_data_service()->RemoveEntry(preview_ui_addr_str_);

  g_print_preview_request_id_map.Get().Erase(preview_ui_addr_str_);
}

void PrintPreviewUI::GetPrintPreviewDataForIndex(
    int index,
    scoped_refptr<RefCountedBytes>* data) {
  print_preview_data_service()->GetDataEntry(preview_ui_addr_str_, index, data);
}

void PrintPreviewUI::SetPrintPreviewDataForIndex(int index,
                                                 const RefCountedBytes* data) {
  print_preview_data_service()->SetDataEntry(preview_ui_addr_str_, index, data);
}

void PrintPreviewUI::ClearAllPreviewData() {
  print_preview_data_service()->RemoveEntry(preview_ui_addr_str_);
}

int PrintPreviewUI::GetAvailableDraftPageCount() {
  return print_preview_data_service()->GetAvailableDraftPageCount(
      preview_ui_addr_str_);
}

void PrintPreviewUI::SetInitiatorTabURLAndTitle(
    const std::string& initiator_url,
    const string16& job_title) {
  initiator_url_ = initiator_url;
  initiator_tab_title_ = job_title;
}

// static
void PrintPreviewUI::GetCurrentPrintPreviewStatus(
    const std::string& preview_ui_addr,
    int request_id,
    bool* cancel) {
  int current_id = -1;
  if (!g_print_preview_request_id_map.Get().Get(preview_ui_addr, &current_id)) {
    *cancel = true;
    return;
  }
  *cancel = (request_id != current_id);
}

std::string PrintPreviewUI::GetPrintPreviewUIAddress() const {
  // Store the PrintPreviewUIAddress as a string.
  // "0x" + deadc0de + '\0' = 2 + 2 * sizeof(this) + 1;
  char preview_ui_addr[2 + (2 * sizeof(this)) + 1];
  base::snprintf(preview_ui_addr, sizeof(preview_ui_addr), "%p", this);
  return preview_ui_addr;
}

void PrintPreviewUI::OnInitiatorTabCrashed() {
  StringValue initiator_tab_url(initiator_url_);
  CallJavascriptFunction("onInitiatorTabCrashed", initiator_tab_url);
}

void PrintPreviewUI::OnInitiatorTabClosed() {
  StringValue initiator_tab_url(initiator_url_);
  CallJavascriptFunction("onInitiatorTabClosed", initiator_tab_url);
}

void PrintPreviewUI::OnPrintPreviewRequest(int request_id) {
  g_print_preview_request_id_map.Get().Set(preview_ui_addr_str_, request_id);
}

void PrintPreviewUI::OnShowSystemDialog() {
  CallJavascriptFunction("onSystemDialogLinkClicked");
}

void PrintPreviewUI::OnDidGetPreviewPageCount(
    const PrintHostMsg_DidGetPreviewPageCount_Params& params) {
  DCHECK_GT(params.page_count, 0);
  base::FundamentalValue count(params.page_count);
  base::FundamentalValue modifiable(params.is_modifiable);
  base::FundamentalValue request_id(params.preview_request_id);
  StringValue title(initiator_tab_title_);
  CallJavascriptFunction("onDidGetPreviewPageCount", count, modifiable,
                         request_id, title);
}

void PrintPreviewUI::OnDidGetDefaultPageLayout(
    const PageSizeMargins& page_layout) {
  if (page_layout.margin_top < 0 || page_layout.margin_left < 0 ||
      page_layout.margin_bottom < 0 || page_layout.margin_right < 0 ||
      page_layout.content_width < 0 || page_layout.content_height < 0) {
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

  CallJavascriptFunction("onDidGetDefaultPageLayout", layout);
}

void PrintPreviewUI::OnDidPreviewPage(int page_number,
                                      int preview_request_id) {
  DCHECK_GE(page_number, 0);
  base::FundamentalValue number(page_number);
  StringValue ui_identifier(preview_ui_addr_str_);
  base::FundamentalValue request_id(preview_request_id);
  CallJavascriptFunction("onDidPreviewPage", number, ui_identifier, request_id);
}

void PrintPreviewUI::OnReusePreviewData(int preview_request_id) {
  base::StringValue ui_identifier(preview_ui_addr_str_);
  base::FundamentalValue ui_preview_request_id(preview_request_id);
  CallJavascriptFunction("reloadPreviewPages", ui_identifier,
                         ui_preview_request_id);
}

void PrintPreviewUI::OnPreviewDataIsAvailable(int expected_pages_count,
                                              int preview_request_id) {
  VLOG(1) << "Print preview request finished with "
          << expected_pages_count << " pages";

  if (!initial_preview_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PrintPreview.InitalDisplayTime",
                        base::TimeTicks::Now() - initial_preview_start_time_);
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.Initial",
                         expected_pages_count);
    initial_preview_start_time_ = base::TimeTicks();
  }
  base::StringValue ui_identifier(preview_ui_addr_str_);
  base::FundamentalValue ui_preview_request_id(preview_request_id);
  CallJavascriptFunction("updatePrintPreview", ui_identifier,
                         ui_preview_request_id);
}

void PrintPreviewUI::OnTabDestroyed() {
  handler_->OnTabDestroyed();
}

void PrintPreviewUI::OnFileSelectionCancelled() {
  CallJavascriptFunction("fileSelectionCancelled");
}

void PrintPreviewUI::OnCancelPendingPreviewRequest() {
  g_print_preview_request_id_map.Get().Set(preview_ui_addr_str_, -1);
}

void PrintPreviewUI::OnPrintPreviewFailed() {
  handler_->OnPrintPreviewFailed();
  CallJavascriptFunction("printPreviewFailed");
}

PrintPreviewDataService* PrintPreviewUI::print_preview_data_service() {
  return PrintPreviewDataService::GetInstance();
}
