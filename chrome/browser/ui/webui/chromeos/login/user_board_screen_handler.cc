// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/user_board_screen_handler.h"

#include "chrome/browser/chromeos/login/ui/models/user_board_model.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

UserBoardScreenHandler::UserBoardScreenHandler()
    : model_(nullptr), weak_factory_(this) {}

UserBoardScreenHandler::~UserBoardScreenHandler() {
}

void UserBoardScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
}

void UserBoardScreenHandler::RegisterMessages() {
  AddCallback("getUsers", &UserBoardScreenHandler::HandleGetUsers);
  AddCallback("attemptUnlock", &UserBoardScreenHandler::HandleAttemptUnlock);
  AddCallback("hardlockPod", &UserBoardScreenHandler::HandleHardlockPod);
  AddCallback("recordClickOnLockIcon",
              &UserBoardScreenHandler::HandleRecordClickOnLockIcon);
}

void UserBoardScreenHandler::Initialize() {
}

//----------------- Handlers

void UserBoardScreenHandler::HandleGetUsers() {
  CHECK(model_);
  model_->SendUserList();
}

void UserBoardScreenHandler::HandleHardlockPod(const AccountId& account_id) {
  CHECK(model_);
  model_->HardLockPod(account_id);
}

void UserBoardScreenHandler::HandleAttemptUnlock(const AccountId& account_id) {
  CHECK(model_);
  model_->AttemptEasyUnlock(account_id);
}

void UserBoardScreenHandler::HandleRecordClickOnLockIcon(
    const AccountId& account_id) {
  CHECK(model_);
  model_->RecordClickOnLockIcon(account_id);
}

//----------------- API

void UserBoardScreenHandler::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {
  CallJS("login.AccountPickerScreen.setPublicSessionDisplayName", account_id,
         display_name);
}

void UserBoardScreenHandler::SetPublicSessionLocales(
    const AccountId& account_id,
    std::unique_ptr<base::ListValue> locales,
    const std::string& default_locale,
    bool multiple_recommended_locales) {
  CallJS("login.AccountPickerScreen.setPublicSessionLocales", account_id,
         *locales, default_locale, multiple_recommended_locales);
}

void UserBoardScreenHandler::ShowBannerMessage(const base::string16& message) {
  CallJS("login.AccountPickerScreen.showBannerMessage", message);
}

void UserBoardScreenHandler::ShowUserPodCustomIcon(
    const AccountId& account_id,
    const base::DictionaryValue& icon) {
  CallJS("login.AccountPickerScreen.showUserPodCustomIcon", account_id, icon);
}

void UserBoardScreenHandler::HideUserPodCustomIcon(
    const AccountId& account_id) {
  CallJS("login.AccountPickerScreen.hideUserPodCustomIcon", account_id);
}

void UserBoardScreenHandler::SetAuthType(
    const AccountId& account_id,
    proximity_auth::ScreenlockBridge::LockHandler::AuthType auth_type,
    const base::string16& initial_value) {
  CallJS("login.AccountPickerScreen.setAuthType", account_id,
         static_cast<int>(auth_type), base::StringValue(initial_value));
}

void UserBoardScreenHandler::Bind(UserBoardModel& model) {
  model_ = &model;
  BaseScreenHandler::SetBaseScreen(model_);
}

void UserBoardScreenHandler::Unbind() {
  model_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

base::WeakPtr<UserBoardView> UserBoardScreenHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos
