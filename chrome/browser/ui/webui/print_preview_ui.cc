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
#include "content/browser/tab_contents/tab_contents.h"

PrintPreviewUI::PrintPreviewUI(TabContents* contents)
    : ChromeWebUI(contents),
      initial_preview_start_time_(base::TimeTicks::Now()) {
  // PrintPreviewUI owns |handler|.
  PrintPreviewHandler* handler = new PrintPreviewHandler();
  AddMessageHandler(handler->Attach(this));

  // Set up the chrome://print/ data source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(
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

void PrintPreviewUI::GetPrintPreviewData(scoped_refptr<RefCountedBytes>* data) {
  print_preview_data_service()->GetDataEntry(preview_ui_addr_str_, data);
}

void PrintPreviewUI::SetPrintPreviewData(const RefCountedBytes* data) {
    print_preview_data_service()->SetDataEntry(preview_ui_addr_str_, data);
}

void PrintPreviewUI::OnInitiatorTabClosed(
    const std::string& initiator_url) {
  StringValue initiator_tab_url(initiator_url);
  CallJavascriptFunction("onInitiatorTabClosed", initiator_tab_url);
}

void PrintPreviewUI::OnPreviewDataIsAvailable(int expected_pages_count,
                                              const string16& job_title,
                                              bool modifiable) {
  VLOG(1) << "Print preview request finished with "
          << expected_pages_count << " pages";
  if (!initial_preview_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PrintPreview.InitalDisplayTime",
                        base::TimeTicks::Now() - initial_preview_start_time_);
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.Initial",
                         expected_pages_count);
    initial_preview_start_time_ = base::TimeTicks();
  }
  FundamentalValue pages_count(expected_pages_count);
  StringValue title(job_title);
  FundamentalValue is_preview_modifiable(modifiable);
  StringValue ui_identifier(preview_ui_addr_str_);
  CallJavascriptFunction("updatePrintPreview", pages_count, title,
                         is_preview_modifiable, ui_identifier);
}

void PrintPreviewUI::OnFileSelectionCancelled() {
  CallJavascriptFunction("fileSelectionCancelled");
}

PrintPreviewDataService* PrintPreviewUI::print_preview_data_service() {
  return PrintPreviewDataService::GetInstance();
}
