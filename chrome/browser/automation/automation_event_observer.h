// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_OBSERVER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_OBSERVER_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/automation/automation_event_queue.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

// AutomationEventObserver watches for a specific event, and pushes an
// AutomationEvent into the AutomationEventQueue for each occurance.
class AutomationEventObserver {
 public:
  explicit AutomationEventObserver(AutomationEventQueue* event_queue,
                                   bool recurring);
  virtual ~AutomationEventObserver();

  void Init(int observer_id);
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
// window.domAutomationController.raiseEvent("EVENT_NAME");
class DomRaisedEventObserver
    : public AutomationEventObserver, public content::NotificationObserver {
 public:
  DomRaisedEventObserver(AutomationEventQueue* event_queue,
                         const std::string& event_name,
                         int automation_id,
                         bool recurring);
  virtual ~DomRaisedEventObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  std::string event_name_;
  int automation_id_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DomRaisedEventObserver);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_OBSERVER_H_
