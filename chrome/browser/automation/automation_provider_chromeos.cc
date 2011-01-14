// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/chromeos/login/login_screen.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "views/window/window_gtk.h"

void AutomationProvider::LoginWithUserAndPass(const std::string& username,
                                              const std::string& password,
                                              IPC::Message* reply_message) {
  WizardController* controller = WizardController::default_controller();
  chromeos::NewUserView* new_user_view =
        controller->GetLoginScreen()->view();

  new_user_view->SetUsername(username);
  new_user_view->SetPassword(password);

  // Set up an observer (it will delete itself).
  new LoginManagerObserver(this, reply_message);

  new_user_view->Login();
}
