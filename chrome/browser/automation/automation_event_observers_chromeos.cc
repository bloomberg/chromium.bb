// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_event_observers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"

LoginEventObserver::LoginEventObserver(
    AutomationEventQueue* event_queue,
    chromeos::ExistingUserController* controller,
    AutomationProvider* automation)
    : AutomationEventObserver(event_queue, false),
      controller_(controller),
      automation_(automation->AsWeakPtr()) {
  controller_->set_login_status_consumer(this);
}

LoginEventObserver::~LoginEventObserver() {}

void LoginEventObserver::OnLoginFailure(const chromeos::LoginFailure& error) {
  _NotifyLoginEvent(error.GetErrorString());
}

void LoginEventObserver::OnLoginSuccess(const std::string& username,
                                        const std::string& password,
                                        bool pending_requests,
                                        bool using_oauth) {
  // Profile changes after login. Ensure AutomationProvider refers to
  // the correct one.
  if (automation_) {
    automation_->set_profile(
        g_browser_process->profile_manager()->GetLastUsedProfile());
  }
  _NotifyLoginEvent(std::string());
}

void LoginEventObserver::_NotifyLoginEvent(const std::string& error_string) {
  DictionaryValue* dict = new DictionaryValue;
  dict->SetString("type", "login_event");
  if (error_string.length())
    dict->SetString("error_string", error_string);
  NotifyEvent(dict);
  controller_->set_login_status_consumer(NULL);
  RemoveIfDone();
}
