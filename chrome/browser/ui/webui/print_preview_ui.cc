// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview_ui.h"

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview_data_source.h"
#include "chrome/browser/ui/webui/print_preview_handler.h"
#include "chrome/common/print_messages.h"
#include "content/browser/tab_contents/tab_contents.h"

PrintPreviewUI::PrintPreviewUI(TabContents* contents)
    : ChromeWebUI(contents),
      initial_preview_start_time_(base::TimeTicks::Now()),
      request_count_(0U),
      document_cookie_(0) {
  // WebUI owns |handler_|.
  handler_ = new PrintPreviewHandler();
  AddMessageHandler(handler_->Attach(this));

  // Set up the chrome://print/ data source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(
      new PrintPreviewDataSource());

  // Store the PrintPreviewUIAddress as a string.
  // "0x" + deadc0de + '\0' = 2 + 2 * sizeof(this) + 1;
  char preview_ui_addr[2 + (2 * sizeof(this)) + 1];
  base::snprintf(preview_ui_addr, sizeof(preview_ui_addr), "%p", this);
  preview_ui_addr_str_ = preview_ui_addr;
}

PrintPreviewUI::~PrintPreviewUI() {
  print_preview_data_service()->RemoveEntry(preview_ui_addr_str_);
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

void PrintPreviewUI::SetInitiatorTabURLAndTitle(
    const std::string& initiator_url,
    const string16& job_title) {
  initiator_url_ = initiator_url;
  initiator_tab_title_ = job_title;
}

void PrintPreviewUI::OnInitiatorTabCrashed() {
  StringValue initiator_tab_url(initiator_url_);
  CallJavascriptFunction("onInitiatorTabCrashed", initiator_tab_url);
}

void PrintPreviewUI::OnInitiatorTabClosed() {
  StringValue initiator_tab_url(initiator_url_);
  CallJavascriptFunction("onInitiatorTabClosed", initiator_tab_url);
}

void PrintPreviewUI::OnPrintPreviewRequest() {
  request_count_++;
}

void PrintPreviewUI::OnDidGetPreviewPageCount(
    const PrintHostMsg_DidGetPreviewPageCount_Params& params) {
  DCHECK_GT(params.page_count, 0);
  document_cookie_ = params.document_cookie;
  base::FundamentalValue count(params.page_count);
  base::FundamentalValue modifiable(params.is_modifiable);
  base::FundamentalValue request_id(params.preview_request_id);
  StringValue title(initiator_tab_title_);
  CallJavascriptFunction("onDidGetPreviewPageCount", count, modifiable,
                         request_id, title);
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
  DecrementRequestCount();

  base::StringValue ui_identifier(preview_ui_addr_str_);
  base::FundamentalValue ui_preview_request_id(preview_request_id);
  CallJavascriptFunction("reloadPreviewPages", ui_identifier,
                         ui_preview_request_id);
}

void PrintPreviewUI::OnPreviewDataIsAvailable(int expected_pages_count,
                                              int preview_request_id) {
  VLOG(1) << "Print preview request finished with "
          << expected_pages_count << " pages";
  DecrementRequestCount();

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

void PrintPreviewUI::OnPrintPreviewFailed() {
  DecrementRequestCount();
  CallJavascriptFunction("printPreviewFailed");
}

void PrintPreviewUI::OnPrintPreviewCancelled() {
  DecrementRequestCount();
}

bool PrintPreviewUI::HasPendingRequests() {
  return request_count_ > 1;
}

PrintPreviewDataService* PrintPreviewUI::print_preview_data_service() {
  return PrintPreviewDataService::GetInstance();
}

void PrintPreviewUI::DecrementRequestCount() {
  if (request_count_ > 0)
    request_count_--;
}

int PrintPreviewUI::document_cookie() {
  return document_cookie_;
}
