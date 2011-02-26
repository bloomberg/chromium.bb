// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/webui/accounts_options_handler.h"

#include "base/json/json_reader.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/ui/webui/options/options_managed_banner_handler.h"
#include "chrome/common/url_constants.h"
#include "content/browser/webui/web_ui_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

AccountsOptionsHandler::AccountsOptionsHandler()
    : CrosOptionsPageUIHandler(new UserCrosSettingsProvider) {
}

AccountsOptionsHandler::~AccountsOptionsHandler() {
}

void AccountsOptionsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("whitelistUser",
      NewCallback(this, &AccountsOptionsHandler::WhitelistUser));
  web_ui_->RegisterMessageCallback("unwhitelistUser",
      NewCallback(this, &AccountsOptionsHandler::UnwhitelistUser));
  web_ui_->RegisterMessageCallback("fetchUserPictures",
      NewCallback(this, &AccountsOptionsHandler::FetchUserPictures));
  web_ui_->RegisterMessageCallback("whitelistExistingUsers",
      NewCallback(this, &AccountsOptionsHandler::WhitelistExistingUsers));
}

void AccountsOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "accountsPage",
                IDS_OPTIONS_ACCOUNTS_TAB_LABEL);

  localized_strings->SetString("allow_BWSI", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ALLOW_BWSI_DESCRIPTION));
  localized_strings->SetString("use_whitelist",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USE_WHITELIST_DESCRIPTION));
  localized_strings->SetString("show_user_on_signin",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_SHOW_USER_NAMES_ON_SINGIN_DESCRIPTION));
  localized_strings->SetString("username_edit_hint",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USERNAME_EDIT_HINT));
  localized_strings->SetString("username_format",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_USERNAME_FORMAT));
  localized_strings->SetString("add_users",l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_ADD_USERS));
  localized_strings->SetString("owner_only", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_OWNER_ONLY));
  localized_strings->SetString("owner_user_id", UTF8ToUTF16(
      UserCrosSettingsProvider::cached_owner()));

  localized_strings->SetString("current_user_is_owner",
      UserManager::Get()->current_user_is_owner() ?
      ASCIIToUTF16("true") : ASCIIToUTF16("false"));
}

UserCrosSettingsProvider* AccountsOptionsHandler::users_settings() const {
  return static_cast<UserCrosSettingsProvider*>(settings_provider_.get());
}

void AccountsOptionsHandler::WhitelistUser(const ListValue* args) {
  std::string email;
  if (!args->GetString(0, &email)) {
    return;
  }

  users_settings()->WhitelistUser(Authenticator::Canonicalize(email));
}

void AccountsOptionsHandler::UnwhitelistUser(const ListValue* args) {
  std::string email;
  if (!args->GetString(0, &email)) {
    return;
  }

  users_settings()->UnwhitelistUser(Authenticator::Canonicalize(email));
}

void AccountsOptionsHandler::FetchUserPictures(const ListValue* args) {
  DictionaryValue user_pictures;

  UserVector users = UserManager::Get()->GetUsers();
  for (UserVector::const_iterator it = users.begin();
       it != users.end(); ++it) {
    StringValue* image_url =
        new StringValue(chrome::kChromeUIUserImageURL + it->email());
    // SetWithoutPathExpansion because email has "." in it.
    user_pictures.SetWithoutPathExpansion(it->email(), image_url);
  }

  web_ui_->CallJavascriptFunction(L"AccountsOptions.setUserPictures",
      user_pictures);
}

void AccountsOptionsHandler::WhitelistExistingUsers(const ListValue* args) {
  ListValue whitelist_users;

  UserVector users = UserManager::Get()->GetUsers();
  for (UserVector::const_iterator it = users.begin();
       it < users.end(); ++it) {
    const std::string& email = it->email();
    if (!UserCrosSettingsProvider::IsEmailInCachedWhitelist(email)) {
      DictionaryValue* user_dict = new DictionaryValue;
      user_dict->SetString("name", it->GetDisplayName());
      user_dict->SetString("email", email);
      user_dict->SetBoolean("owner", false);

      whitelist_users.Append(user_dict);
    }
  }

  web_ui_->CallJavascriptFunction(L"AccountsOptions.addUsers", whitelist_users);
}

void AccountsOptionsHandler::Initialize() {
  DCHECK(web_ui_);
  banner_handler_.reset(
      new OptionsManagedBannerHandler(web_ui_,
                                      ASCIIToUTF16("AccountsOptions"),
                                      OPTIONS_PAGE_ACCOUNTS));
}

}  // namespace chromeos
