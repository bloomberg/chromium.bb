// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_UI_H_
#pragma once

#include <string>

#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"

class QuotaInternalsUI : public ChromeWebUI {
 public:
  explicit QuotaInternalsUI(TabContents* contents);
  virtual ~QuotaInternalsUI() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(QuotaInternalsUI);
};

namespace quota_internals {

class QuotaInternalsHTMLSource : public ChromeWebUIDataSource {
 public:
  static const char kStringsJSPath[];

  QuotaInternalsHTMLSource();
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;

 private:
  virtual ~QuotaInternalsHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(QuotaInternalsHTMLSource);
};

}  // namespace quota_internals

#endif  // CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_UI_H_
