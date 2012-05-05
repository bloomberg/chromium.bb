// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_event_observers.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"

LoginEventObserver::LoginEventObserver(
    AutomationEventQueue* event_queue,
    chromeos::ExistingUserController* controller)
    : AutomationEventObserver(event_queue, false),
      controller_(controller) {
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
  _NotifyLoginEvent(std::string());
}

void LoginEventObserver::_NotifyLoginEvent(const std::string& error_string) {
  DictionaryValue* dict = new DictionaryValue;
  dict->SetString("type", "login_event");
  dict->SetInteger("observer_id", GetId());
  if (error_string.length())
    dict->SetString("error_string", error_string);
  NotifyEvent(dict);
  controller_->set_login_status_consumer(NULL);
  RemoveIfDone();
}
