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
  explicit AutomationEventObserver(AutomationEventQueue* event_queue);
  virtual ~AutomationEventObserver();

  void Init(int observer_id);
  void NotifyEvent(DictionaryValue* value);
  int GetId() const;

 private:
  AutomationEventQueue* event_queue_;
  int observer_id_;

  DISALLOW_COPY_AND_ASSIGN(AutomationEventObserver);
};

// AutomationEventObserver implementation that listens for explicitly raised
// events. A webpage currently raises events by calling:
// window.domAutomationController.setAutomationId(42); // Any integer works.
// window.domAutomationController("EVENT_NAME");
// TODO(craigdh): This method is a temporary hack.
class DomRaisedEventObserver
    : public AutomationEventObserver, public DomOperationObserver {
 public:
  DomRaisedEventObserver(AutomationEventQueue* event_queue,
                         const std::string& event_name);
  virtual ~DomRaisedEventObserver();

  virtual void OnDomOperationCompleted(const std::string& json) OVERRIDE;
  virtual void OnModalDialogShown() OVERRIDE;
  virtual void OnJavascriptBlocked() OVERRIDE;

 private:
  std::string event_name_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DomRaisedEventObserver);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_OBSERVER_H_
