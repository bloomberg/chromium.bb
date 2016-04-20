// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
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

  // Whether the theme can be reset.
  bool ResetThemeEnabled() const;

  // Resets the UI theme of the browser to the default theme.
  void HandleResetTheme(const base::ListValue* args);

  // Sends the enabled state of the reset-theme control to the JS.
  void HandleGetResetThemeEnabled(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  // Open the wallpaper manager app.
  void HandleOpenWallpaperManager(const base::ListValue* args);
#endif

  Profile* profile_;  // Weak pointer.

  // Used to register for relevant notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppearanceHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_APPEARANCE_HANDLER_H_
