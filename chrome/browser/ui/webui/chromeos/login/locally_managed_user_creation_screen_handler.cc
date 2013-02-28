// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/locally_managed_user_creation_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Locally managed user creation screen id.
const char kLocallyManagedUserCreationScreen[] =
    "locally-managed-user-creation";

}  // namespace

namespace chromeos {

LocallyManagedUserCreationScreenHandler::
    LocallyManagedUserCreationScreenHandler() {
}

LocallyManagedUserCreationScreenHandler::
    ~LocallyManagedUserCreationScreenHandler() {
}

void LocallyManagedUserCreationScreenHandler::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "managedUserCreationErrorTitle",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_TITLE));
  localized_strings->SetString(
      "managedUserCreationSuccessMessage",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_SUCCESS_MESSAGE));
  localized_strings->SetString(
      "managedUserCreationFlowCancelButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_ERROR_CANCEL_BUTTON_TITLE));
  localized_strings->SetString(
      "managedUserCreationFlowFinishButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_CREATE_LOCALLY_MANAGED_USER_CREATION_SUCCESS_BUTTON_TITLE));
}

void LocallyManagedUserCreationScreenHandler::Initialize() {
}

void LocallyManagedUserCreationScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("finishLocalManagedUserCreation",
      base::Bind(&LocallyManagedUserCreationScreenHandler::
                     HandleFinishLocalManagedUserCreation,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("abortLocalManagedUserCreation",
      base::Bind(&LocallyManagedUserCreationScreenHandler::
                     HandleAbortLocalManagedUserCreation,
                 base::Unretained(this)));
}

void LocallyManagedUserCreationScreenHandler::PrepareToShow() {
}

void LocallyManagedUserCreationScreenHandler::Show() {
  ShowScreen(OobeUI::kScreenManagedUserCreationFlow, NULL);
}

void LocallyManagedUserCreationScreenHandler::Hide() {
}

void LocallyManagedUserCreationScreenHandler::ShowSuccessMessage() {
  web_ui()->CallJavascriptFunction(
      "login.LocallyManagedUserCreationScreen.showFinishedMessage");
}

void LocallyManagedUserCreationScreenHandler::ShowErrorMessage(
    string16 message) {
  web_ui()->CallJavascriptFunction(
      "login.LocallyManagedUserCreationScreen.showErrorMessage",
      base::StringValue(message));
}

void LocallyManagedUserCreationScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void LocallyManagedUserCreationScreenHandler::
    HandleFinishLocalManagedUserCreation(const base::ListValue* args) {
  delegate_->FinishFlow();
}

void LocallyManagedUserCreationScreenHandler::
    HandleAbortLocalManagedUserCreation(const base::ListValue* args) {
  delegate_->AbortFlow();
}

}  // namespace chromeos
