// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_tab_tracker.h"

#include "chrome/common/notification_type.h"
#include "chrome/common/notification_source.h"
#include "content/browser/tab_contents/navigation_controller.h"

AutomationTabTracker::AutomationTabTracker(IPC::Message::Sender* automation)
    : AutomationResourceTracker<NavigationController*>(automation) {
}

AutomationTabTracker::~AutomationTabTracker() {
}

void AutomationTabTracker::AddObserver(NavigationController* resource) {
  // This tab could either be a regular tab or an external tab
  // Register for both notifications.
  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(resource));
  registrar_.Add(this, NotificationType::EXTERNAL_TAB_CLOSED,
                 Source<NavigationController>(resource));
  // We also want to know about navigations so we can keep track of the last
  // navigation time.
  registrar_.Add(this, NotificationType::LOAD_STOP,
                 Source<NavigationController>(resource));
}

void AutomationTabTracker::RemoveObserver(NavigationController* resource) {
  registrar_.Remove(this, NotificationType::TAB_CLOSING,
                    Source<NavigationController>(resource));
  registrar_.Remove(this, NotificationType::EXTERNAL_TAB_CLOSED,
                    Source<NavigationController>(resource));
  registrar_.Remove(this, NotificationType::LOAD_STOP,
                    Source<NavigationController>(resource));
}

void AutomationTabTracker::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::LOAD_STOP:
      last_navigation_times_[Source<NavigationController>(source).ptr()] =
          base::Time::Now();
      return;
    case NotificationType::EXTERNAL_TAB_CLOSED:
    case NotificationType::TAB_CLOSING:
      {
        std::map<NavigationController*, base::Time>::iterator iter =
            last_navigation_times_.find(
                Source<NavigationController>(source).ptr());
        if (iter != last_navigation_times_.end())
          last_navigation_times_.erase(iter);
      }
      break;
    default:
      NOTREACHED();
  }
  AutomationResourceTracker<NavigationController*>::Observe(type, source,
                                                            details);
}

base::Time AutomationTabTracker::GetLastNavigationTime(int handle) {
  if (ContainsHandle(handle)) {
    NavigationController* controller = GetResource(handle);
    if (controller) {
      std::map<NavigationController*, base::Time>::const_iterator iter =
          last_navigation_times_.find(controller);
      if (iter != last_navigation_times_.end())
        return iter->second;
    }
  }
  return base::Time();
}
