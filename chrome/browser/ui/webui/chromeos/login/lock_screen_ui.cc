// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/lock_screen_ui.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace chromeos {

LockScreenUI::LockScreenUI(TabContents* contents) : ChromeWebUI(contents) {
  // Set up the chrome://lock source.
  ChromeWebUIDataSource* html_source =
      new ChromeWebUIDataSource(chrome::kChromeUILockScreenHost);

  // Register callback handler.
  RegisterMessageCallback("unlockScreenRequest",
      base::Bind(&LockScreenUI::UnlockScreenRequest, base::Unretained(this)));
  RegisterMessageCallback("signoutRequest",
      base::Bind(&LockScreenUI::SignoutRequest, base::Unretained(this)));

  // Email address of current user.
  html_source->AddString("email", ASCIIToUTF16(
      chromeos::UserManager::Get()->logged_in_user().email()));

  // Localized strings.
  // TODO(flackr): Change button text to unlock.
  html_source->AddLocalizedString("unlock", IDS_LOGIN_BUTTON);
  html_source->AddLocalizedString("password", IDS_LOGIN_PASSWORD);
  html_source->AddLocalizedString("signout", IDS_SCREEN_LOCK_SIGN_OUT);
  html_source->set_json_path("strings.js");

  // Add required resources.
  html_source->add_resource_path("lock_screen.css", IDR_LOCK_SCREEN_CSS);
  html_source->add_resource_path("lock_screen.js", IDR_LOCK_SCREEN_JS);
  html_source->set_default_resource(IDR_LOCK_SCREEN_HTML);

  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);

  // Set up the chrome://userimage/ source.
  UserImageSource* user_image_source = new UserImageSource();
  profile->GetChromeURLDataManager()->AddDataSource(user_image_source);
}

LockScreenUI::~LockScreenUI() {
}

void LockScreenUI::UnlockScreenRequest(const base::ListValue* args) {
  string16 password;
  if (!args->GetString(0, &password))
    return;

  chromeos::ScreenLocker* screen_locker_ =
      chromeos::ScreenLocker::default_screen_locker();
  if (!screen_locker_)
    return;

  screen_locker_->Authenticate(password);
}

void LockScreenUI::SignoutRequest(const base::ListValue* args) {
  chromeos::ScreenLocker* screen_locker_ =
      chromeos::ScreenLocker::default_screen_locker();
  if (!screen_locker_)
    return;

  screen_locker_->Signout();
}

}  // namespace chromeos
