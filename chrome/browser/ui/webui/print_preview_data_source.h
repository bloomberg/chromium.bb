// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_DATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_DATA_SOURCE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

// PrintPreviewDataSource serves data for chrome://print requests.
//
// The format for requesting data is as follows:
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

class PrintPreviewDataSource : public ChromeURLDataManager::DataSource {
 public:
  PrintPreviewDataSource();

  // ChromeURLDataManager::DataSource implementation.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;

 private:
  virtual ~PrintPreviewDataSource();

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDataSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_DATA_SOURCE_H_
