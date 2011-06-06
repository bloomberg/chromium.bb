// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/time.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

class PrintPreviewDataService;

class PrintPreviewUI : public ChromeWebUI {
 public:
  explicit PrintPreviewUI(TabContents* contents);
  virtual ~PrintPreviewUI();

  // Gets the print preview |data|. The data is valid as long as the
  // PrintPreviewDataService is valid and SetPrintPreviewData() does not get
  // called.
  void GetPrintPreviewData(scoped_refptr<RefCountedBytes>* data);

  // Sets the print preview |data|.
  void SetPrintPreviewData(const RefCountedBytes* data);

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
  // Helper function
  PrintPreviewDataService* print_preview_data_service();

  base::TimeTicks initial_preview_start_time_;

  // Store the PrintPreviewUI address string.
  std::string preview_ui_addr_str_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_H_
