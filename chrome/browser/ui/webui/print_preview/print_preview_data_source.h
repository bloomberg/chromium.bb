// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_DATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_DATA_SOURCE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"

// PrintPreviewDataSource serves data for chrome://print requests.
//
// The format for requesting PDF data is as follows:
// chrome://print/<PrintPreviewUIAddrStr>/<PageIndex>/print.pdf
//
// Parameters (< > required):
//    <PrintPreviewUIAddrStr> = Print preview UI identifier.
//    <PageIndex> = Page index is zero-based or
//                  |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to represent
//                  a print ready PDF.
//
// Example:
//    chrome://print/0xab0123ef/10/print.pdf
//
// Requests to chrome://print with paths not ending in /print.pdf are used
// to return the markup or other resources for the print preview page itself.
class PrintPreviewDataSource : public ChromeWebUIDataSource {
 public:
  PrintPreviewDataSource();

  // ChromeURLDataManager::DataSource implementation.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;
 private:
  virtual ~PrintPreviewDataSource();
  void Init();

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDataSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_DATA_SOURCE_H_
