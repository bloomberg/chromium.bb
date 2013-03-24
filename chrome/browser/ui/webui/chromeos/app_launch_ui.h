// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_APP_LAUNCH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_APP_LAUNCH_UI_H_

#include <string>

#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

// A custom WebUI that shows app install/launch splash screen in ChromeOS
// app mode (aka Kiosk).
class AppLaunchUI : public content::WebUIController {
 public:
  explicit AppLaunchUI(content::WebUI* web_ui);

  void SetLaunchText(const std::string& text);

 private:
  class AppLaunchUIHandler;

  AppLaunchUIHandler* handler_;  // Owned by WebUI.

  DISALLOW_COPY_AND_ASSIGN(AppLaunchUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_APP_LAUNCH_UI_H_
