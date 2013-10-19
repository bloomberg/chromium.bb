// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider_observers.h"

#include "base/values.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "content/public/browser/notification_service.h"

using chromeos::WizardController;

namespace {

// Fake screen name for the user session (reported by WizardControllerObserver).
const char kSessionScreenName[] = "session";

}

OOBEWebuiReadyObserver::OOBEWebuiReadyObserver(AutomationProvider* automation)
    : automation_(automation->AsWeakPtr()) {
  if (WizardController::default_controller() &&
      WizardController::default_controller()->current_screen()) {
    OOBEWebuiReady();
  } else {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
  }
}

void OOBEWebuiReadyObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE);
  OOBEWebuiReady();
}

void OOBEWebuiReadyObserver::OOBEWebuiReady() {
  if (automation_)
    automation_->OnOOBEWebuiReady();
  delete this;
}

LoginObserver::LoginObserver(chromeos::ExistingUserController* controller,
                             AutomationProvider* automation,
                             IPC::Message* reply_message)
    : controller_(controller),
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  controller_->set_login_status_consumer(this);
}

LoginObserver::~LoginObserver() {
  controller_->set_login_status_consumer(NULL);
}

void LoginObserver::OnLoginFailure(const chromeos::LoginFailure& error) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetString("error_string", error.GetErrorString());
  AutomationJSONReply(automation_.get(), reply_message_.release())
      .SendSuccess(return_value.get());
  delete this;
}

void LoginObserver::OnLoginSuccess(const chromeos::UserContext& user_context) {
  controller_->set_login_status_consumer(NULL);
  AutomationJSONReply(automation_.get(), reply_message_.release())
      .SendSuccess(NULL);
  delete this;
}

WizardControllerObserver::WizardControllerObserver(
    WizardController* wizard_controller,
    AutomationProvider* automation,
    IPC::Message* reply_message)
    : wizard_controller_(wizard_controller),
      automation_(automation->AsWeakPtr()),
      reply_message_(reply_message) {
  wizard_controller_->AddObserver(this);
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_WEBUI_LOADED,
                 content::NotificationService::AllSources());
}

WizardControllerObserver::~WizardControllerObserver() {
  wizard_controller_->RemoveObserver(this);
}

void WizardControllerObserver::OnScreenChanged(
    chromeos::WizardScreen* next_screen) {
  std::string screen_name = next_screen->GetName();
  if (screen_to_wait_for_.empty() || screen_to_wait_for_ == screen_name) {
    SendReply(screen_name);
  } else {
    DVLOG(2) << "Still waiting for " << screen_to_wait_for_;
  }
}

void WizardControllerObserver::OnSessionStart() {
  SendReply(kSessionScreenName);
}

void WizardControllerObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_LOGIN_WEBUI_LOADED);
  SendReply(WizardController::kLoginScreenName);
}

void WizardControllerObserver::SendReply(const std::string& screen_name) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->SetString("next_screen", screen_name);
  AutomationJSONReply(automation_.get(), reply_message_.release())
      .SendSuccess(return_value.get());
  delete this;
}

ScreenLockUnlockObserver::ScreenLockUnlockObserver(
    AutomationProvider* automation,
    IPC::Message* reply_message,
    bool lock_screen)
    : automation_(automation->AsWeakPtr()),
      reply_message_(reply_message),
      lock_screen_(lock_screen) {
  registrar_.Add(this, chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                 content::NotificationService::AllSources());
}

ScreenLockUnlockObserver::~ScreenLockUnlockObserver() {}

void ScreenLockUnlockObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED);
  if (automation_) {
    AutomationJSONReply reply(automation_.get(), reply_message_.release());
    bool is_screen_locked = *content::Details<bool>(details).ptr();
    if (lock_screen_ == is_screen_locked)
      reply.SendSuccess(NULL);
    else
      reply.SendError("Screen lock failure.");
  }
  delete this;
}

ScreenUnlockObserver::ScreenUnlockObserver(AutomationProvider* automation,
                                           IPC::Message* reply_message)
    : ScreenLockUnlockObserver(automation, reply_message, false) {
  chromeos::ScreenLocker::default_screen_locker()->SetLoginStatusConsumer(this);
}

ScreenUnlockObserver::~ScreenUnlockObserver() {
  chromeos::ScreenLocker* screen_locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (screen_locker)
    screen_locker->SetLoginStatusConsumer(NULL);
}

void ScreenUnlockObserver::OnLoginFailure(const chromeos::LoginFailure& error) {
  if (automation_) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetString("error_string", error.GetErrorString());
    AutomationJSONReply(automation_.get(), reply_message_.release())
        .SendSuccess(return_value.get());
  }
  delete this;
}
