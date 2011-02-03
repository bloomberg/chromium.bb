// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/print_preview_ui.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/print_preview_handler.h"
#include "chrome/browser/dom_ui/print_preview_ui_html_source.h"

PrintPreviewUI::PrintPreviewUI(TabContents* contents)
    : DOMUI(contents),
      html_source_(new PrintPreviewUIHTMLSource()) {
  // PrintPreviewUI owns |handler|.
  PrintPreviewHandler* handler = new PrintPreviewHandler();
  AddMessageHandler(handler->Attach(this));

  // Set up the chrome://print/ source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          html_source_));
}

PrintPreviewUI::~PrintPreviewUI() {
}

PrintPreviewUIHTMLSource* PrintPreviewUI::html_source() {
  return html_source_.get();
}
