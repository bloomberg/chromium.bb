// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mock_cros_settings.h"

namespace chromeos {

MockCrosSettings::MockCrosSettings()
    : dict_(new DictionaryValue) {
  // Some mock settings
  SetBoolean(kAccountsPrefAllowBWSI, true);
  SetBoolean(kAccountsPrefAllowGuest, true);

  ListValue* user_list = new ListValue;

  DictionaryValue* mock_user = new DictionaryValue;
  mock_user->SetString(L"email", L"mock_user_1@gmail.com");
  user_list->Append(mock_user);

  mock_user = new DictionaryValue;
  mock_user->SetString(L"email", L"mock_user_2@gmail.com");
  user_list->Append(mock_user);

  Set(kAccountsPrefUsers, user_list);
}

void MockCrosSettings::Set(const std::wstring& path, Value* in_value) {
  dict_->Set(path, in_value);
}

bool MockCrosSettings::Get(const std::wstring& path, Value** out_value) const {
  return dict_->Get(path, out_value);
}

}  // namespace chromeos
