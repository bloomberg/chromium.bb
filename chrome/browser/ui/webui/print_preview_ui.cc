// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview_ui_html_source.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"

PrintPreviewUI::PrintPreviewUI(TabContents* contents)
    : WebUI(contents),
      html_source_(new PrintPreviewUIHTMLSource()) {
  // PrintPreviewUI owns |handler|.
  PrintPreviewHandler* handler = new PrintPreviewHandler();
  AddMessageHandler(handler->Attach(this));

  // Set up the chrome://print/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source_);
}

PrintPreviewUI::~PrintPreviewUI() {
}

PrintPreviewUIHTMLSource* PrintPreviewUI::html_source() {
  return html_source_.get();
}

void PrintPreviewUI::PreviewDataIsAvailable(int expected_pages_count,
                                            const string16& job_title) {
  FundamentalValue pages_count(expected_pages_count);
  StringValue title(job_title);
  CallJavascriptFunction("updatePrintPreview", pages_count, title);
}
