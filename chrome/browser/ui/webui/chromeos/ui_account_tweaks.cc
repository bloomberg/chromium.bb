// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

void AddAccountUITweaksLocalizedValues(
    base::DictionaryValue* localized_strings,
    Profile* profile) {
  DCHECK(localized_strings);

  std::string owner_email;
  CrosSettings::Get()->GetString(kDeviceOwner, &owner_email);
  // Translate owner's email to the display email.
  std::string display_email =
      user_manager::UserManager::Get()->GetUserDisplayEmail(owner_email);
  localized_strings->SetString("ownerUserId", display_email);

  localized_strings->SetBoolean("currentUserIsOwner",
      ProfileHelper::IsOwnerProfile(profile));

  localized_strings->SetBoolean(
      "loggedInAsGuest", user_manager::UserManager::Get()->IsLoggedInAsGuest());

  localized_strings->SetBoolean(
      "loggedInAsSupervisedUser",
      user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser());

  localized_strings->SetBoolean(
      "loggedInAsPublicAccount",
      user_manager::UserManager::Get()->IsLoggedInAsPublicAccount());
}

void AddAccountUITweaksLocalizedValues(
    content::WebUIDataSource* source,
    Profile* profile) {
  DCHECK(source);
  base::DictionaryValue dict;
  AddAccountUITweaksLocalizedValues(&dict, profile);
  source->AddLocalizedStrings(dict);
}

}  // namespace chromeos
