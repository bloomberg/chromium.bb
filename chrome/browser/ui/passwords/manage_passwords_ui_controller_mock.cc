// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"

#include "content/public/browser/web_contents.h"

ManagePasswordsUIControllerMock::ManagePasswordsUIControllerMock(
    content::WebContents* contents)
    : ManagePasswordsUIController(contents),
      navigated_to_settings_page_(false),
      saved_password_(false),
      never_saved_password_(false) {
  contents->SetUserData(UserDataKey(), this);
}

ManagePasswordsUIControllerMock::
    ~ManagePasswordsUIControllerMock() {}

void ManagePasswordsUIControllerMock::
    NavigateToPasswordManagerSettingsPage() {
  navigated_to_settings_page_ = true;
}

const autofill::PasswordForm&
    ManagePasswordsUIControllerMock::PendingCredentials() const {
  return pending_credentials_;
}

void ManagePasswordsUIControllerMock::SetPendingCredentials(
    autofill::PasswordForm pending_credentials) {
  pending_credentials_ = pending_credentials;
}

bool ManagePasswordsUIControllerMock::IsInstalled() const {
  return web_contents()->GetUserData(UserDataKey()) == this;
}

void ManagePasswordsUIControllerMock::SavePasswordInternal() {
  saved_password_ = true;
}

void ManagePasswordsUIControllerMock::NeverSavePasswordInternal() {
  never_saved_password_ = true;
}
