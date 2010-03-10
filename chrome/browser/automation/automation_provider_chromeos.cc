// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/chromeos/login/login_manager_view.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "views/window/window_gtk.h"

void AutomationProvider::LoginWithUserAndPass(const std::string& username,
                                              const std::string& password,
                                              IPC::Message* reply_message) {
  WizardController* controller = WizardController::default_controller();
  chromeos::LoginManagerView* login_manager_view =
    reinterpret_cast<chromeos::LoginManagerView*>(
        controller->GetLoginScreen()->view_);

  login_manager_view->SetUsername(username);
  login_manager_view->SetPassword(password);

  // Set up an observer (it will delete itself).
  new LoginManagerObserver(this, reply_message);

  login_manager_view->Login();
}
