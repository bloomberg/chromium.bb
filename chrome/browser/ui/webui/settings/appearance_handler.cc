// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/appearance_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_ui.h"

#if defined(OS_CHROMEOS)
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shell.h"
#endif

namespace settings {

AppearanceHandler::AppearanceHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile_)));
}

AppearanceHandler::~AppearanceHandler() {
  registrar_.RemoveAll();
}

void AppearanceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "resetTheme",
      base::Bind(&AppearanceHandler::HandleResetTheme, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getResetThemeEnabled",
      base::Bind(&AppearanceHandler::HandleGetResetThemeEnabled,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "openWallpaperManager",
      base::Bind(&AppearanceHandler::HandleOpenWallpaperManager,
                 base::Unretained(this)));
#endif
}

void AppearanceHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED: {
      web_ui()->CallJavascriptFunction(
          "cr.webUIListenerCallback",
          base::StringValue("reset-theme-enabled-changed"),
          base::FundamentalValue(ResetThemeEnabled()));
      break;
    }
    default:
      NOTREACHED();
  }
}

void AppearanceHandler::HandleResetTheme(const base::ListValue* /*args*/) {
  ThemeServiceFactory::GetForProfile(profile_)->UseDefaultTheme();
}

bool AppearanceHandler::ResetThemeEnabled() const {
  // TODO(jhawkins): Handle native/system theme button.
  return !ThemeServiceFactory::GetForProfile(profile_)->UsingDefaultTheme();
}

void AppearanceHandler::HandleGetResetThemeEnabled(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  ResolveJavascriptCallback(*callback_id,
                            base::FundamentalValue(ResetThemeEnabled()));
}

#if defined(OS_CHROMEOS)
void AppearanceHandler::HandleOpenWallpaperManager(
    const base::ListValue* /*args*/) {
  ash::Shell::GetInstance()->user_wallpaper_delegate()->OpenSetWallpaperPage();
}
#endif

}  // namespace settings
