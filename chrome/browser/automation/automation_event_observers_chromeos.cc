// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_event_observers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "content/public/browser/notification_service.h"

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
  _NotifyLoginEvent(error.GetErrorString());
}

void LoginEventObserver::OnLoginSuccess(
    const chromeos::UserCredentials& credentials,
    bool pending_requests,
    bool using_oauth) {
  // Profile changes after login. Ensure AutomationProvider refers to
  // the correct one.
  if (automation_) {
    automation_->set_profile(
        g_browser_process->profile_manager()->GetLastUsedProfile());
  }
  VLOG(1) << "Successfully logged in. Waiting for a page to load";
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void LoginEventObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_STOP) {
    VLOG(1) << "Page load done.";
    _NotifyLoginEvent(std::string());
  }
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
