// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "content/browser/webui/web_ui.h"

class PrintPreviewUIHTMLSource;

class PrintPreviewUI : public WebUI {
 public:
  explicit PrintPreviewUI(TabContents* contents);
  virtual ~PrintPreviewUI();

  PrintPreviewUIHTMLSource* html_source();

  // Notify the Web UI renderer that preview data is available.
  // |expected_pages_count| specifies the total number of pages.
  // |job_title| is the title of the page being previewed.
  // |modifiable| indicates if the preview can be rerendered with different
  // print settings.
  void OnPreviewDataIsAvailable(int expected_pages_count,
                                const string16& job_title,
                                bool modifiable);

  // Notify the Web UI that initiator tab is closed, so we can disable all
  // the controls that need the initiator tab for generating the preview data.
  // |initiator_tab_url| is passed in order to display a more accurate error
  // message.
  void OnInitiatorTabClosed(const std::string& initiator_tab_url);

 private:
  scoped_refptr<PrintPreviewUIHTMLSource> html_source_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_H_
