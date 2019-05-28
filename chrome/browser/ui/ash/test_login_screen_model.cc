// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_login_screen_model.h"

TestLoginScreenModel::TestLoginScreenModel() = default;
TestLoginScreenModel::~TestLoginScreenModel() = default;

void TestLoginScreenModel::SetUserList(
    const std::vector<ash::LoginUserInfo>& users) {}
void TestLoginScreenModel::SetAvatarForUser(const AccountId& account_id,
                                            const ash::UserAvatar& avatar) {}
void TestLoginScreenModel::ShowEasyUnlockIcon(
    const AccountId& account_id,
    const ash::EasyUnlockIconOptions& icon) {}
void TestLoginScreenModel::UpdateWarningMessage(const base::string16& message) {
}
void TestLoginScreenModel::SetFingerprintState(const AccountId& account_id,
                                               ash::FingerprintState state) {}
void TestLoginScreenModel::SetPublicSessionLocales(
    const AccountId& account_id,
    const std::vector<ash::LocaleItem>& locales,
    const std::string& default_locale,
    bool show_advanced_view) {}
void TestLoginScreenModel::SetPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale,
    const std::vector<ash::InputMethodItem>& keyboard_layouts) {}
void TestLoginScreenModel::HandleFocusLeavingLockScreenApps(bool reverse) {}
void TestLoginScreenModel::NotifyOobeDialogState(ash::OobeDialogState state) {}
