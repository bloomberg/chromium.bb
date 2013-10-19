// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_event_observers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"

using chromeos::ExistingUserController;

LoginEventObserver::LoginEventObserver(
    AutomationEventQueue* event_queue,
    AutomationProvider* automation)
    : AutomationEventObserver(event_queue, false),
      automation_(automation->AsWeakPtr()) {
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  DCHECK(controller);
  controller->set_login_status_consumer(this);
}

LoginEventObserver::~LoginEventObserver() {}

void LoginEventObserver::OnLoginFailure(const chromeos::LoginFailure& error) {
  VLOG(1) << "Login failed, error=" << error.GetErrorString();
  _NotifyLoginEvent(error.GetErrorString());
}

void LoginEventObserver::OnLoginSuccess(
    const chromeos::UserContext& user_context) {
  // Profile changes after login. Ensure AutomationProvider refers to
  // the correct one.
  if (automation_) {
    automation_->set_profile(
        g_browser_process->profile_manager()->GetLastUsedProfile());
  }
  VLOG(1) << "Successfully logged in.";
  _NotifyLoginEvent(std::string());
}

void LoginEventObserver::_NotifyLoginEvent(const std::string& error_string) {
  DictionaryValue* dict = new DictionaryValue;
  dict->SetString("type", "login_event");
  if (error_string.length())
    dict->SetString("error_string", error_string);
  NotifyEvent(dict);
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  if (controller)
    controller->set_login_status_consumer(NULL);
  RemoveIfDone();
}
