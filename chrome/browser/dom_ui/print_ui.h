// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_PRINT_UI_H_
#define CHROME_BROWSER_DOM_UI_PRINT_UI_H_

#include <string>

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"

// The TabContents used for the print page.
class PrintUI : public DOMUI {
 public:
  explicit PrintUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintUI);
};

// To serve dynamic print data off of chrome.
class PrintUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  PrintUIHTMLSource();

  // ChromeURLDataManager overrides.
  virtual void StartDataRequest(const std::string& path, int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~PrintUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(PrintUIHTMLSource);
};

#endif  // CHROME_BROWSER_DOM_UI_PRINT_UI_H_
