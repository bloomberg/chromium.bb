// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/locally_managed_user_creation_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_flow.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Locally managed user creation screen id.
const char kLocallyManagedUserCreationScreen[] =
    "locally-managed-user-creation";

}  // namespace

namespace chromeos {

LocallyManagedUserCreationScreenHandler::
    LocallyManagedUserCreationScreenHandler() {}

LocallyManagedUserCreationScreenHandler::
    ~LocallyManagedUserCreationScreenHandler() {}

void LocallyManagedUserCreationScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("managedUserCreationErrorTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_TITLE);
  builder->Add(
      "managedUserCreationFlowRetryButtonTitle",
      IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_RETRY_BUTTON_TITLE);
  builder->Add(
      "managedUserCreationFlowCancelButtonTitle",
      IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_CANCEL_BUTTON_TITLE);
  builder->Add(
      "managedUserCreationFlowFinishButtonTitle",
       IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_SUCCESS_BUTTON_TITLE);
  builder->Add("managedUserProfileCreatedMessageTemplate",
               IDS_CREATE_LOCALLY_MANAGED_USER_PROFILE_CREATED_TEXT);
  builder->Add("managedUserInstructionTemplate",
               IDS_CREATE_LOCALLY_MANAGED_USER_INSTRUCTIONS_TEXT);
  builder->Add("createManagedUserNameTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ACCOUNT_NAME_TITLE);
  builder->Add("createManagedUserPasswordTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_TITLE);
  builder->Add("createManagedUserPasswordHint",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_HINT);
  builder->Add("createManagedUserPasswordConfirmHint",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_CONFIRM_HINT);
  builder->Add("managedUserCreationFlowProceedButtonTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_CONTINUE_BUTTON_TEXT);
  builder->Add("managedUserCreationFlowStartButtonTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_START_BUTTON_TEXT);
  builder->Add("managedUserCreationFlowPreviousButtonTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PREVIOUS_BUTTON_TEXT);
  builder->Add("managedUserCreationFlowNextButtonTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_NEXT_BUTTON_TEXT);
  builder->Add("createManagedUserPasswordMismatchError",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_MISMATCH_ERROR);
  builder->Add("createManagedUserSelectManagerTitle",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_SELECT_MANAGER_TEXT);
  builder->Add("createManagedUserManagerPasswordHint",
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_MANAGER_PASSWORD_HINT);
  builder->Add("createManagedUserWrongManagerPasswordText",
               IDS_CREATE_LOCALLY_MANAGED_USER_MANAGER_PASSWORD_ERROR);
}

void LocallyManagedUserCreationScreenHandler::Initialize() {}

void LocallyManagedUserCreationScreenHandler::RegisterMessages() {
  AddCallback("finishLocalManagedUserCreation",
              &LocallyManagedUserCreationScreenHandler::
                  HandleFinishLocalManagedUserCreation);
  AddCallback("abortLocalManagedUserCreation",
              &LocallyManagedUserCreationScreenHandler::
                  HandleAbortLocalManagedUserCreation);
  AddCallback("checkLocallyManagedUserName",
              &LocallyManagedUserCreationScreenHandler::
                  HandleCheckLocallyManagedUserName);
  AddCallback("authenticateManagerInLocallyManagedUserCreationFlow",
              &LocallyManagedUserCreationScreenHandler::
                  HandleAuthenticateManager);
  AddCallback("specifyLocallyManagedUserCreationFlowUserData",
              &LocallyManagedUserCreationScreenHandler::
                  HandleCreateManagedUser);
  AddCallback("managerSelectedOnLocallyManagedUserCreationFlow",
              &LocallyManagedUserCreationScreenHandler::
                  HandleManagerSelected);
}

void LocallyManagedUserCreationScreenHandler::PrepareToShow() {}

void LocallyManagedUserCreationScreenHandler::Show() {
  scoped_ptr<DictionaryValue> data(new base::DictionaryValue());
  scoped_ptr<ListValue> users_list(new base::ListValue());
  const UserList& users = UserManager::Get()->GetUsers();
  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);

  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetType() != User::USER_TYPE_REGULAR)
      continue;
    bool is_owner = ((*it)->email() == owner);
    DictionaryValue* user_dict = new DictionaryValue();
    SigninScreenHandler::FillUserDictionary(*it, is_owner, user_dict);
    users_list->Append(user_dict);
  }
  data->Set("managers", users_list.release());
  ShowScreen(OobeUI::kScreenManagedUserCreationFlow, data.get());
}

void LocallyManagedUserCreationScreenHandler::Hide() {}

void LocallyManagedUserCreationScreenHandler::
    ShowManagerInconsistentStateErrorPage() {
  ShowErrorPage(
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_MANAGER_INCONSISTENT_STATE),
      false);
}

void LocallyManagedUserCreationScreenHandler::ShowIntroPage() {
  CallJS("login.LocallyManagedUserCreationScreen.showIntroPage");
}

void LocallyManagedUserCreationScreenHandler::ShowManagerPasswordError() {
  CallJS("login.LocallyManagedUserCreationScreen.showManagerPasswordError");
}

void LocallyManagedUserCreationScreenHandler::ShowProgressPage() {
  CallJS("login.LocallyManagedUserCreationScreen.showProgressPage");
}

void LocallyManagedUserCreationScreenHandler::ShowUsernamePage() {
  CallJS("login.LocallyManagedUserCreationScreen.showUsernamePage");
}

void LocallyManagedUserCreationScreenHandler::ShowTutorialPage() {
  CallJS("login.LocallyManagedUserCreationScreen.showTutorialPage");
}

void LocallyManagedUserCreationScreenHandler::ShowErrorPage(
    const string16& message,
    bool recoverable) {
  CallJS("login.LocallyManagedUserCreationScreen.showErrorPage", message,
         recoverable);
}

void LocallyManagedUserCreationScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void LocallyManagedUserCreationScreenHandler::
    HandleFinishLocalManagedUserCreation() {
  delegate_->FinishFlow();
}

void LocallyManagedUserCreationScreenHandler::
    HandleAbortLocalManagedUserCreation() {
  delegate_->AbortFlow();
}

void LocallyManagedUserCreationScreenHandler::HandleManagerSelected(
    const std::string& manager_id) {
  if (!delegate_)
    return;
  WallpaperManager::Get()->SetUserWallpaper(manager_id);
}

void LocallyManagedUserCreationScreenHandler::HandleCheckLocallyManagedUserName(
    const string16& name) {
  if (NULL != UserManager::Get()->
          FindLocallyManagedUser(CollapseWhitespace(name, true))) {
    CallJS("login.LocallyManagedUserCreationScreen.managedUserNameError",
           name, l10n_util::GetStringFUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_USERNAME_ALREADY_EXISTS,
               name));
  } else {
    CallJS("login.LocallyManagedUserCreationScreen.managedUserNameOk", name);
  }
}

void LocallyManagedUserCreationScreenHandler::HandleCreateManagedUser(
    const string16& new_raw_user_name,
    const std::string& new_user_password) {
  if (!delegate_)
    return;
  const string16 new_user_name = CollapseWhitespace(new_raw_user_name, true);
  if (NULL != UserManager::Get()->FindLocallyManagedUser(new_user_name)) {
    CallJS("login.LocallyManagedUserCreationScreen.managedUserNameError",
           new_user_name, l10n_util::GetStringFUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_USERNAME_ALREADY_EXISTS,
               new_user_name));
    return;
  }

  // TODO(antrim): Any other password checks here?
  if (new_user_password.length() == 0) {
    CallJS("login.LocallyManagedUserCreationScreen.showPasswordError",
           l10n_util::GetStringUTF16(
               IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_TOO_SHORT));
    return;
  }

  delegate_->CreateManagedUser(new_user_name, new_user_password);
}

void LocallyManagedUserCreationScreenHandler::HandleAuthenticateManager(
    const std::string& raw_manager_username,
    const std::string& manager_password) {
  const std::string manager_username =
      gaia::SanitizeEmail(raw_manager_username);

  UserFlow* flow = new LocallyManagedUserCreationFlow(manager_username);
  UserManager::Get()->SetUserFlow(manager_username, flow);

  delegate_->AuthenticateManager(manager_username, manager_password);
}

}  // namespace chromeos
