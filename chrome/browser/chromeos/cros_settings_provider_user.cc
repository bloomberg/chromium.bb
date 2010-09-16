// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings_provider_user.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/user_manager.h"

namespace {

Value* CreateSettingsBooleanValue(bool value, bool managed) {
  DictionaryValue* dict = new DictionaryValue;
  dict->Set("value", Value::CreateBooleanValue(value));
  dict->Set("managed", Value::CreateBooleanValue(managed));
  return dict;
}

}  // namespace

namespace chromeos {

UserCrosSettingsProvider::UserCrosSettingsProvider()
    : dict_(new DictionaryValue) {
  bool is_owner = UserManager::Get()->current_user_is_owner();

  Set(kAccountsPrefAllowBWSI, CreateSettingsBooleanValue(true, !is_owner));
  Set(kAccountsPrefAllowGuest, CreateSettingsBooleanValue(true, !is_owner));
  Set(kAccountsPrefShowUserNamesOnSignIn,
      CreateSettingsBooleanValue(true, !is_owner));

  ListValue* user_list = new ListValue;

  DictionaryValue* mock_user = new DictionaryValue;
  mock_user->SetString("email", "mock_user_1@gmail.com");
  mock_user->SetString("name", "Mock User One");
  mock_user->SetBoolean("owner", true);
  user_list->Append(mock_user);

  mock_user = new DictionaryValue;
  mock_user->SetString("email", "mock_user_2@gmail.com");
  mock_user->SetString("name", "Mock User Two");
  mock_user->SetBoolean("owner", false);
  user_list->Append(mock_user);

  Set(kAccountsPrefUsers, user_list);
}

void UserCrosSettingsProvider::Set(const std::string& path, Value* in_value) {
  dict_->Set(path, in_value);
}

bool UserCrosSettingsProvider::Get(const std::string& path,
                                   Value** out_value) const {
  Value* value = NULL;
  if (dict_->Get(path, &value) && value) {
    *out_value = value->DeepCopy();
    return true;
  }

  return false;
}

bool UserCrosSettingsProvider::HandlesSetting(const std::string& path) {
  return ::StartsWithASCII(path, "cros.accounts.", true);
}

}  // namespace chromeos
