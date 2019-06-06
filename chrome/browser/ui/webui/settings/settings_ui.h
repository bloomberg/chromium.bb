// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "content/public/browser/web_ui_controller.h"

class Profile;

namespace content {
class WebUIDataSource;
class WebUIMessageHandler;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace settings {

// The WebUI handler for chrome://settings.
class SettingsUI : public content::WebUIController {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit SettingsUI(content::WebUI* web_ui);
  ~SettingsUI() override;

#if defined(OS_CHROMEOS)
  // Initializes the WebUI message handlers for OS-specific settings.
  static void InitOSWebUIHandlers(Profile* profile,
                                  content::WebUI* web_ui,
                                  content::WebUIDataSource* html_source);

  static bool ShouldShowParentalControls(Profile* profile);
#endif  // defined(OS_CHROMEOS)

 private:
  void AddSettingsPageUIHandler(
      std::unique_ptr<content::WebUIMessageHandler> handler);

  WebuiLoadTimer webui_load_timer_;

  DISALLOW_COPY_AND_ASSIGN(SettingsUI);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UI_H_
