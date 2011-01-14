// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"

using chromeos::ExistingUserController;

void AutomationProvider::LoginWithUserAndPass(const std::string& username,
                                              const std::string& password,
                                              IPC::Message* reply_message) {
  ExistingUserController* controller =
      ExistingUserController::current_controller();

  // Set up an observer (it will delete itself).
  new LoginManagerObserver(this, reply_message);

  controller->LoginNewUser(username, password);
}
