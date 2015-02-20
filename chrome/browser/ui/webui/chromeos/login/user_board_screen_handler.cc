// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/user_board_screen_handler.h"

#include "chrome/browser/chromeos/login/ui/models/user_board_model.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

UserBoardScreenHandler::UserBoardScreenHandler() : model_(nullptr) {
}

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

void UserBoardScreenHandler::HandleHardlockPod(const std::string& user_id) {
  CHECK(model_);
  model_->HardLockPod(user_id);
}

void UserBoardScreenHandler::HandleAttemptUnlock(const std::string& user_id) {
  CHECK(model_);
  model_->AttemptEasyUnlock(user_id);
}

void UserBoardScreenHandler::HandleRecordClickOnLockIcon(
    const std::string& user_id) {
  CHECK(model_);
  model_->RecordClickOnLockIcon(user_id);
}

//----------------- API

void UserBoardScreenHandler::SetPublicSessionDisplayName(
    const std::string& user_id,
    const std::string& display_name) {
  CallJS("login.AccountPickerScreen.setPublicSessionDisplayName", user_id,
         display_name);
}

void UserBoardScreenHandler::SetPublicSessionLocales(
    const std::string& user_id,
    scoped_ptr<base::ListValue> locales,
    const std::string& default_locale,
    bool multiple_recommended_locales) {
  CallJS("login.AccountPickerScreen.setPublicSessionLocales", user_id, *locales,
         default_locale, multiple_recommended_locales);
}

void UserBoardScreenHandler::ShowBannerMessage(const base::string16& message) {
  CallJS("login.AccountPickerScreen.showBannerMessage", message);
}

void UserBoardScreenHandler::ShowUserPodCustomIcon(
    const std::string& user_id,
    const base::DictionaryValue& icon) {
  CallJS("login.AccountPickerScreen.showUserPodCustomIcon", user_id, icon);
}

void UserBoardScreenHandler::HideUserPodCustomIcon(const std::string& user_id) {
  CallJS("login.AccountPickerScreen.hideUserPodCustomIcon", user_id);
}

void UserBoardScreenHandler::SetAuthType(
    const std::string& user_id,
    ScreenlockBridge::LockHandler::AuthType auth_type,
    const base::string16& initial_value) {
  CallJS("login.AccountPickerScreen.setAuthType", user_id,
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

}  // namespace chromeos
