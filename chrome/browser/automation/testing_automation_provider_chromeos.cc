// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "base/values.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"

void TestingAutomationProvider::LoginAsGuest(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  // Set up an observer (it will delete itself).
  new LoginManagerObserver(this, reply_message);
  controller->LoginAsGuest();
}

void TestingAutomationProvider::Login(DictionaryValue* args,
                                      IPC::Message* reply_message) {
  std::string username, password;
  if (!args->GetString("username", &username) ||
      !args->GetString("password", &password)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid or missing args");
    return;
  }

  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  // Set up an observer (it will delete itself).
  new LoginManagerObserver(this, reply_message);
  controller->Login(username, password);
}

// Logging out could have other undesirable side effects: session_manager is
// killed, so its children, including chrome and the window manager will also
// be killed. SSH connections might be closed, the test binary might be killed.
void TestingAutomationProvider::Logout(DictionaryValue* args,
                                       IPC::Message* reply_message) {
  // Send success before stopping session because if we're a child of
  // session manager then we'll die when the session is stopped.
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  if (chromeos::CrosLibrary::Get()->EnsureLoaded())
    chromeos::CrosLibrary::Get()->GetLoginLibrary()->StopSession("");
}

void TestingAutomationProvider::ScreenLock(DictionaryValue* args,
                                           IPC::Message* reply_message) {
  new ScreenLockUnlockObserver(this, reply_message, true);
  chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->
      NotifyScreenLockRequested();
}

void TestingAutomationProvider::ScreenUnlock(DictionaryValue* args,
                                             IPC::Message* reply_message) {
  new ScreenLockUnlockObserver(this, reply_message, false);
  chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->
      NotifyScreenUnlockRequested();
}
