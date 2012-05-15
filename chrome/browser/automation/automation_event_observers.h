// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_OBSERVERS_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_OBSERVERS_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/automation/automation_event_queue.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#endif  // defined(OS_CHROMEOS)
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#if defined(OS_CHROMEOS)
namespace chromeos {
class ExistingUserController;
}
#endif  // defined(OS_CHROMEOS)

// AutomationEventObserver watches for a specific event, and pushes an
// AutomationEvent into the AutomationEventQueue for each occurance.
class AutomationEventObserver {
 public:
  explicit AutomationEventObserver(AutomationEventQueue* event_queue,
                                   bool recurring);
  virtual ~AutomationEventObserver();

  virtual void Init(int observer_id);
  void NotifyEvent(DictionaryValue* value);
  int GetId() const;

 protected:
  void RemoveIfDone();  // This may delete the object.

 private:
  AutomationEventQueue* event_queue_;
  bool recurring_;
  int observer_id_;
  // TODO(craigdh): Add a PyAuto hook to retrieve the number of times an event
  // has occurred.
  int event_count_;

  DISALLOW_COPY_AND_ASSIGN(AutomationEventObserver);
};

// AutomationEventObserver implementation that listens for explicitly raised
// events. A webpage explicitly raises events by calling:
// window.domAutomationController.sendWithId(automation_id, event_name);
class DomEventObserver
    : public AutomationEventObserver, public content::NotificationObserver {
 public:
  DomEventObserver(AutomationEventQueue* event_queue,
                   const std::string& event_name,
                   int automation_id,
                   bool recurring);
  virtual ~DomEventObserver();

  virtual void Init(int observer_id) OVERRIDE;
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  std::string event_name_;
  std::string event_name_base_;
  int automation_id_;
  content::NotificationRegistrar registrar_;

  // The first instance of this string in an event name will be replaced with
  // the id of this observer.
  static const char* kSubstringReplaceWithObserverId;

  DISALLOW_COPY_AND_ASSIGN(DomEventObserver);
};

#if defined(OS_CHROMEOS)

// Event observer that listens for the completion of login.
class LoginEventObserver
    : public AutomationEventObserver, public chromeos::LoginStatusConsumer {
 public:
  LoginEventObserver(AutomationEventQueue* event_queue,
                     chromeos::ExistingUserController* controller,
                     AutomationProvider* automation);
  virtual ~LoginEventObserver();

  virtual void OnLoginFailure(const chromeos::LoginFailure& error) OVERRIDE;

  virtual void OnLoginSuccess(const std::string& username,
                              const std::string& password,
                              bool pending_requests, bool using_oauth) OVERRIDE;

 private:
  chromeos::ExistingUserController* controller_;
  base::WeakPtr<AutomationProvider> automation_;

  void _NotifyLoginEvent(const std::string& error_string);

  DISALLOW_COPY_AND_ASSIGN(LoginEventObserver);
};

#endif  // defined(OS_CHROMEOS)

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_OBSERVERS_H_
