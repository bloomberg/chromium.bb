// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/locally_managed_user_creation_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_flow.h"
#include "chrome/browser/chromeos/login/user_manager.h"
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

void LocallyManagedUserCreationScreenHandler::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "managedUserCreationErrorTitle",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_TITLE));
  localized_strings->SetString(
      "managedUserCreationSuccessTitle",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_SUCCESS_TITLE));
  localized_strings->SetString(
      "managedUserCreationSuccessImageText",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_SUCCESS_IMAGE_TEXT));
  localized_strings->SetString(
      "managedUserCreationSuccessSendEmailInstructionsText",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_SUCCESS_EMAIL_INSTRUCTIONS));
  localized_strings->SetString(
      "managedUserCreationFlowRetryButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_RETRY_BUTTON_TITLE));
  localized_strings->SetString(
      "managedUserCreationFlowCancelButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_CANCEL_BUTTON_TITLE));
  localized_strings->SetString(
      "managedUserCreationFlowFinishButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_SUCCESS_BUTTON_TITLE));
  localized_strings->SetString("createManagedUserNameTitle",
       l10n_util::GetStringUTF16(
           IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_ACCOUNT_NAME_TITLE));
  localized_strings->SetString("createManagedUserPasswordTitle",
       l10n_util::GetStringUTF16(
           IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_TITLE));
  localized_strings->SetString("createManagedUserPasswordHint",
       l10n_util::GetStringUTF16(
           IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_HINT));
  localized_strings->SetString("createManagedUserPasswordConfirmHint",
       l10n_util::GetStringUTF16(
           IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_CONFIRM_HINT));
  localized_strings->SetString("managedUserCreationFlowProceedButtonTitle",
       l10n_util::GetStringUTF16(
           IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_CONTINUE_BUTTON_TEXT));
  localized_strings->SetString("createManagedUserPasswordMismatchError",
       l10n_util::GetStringUTF16(
           IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_MISMATCH_ERROR));

  localized_strings->SetString("createManagedUserSelectManagerTitle",
       l10n_util::GetStringUTF16(
           IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_SELECT_MANAGER_TEXT));
  localized_strings->SetString("createManagedUserManagerPasswordHint",
       l10n_util::GetStringUTF16(
           IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_MANAGER_PASSWORD_HINT));
  localized_strings->SetString("createManagedUserWrongManagerPasswordText",
       l10n_util::GetStringUTF16(
           IDS_CREATE_LOCALLY_MANAGED_USER_MANAGER_PASSWORD_ERROR));
}

void LocallyManagedUserCreationScreenHandler::Initialize() {}

void LocallyManagedUserCreationScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "finishLocalManagedUserCreation",
      base::Bind(&LocallyManagedUserCreationScreenHandler::
                     HandleFinishLocalManagedUserCreation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "abortLocalManagedUserCreation",
      base::Bind(&LocallyManagedUserCreationScreenHandler::
                     HandleAbortLocalManagedUserCreation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "retryLocalManagedUserCreation",
      base::Bind(&LocallyManagedUserCreationScreenHandler::
                     HandleRetryLocalManagedUserCreation,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("checkLocallyManagedUserName",
      base::Bind(&LocallyManagedUserCreationScreenHandler::
                     HandleCheckLocallyManagedUserName,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("runLocallyManagedUserCreationFlow",
      base::Bind(&LocallyManagedUserCreationScreenHandler::
                     HandleRunLocallyManagedUserCreationFlow,
                 base::Unretained(this)));
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

void LocallyManagedUserCreationScreenHandler::ShowInitialScreen() {
  web_ui()->CallJavascriptFunction(
      "login.LocallyManagedUserCreationScreen.showIntialScreen");
}


void LocallyManagedUserCreationScreenHandler::
    ShowManagerInconsistentStateErrorScreen() {
  ShowErrorMessage(
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_MANAGER_INCONSISTENT_STATE),
      false);
}

void LocallyManagedUserCreationScreenHandler::ShowManagerPasswordError() {
  web_ui()->CallJavascriptFunction(
      "login.LocallyManagedUserCreationScreen.showManagerPasswordError");
}

void LocallyManagedUserCreationScreenHandler::ShowProgressScreen() {
  web_ui()->CallJavascriptFunction(
      "login.LocallyManagedUserCreationScreen.showProgressScreen");
}

void LocallyManagedUserCreationScreenHandler::ShowSuccessMessage() {
  web_ui()->CallJavascriptFunction(
      "login.LocallyManagedUserCreationScreen.showFinishedMessage");
}

void LocallyManagedUserCreationScreenHandler::ShowErrorMessage(
    string16 message,
    bool recoverable) {
  web_ui()->CallJavascriptFunction(
      "login.LocallyManagedUserCreationScreen.showErrorMessage",
      base::StringValue(message),
      base::FundamentalValue(recoverable));
}

void LocallyManagedUserCreationScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void LocallyManagedUserCreationScreenHandler::
    HandleFinishLocalManagedUserCreation(const base::ListValue* args) {
  delegate_->FinishFlow();
}

void LocallyManagedUserCreationScreenHandler::
    HandleRetryLocalManagedUserCreation(const base::ListValue* args) {
  delegate_->RetryLastStep();
}

void LocallyManagedUserCreationScreenHandler::
    HandleAbortLocalManagedUserCreation(const base::ListValue* args) {
  delegate_->AbortFlow();
}

void LocallyManagedUserCreationScreenHandler::HandleCheckLocallyManagedUserName(
    const base::ListValue* args) {
  DCHECK(args && args->GetSize() == 1);

  string16 name;
  if (!args->GetString(0, &name)) {
    NOTREACHED();
    return;
  }
  if (NULL != UserManager::Get()->
          FindLocallyManagedUser(CollapseWhitespace(name, true))) {
    web_ui()->CallJavascriptFunction(
        "login.LocallyManagedUserCreationScreen.managedUserNameError",
        base::StringValue(name),
        base::StringValue(l10n_util::GetStringFUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_USERNAME_ALREADY_EXISTS,
            name)));
  } else {
    web_ui()->CallJavascriptFunction(
        "login.LocallyManagedUserCreationScreen.managedUserNameOk",
        base::StringValue(name));
  }
}

void LocallyManagedUserCreationScreenHandler::
    HandleRunLocallyManagedUserCreationFlow(const base::ListValue* args) {
  if (!delegate_)
    return;
  DCHECK(args && args->GetSize() == 4);

  string16 new_user_name;
  std::string new_user_password;
  std::string manager_username;
  std::string manager_password;
  if (!args->GetString(0, &new_user_name) ||
      !args->GetString(1, &new_user_password) ||
      !args->GetString(2, &manager_username) ||
      !args->GetString(3, &manager_password)) {
    NOTREACHED();
    return;
  }

  new_user_name = CollapseWhitespace(new_user_name, true);
  if (NULL != UserManager::Get()->FindLocallyManagedUser(new_user_name)) {
    web_ui()->CallJavascriptFunction(
        "login.LocallyManagedUserCreationScreen.managedUserNameError",
        base::StringValue(new_user_name),
        base::StringValue(l10n_util::GetStringFUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_USERNAME_ALREADY_EXISTS,
            new_user_name)));
    return;
  }

  // TODO(antrim): Any other password checks here?
  if (new_user_password.length() == 0) {
    web_ui()->CallJavascriptFunction(
        "login.LocallyManagedUserCreationScreen.showPasswordError",
        base::StringValue(l10n_util::GetStringUTF16(
            IDS_CREATE_LOCALLY_MANAGED_USER_CREATE_PASSWORD_TOO_SHORT)));
    return;
  }

  manager_username = gaia::SanitizeEmail(manager_username);

  UserFlow* flow =
      new LocallyManagedUserCreationFlow(manager_username,
                                         new_user_name,
                                         new_user_password);
  UserManager::Get()->SetUserFlow(manager_username, flow);

  delegate_->RunFlow(new_user_name, new_user_password,
                     manager_username, manager_password);
}

}  // namespace chromeos
