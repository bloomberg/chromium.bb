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
      base::Bind(&AppearanceHandler::ResetTheme, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getResetThemeEnabled",
      base::Bind(&AppearanceHandler::GetResetThemeEnabled,
                 base::Unretained(this)));
}

void AppearanceHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED: {
      base::StringValue event("reset-theme-enabled-changed");
      base::FundamentalValue enabled(QueryResetThemeEnabledState());
      web_ui()->CallJavascriptFunction(
          "cr.webUIListenerCallback", event, enabled);
      break;
    }
    default:
      NOTREACHED();
  }
}

void AppearanceHandler::ResetTheme(const base::ListValue* /* args */) {
  Profile* profile = Profile::FromWebUI(web_ui());
  ThemeServiceFactory::GetForProfile(profile)->UseDefaultTheme();
}

base::FundamentalValue AppearanceHandler::QueryResetThemeEnabledState() {
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  bool is_system_theme = false;

  // TODO(jhawkins): Handle native/system theme button.

  bool is_classic_theme = !is_system_theme &&
                          theme_service->UsingDefaultTheme();
  return base::FundamentalValue(!is_classic_theme);
}

void AppearanceHandler::GetResetThemeEnabled(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  base::FundamentalValue enabled(QueryResetThemeEnabledState());
  CallJavascriptCallback(*callback_id, enabled);
}

}  // namespace settings
