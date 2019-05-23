// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_MODEL_H_
#define CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_MODEL_H_

#include "ash/public/cpp/login_screen_model.h"
#include "base/macros.h"

class TestLoginScreenModel : public ash::LoginScreenModel {
 public:
  TestLoginScreenModel();
  ~TestLoginScreenModel() override;

  // ash::LoginScreenModel:
  void SetUserList(const std::vector<ash::LoginUserInfo>& users) override;
  void SetAvatarForUser(const AccountId& account_id,
                        const ash::UserAvatar& avatar) override;
  void SetFingerprintState(const AccountId& account_id,
                           ash::FingerprintState state) override;
  void ShowEasyUnlockIcon(const AccountId& user,
                          const ash::EasyUnlockIconOptions& icon) override;
  void SetPublicSessionLocales(const AccountId& account_id,
                               const std::vector<ash::LocaleItem>& locales,
                               const std::string& default_locale,
                               bool show_advanced_view) override;
  void SetPublicSessionKeyboardLayouts(
      const AccountId& account_id,
      const std::string& locale,
      const std::vector<ash::InputMethodItem>& keyboard_layouts) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLoginScreenModel);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_MODEL_H_
