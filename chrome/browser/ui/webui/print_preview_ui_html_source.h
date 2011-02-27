// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_HTML_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_HTML_SOURCE_H_
#pragma once

#include <string>
#include <utility>

#include "chrome/browser/webui/chrome_url_data_manager.h"

namespace base {
class SharedMemory;
}

class PrintPreviewUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // A SharedMemory that contains the data for print preview,
  // and the size of the print preview data in bytes.
  typedef std::pair<base::SharedMemory*, uint32> PrintPreviewData;

  PrintPreviewUIHTMLSource();
  virtual ~PrintPreviewUIHTMLSource();

  // Gets the print preview |data|. The data is valid as long as the
  // PrintPreviewHandler is valid and SetPrintPreviewData() does not get called.
  void GetPrintPreviewData(PrintPreviewData* data);

  // Sets the print preview |data|. PrintPreviewHandler owns the data and is
  // responsible for freeing it when either:
  // a) there is new data.
  // b) when PrintPreviewHandler is destroyed.
  void SetPrintPreviewData(const PrintPreviewData& data);

  // ChromeURLDataManager::DataSource implementation.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
  // Current print preview data, the contents of which are owned by
  // PrintPreviewHandler.
  PrintPreviewData data_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewUIHTMLSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_HTML_SOURCE_H_
