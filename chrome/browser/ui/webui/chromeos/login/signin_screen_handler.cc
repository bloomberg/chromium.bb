// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include "base/command_line.h"
#include "base/values.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Sign in screen id.
const char kSigninScreen[] = "signin";
// Sign in screen id for GAIA extension hosted content.
const char kGaiaSigninScreen[] = "gaia-signin";
// Start page of GAIA authentication extension.
const char kGaiaExtStartPage[] =
    "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/main.html";

// User dictionary keys.
const char kKeyName[] = "name";
const char kKeyEmailAddress[] = "emailAddress";
const char kKeyCanRemove[] = "canRemove";
const char kKeyImageUrl[] = "imageUrl";

}  // namespace

namespace chromeos {

SigninScreenHandler::SigninScreenHandler()
    : delegate_(WebUILoginDisplay::GetInstance()),
      show_on_init_(false),
      extension_driven_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kWebUIGaiaLogin)) {
  delegate_->SetWebUIHandler(this);
}

void SigninScreenHandler::GetLocalizedStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("signinScreenTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_TITLE));
  localized_strings->SetString("emailHint",
      l10n_util::GetStringUTF16(IDS_LOGIN_USERNAME));
  localized_strings->SetString("passwordHint",
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD));
  localized_strings->SetString("signinButton",
      l10n_util::GetStringUTF16(IDS_LOGIN_BUTTON));

  if (extension_driven_)
    localized_strings->SetString("authType", "ext");
  else
    localized_strings->SetString("authType", "webui");
}

void SigninScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kWebUIGaiaLogin)) {
    ShowScreen(kGaiaSigninScreen, kGaiaExtStartPage);
  } else {
    ShowScreen(kSigninScreen, NULL);
  }
}

// SigninScreenHandler, private: -----------------------------------------------

void SigninScreenHandler::Initialize() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

void SigninScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("authenticateUser",
      NewCallback(this, &SigninScreenHandler::HandleAuthenticateUser));
  web_ui_->RegisterMessageCallback("completeLogin",
      NewCallback(this, &SigninScreenHandler::HandleCompleteLogin));
  web_ui_->RegisterMessageCallback("getUsers",
      NewCallback(this, &SigninScreenHandler::HandleGetUsers));
  web_ui_->RegisterMessageCallback("launchIncognito",
      NewCallback(this, &SigninScreenHandler::HandleLaunchIncognito));
  web_ui_->RegisterMessageCallback("shutdownSystem",
      NewCallback(this, &SigninScreenHandler::HandleShutdownSystem));
  web_ui_->RegisterMessageCallback("removeUser",
      NewCallback(this, &SigninScreenHandler::HandleRemoveUser));
}

void SigninScreenHandler::HandleGetUsers(const base::ListValue* args) {
  ListValue users_list;

  // Grab the users from the user manager.
  UserVector users = UserManager::Get()->GetUsers();
  for (UserVector::const_iterator it = users.begin();
       it != users.end(); ++it) {
    const std::string& email = it->email();

    DictionaryValue* user_dict = new DictionaryValue();
    user_dict->SetString(kKeyName, it->GetDisplayName());
    user_dict->SetString(kKeyEmailAddress, email);
    user_dict->SetBoolean(kKeyCanRemove, !email.empty());

    if (!email.empty()) {
      long long timestamp = base::TimeTicks::Now().ToInternalValue();
      std::string image_url(
          StringPrintf("%s%s?id=%lld",
                       chrome::kChromeUIUserImageURL,
                       email.c_str(),
                       timestamp));
      user_dict->SetString(kKeyImageUrl, image_url);
    } else {
      std::string image_url(std::string(chrome::kChromeUIScheme) + "://" +
          std::string(chrome::kChromeUIThemePath) + "/IDR_LOGIN_DEFAULT_USER");
      user_dict->SetString(kKeyImageUrl, image_url);
    }

    users_list.Append(user_dict);
  }

  // Add the Guest to the user list.
  DictionaryValue* guest_dict = new DictionaryValue();
  guest_dict->SetString(kKeyName, l10n_util::GetStringUTF16(IDS_GUEST));
  guest_dict->SetString(kKeyEmailAddress, "");
  guest_dict->SetBoolean(kKeyCanRemove, false);
  std::string image_url(std::string(chrome::kChromeUIScheme) + "://" +
      std::string(chrome::kChromeUIThemePath) + "/IDR_LOGIN_GUEST");
  guest_dict->SetString(kKeyImageUrl, image_url);
  users_list.Append(guest_dict);

  // Call the Javascript callback
  web_ui_->CallJavascriptFunction("getUsersCallback", users_list);
}

void SigninScreenHandler::ClearAndEnablePassword() {
  web_ui_->CallJavascriptFunction("login.SigninScreen.reset");
}

void SigninScreenHandler::ShowError(const std::string& error_text,
                                    const std::string& help_link_text,
                                    HelpAppLauncher::HelpTopic help_topic_id) {
  // TODO(xiyuan): Pass error + help to a propery error UI and save topic id.
  ClearAndEnablePassword();
}

void SigninScreenHandler::HandleCompleteLogin(const base::ListValue* args) {
  std::string username;
  std::string password;
  if (!args->GetString(0, &username) ||
      !args->GetString(1, &password)) {
    NOTREACHED();
    return;
  }

  delegate_->CompleteLogin(username, password);
}

void SigninScreenHandler::HandleAuthenticateUser(const base::ListValue* args) {
  std::string username;
  std::string password;
  if (!args->GetString(0, &username) ||
      !args->GetString(1, &password)) {
    NOTREACHED();
    return;
  }

  delegate_->Login(username, password);
}

void SigninScreenHandler::HandleLaunchIncognito(const base::ListValue* args) {
  delegate_->LoginAsGuest();
}

void SigninScreenHandler::HandleShutdownSystem(const base::ListValue* args) {
  DCHECK(CrosLibrary::Get()->EnsureLoaded());
  CrosLibrary::Get()->GetPowerLibrary()->RequestShutdown();
}

void SigninScreenHandler::HandleRemoveUser(const base::ListValue* args) {
  std::string email;
  if (!args->GetString(0, &email)) {
    NOTREACHED();
    return;
  }

  UserManager::Get()->RemoveUserFromList(email);
}

}  // namespace chromeos
