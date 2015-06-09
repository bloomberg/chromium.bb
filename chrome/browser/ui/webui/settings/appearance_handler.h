// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/md_settings_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class ListValue;
}

namespace content {
class WebUI;
}

class Profile;

namespace settings {

// Chrome "Appearance" settings page UI handler.
class AppearanceHandler : public SettingsPageUIHandler,
                          public content::NotificationObserver {
 public:
  explicit AppearanceHandler(content::WebUI* webui);
  ~AppearanceHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;

 private:
  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Queries the enabled state of the reset-theme control.
  base::FundamentalValue QueryResetThemeEnabledState();

  // Resets the UI theme of the browser to the default theme.
  void ResetTheme(const base::ListValue*);

  // Sends the enabled state of the reset-theme control to the JS.
  void GetResetThemeEnabled(const base::ListValue* args);

  Profile* profile_;  // Weak pointer.

  // Used to register for relevant notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppearanceHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
