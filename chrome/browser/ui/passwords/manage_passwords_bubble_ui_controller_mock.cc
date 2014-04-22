// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller_mock.h"

#include "content/public/browser/web_contents.h"

ManagePasswordsBubbleUIControllerMock::ManagePasswordsBubbleUIControllerMock(
    content::WebContents* contents)
    : ManagePasswordsBubbleUIController(contents),
      navigated_to_settings_page_(false),
      saved_password_(false),
      never_saved_password_(false) {
  contents->SetUserData(UserDataKey(), this);
}

ManagePasswordsBubbleUIControllerMock::
    ~ManagePasswordsBubbleUIControllerMock() {}

void ManagePasswordsBubbleUIControllerMock::
    NavigateToPasswordManagerSettingsPage() {
  navigated_to_settings_page_ = true;
}

const autofill::PasswordForm&
    ManagePasswordsBubbleUIControllerMock::PendingCredentials() const {
  return pending_credentials_;
}

bool ManagePasswordsBubbleUIControllerMock::IsInstalled() const {
  return web_contents()->GetUserData(UserDataKey()) == this;
}

void ManagePasswordsBubbleUIControllerMock::SavePassword() {
  saved_password_ = true;
}

void ManagePasswordsBubbleUIControllerMock::NeverSavePassword() {
  never_saved_password_ = true;
}
