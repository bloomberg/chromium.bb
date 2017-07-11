// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SAFE_BROWSING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SAFE_BROWSING_UI_H_

#include "base/macros.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://safe-browsing
class SafeBrowsingUI : public content::WebUIController {
 public:
  explicit SafeBrowsingUI(content::WebUI* web_ui);
  ~SafeBrowsingUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SAFE_BROWSING_UI_H_
