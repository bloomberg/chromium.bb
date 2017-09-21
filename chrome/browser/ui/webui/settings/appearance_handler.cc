// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/appearance_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "content/public/browser/web_ui.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chromeos/login/login_state.h"
#endif

namespace settings {

AppearanceHandler::AppearanceHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)) {
}

AppearanceHandler::~AppearanceHandler() {}

void AppearanceHandler::OnJavascriptAllowed() {}
void AppearanceHandler::OnJavascriptDisallowed() {}

void AppearanceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "useDefaultTheme",
      base::Bind(&AppearanceHandler::HandleUseDefaultTheme,
                 base::Unretained(this)));
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "useSystemTheme",
      base::Bind(&AppearanceHandler::HandleUseSystemTheme,
                 base::Unretained(this)));
#endif
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "openWallpaperManager",
      base::Bind(&AppearanceHandler::HandleOpenWallpaperManager,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "isWallpaperSettingVisible",
      base::Bind(&AppearanceHandler::IsWallpaperSettingVisible,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "isWallpaperPolicyControlled",
      base::Bind(&AppearanceHandler::IsWallpaperPolicyControlled,
                 base::Unretained(this)));
#endif
}

void AppearanceHandler::HandleUseDefaultTheme(const base::ListValue* args) {
  ThemeServiceFactory::GetForProfile(profile_)->UseDefaultTheme();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void AppearanceHandler::HandleUseSystemTheme(const base::ListValue* args) {
  if (profile_->IsSupervised())
    NOTREACHED();
  else
    ThemeServiceFactory::GetForProfile(profile_)->UseSystemTheme();
}
#endif

#if defined(OS_CHROMEOS)
void AppearanceHandler::IsWallpaperSettingVisible(const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  AllowJavascript();

  bool is_wallpaper_visible = false;
  const chromeos::LoginState* login_state = chromeos::LoginState::Get();
  const chromeos::LoginState::LoggedInUserType user_type =
      login_state->GetLoggedInUserType();
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();

  // Only login, whitelist types and active users are allowed to change
  // their wallpaper. Then show the wallpaper setting row for them.
  if (login_state->IsUserLoggedIn() && user &&
      (user_type == chromeos::LoginState::LOGGED_IN_USER_REGULAR ||
       user_type == chromeos::LoginState::LOGGED_IN_USER_OWNER ||
       user_type == chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT ||
       user_type == chromeos::LoginState::LOGGED_IN_USER_SUPERVISED)) {
    is_wallpaper_visible = true;
  }

  ResolveJavascriptCallback(*callback_id, base::Value(is_wallpaper_visible));
}

void AppearanceHandler::IsWallpaperPolicyControlled(
    const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  AllowJavascript();

  ResolveJavascriptCallback(
      *callback_id,
      base::Value(chromeos::WallpaperManager::Get()->IsPolicyControlled(
          user_manager::UserManager::Get()->GetActiveUser()->GetAccountId())));
}

void AppearanceHandler::HandleOpenWallpaperManager(
    const base::ListValue* /*args*/) {
  if (!chromeos::WallpaperManager::Get()->IsPolicyControlled(
          user_manager::UserManager::Get()->GetActiveUser()->GetAccountId())) {
    chromeos::WallpaperManager::Get()->Open();
  }
}
#endif

}  // namespace settings
