// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/languages_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#endif

namespace settings {

LanguagesHandler::LanguagesHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)) {
}

LanguagesHandler::~LanguagesHandler() {
}

void LanguagesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setUILanguage",
      base::Bind(&LanguagesHandler::HandleSetUILanguage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restart",
      base::Bind(&LanguagesHandler::HandleRestart,
                 base::Unretained(this)));
}

void LanguagesHandler::HandleSetUILanguage(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());

  std::string language_code;
  CHECK(args->GetString(0, &language_code));

#if defined(OS_WIN)
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kApplicationLocale, language_code);
#elif defined(OS_CHROMEOS)
  // Secondary users and public session users cannot change the locale.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user &&
      user->email() == user_manager->GetPrimaryUser()->email() &&
      user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    profile_->ChangeAppLocale(language_code,
                              Profile::APP_LOCALE_CHANGED_VIA_SETTINGS);
  }
#else
  NOTREACHED() << "Attempting to set locale on unsupported platform";
#endif
}

void LanguagesHandler::HandleRestart(const base::ListValue* args) {
#if defined(OS_CHROMEOS)
  chrome::AttemptUserExit();
#else
  chrome::AttemptRestart();
#endif
}

}  // namespace settings
