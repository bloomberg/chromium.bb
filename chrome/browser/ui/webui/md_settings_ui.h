// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MD_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MD_SETTINGS_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI handler for chrome://md-settings.
class MdSettingsUI : public content::WebUIController {
 public:
  explicit MdSettingsUI(content::WebUI* web_ui);
  ~MdSettingsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MdSettingsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MD_SETTINGS_UI_H_
