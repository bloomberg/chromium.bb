// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_tab_tracker.h"

#include "content/browser/tab_contents/navigation_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"

AutomationTabTracker::AutomationTabTracker(IPC::Message::Sender* automation)
    : AutomationResourceTracker<content::NavigationController*>(automation) {
}

AutomationTabTracker::~AutomationTabTracker() {
}

void AutomationTabTracker::AddObserver(
    content::NavigationController* resource) {
  // This tab could either be a regular tab or an external tab
  // Register for both notifications.
  registrar_.Add(this, content::NOTIFICATION_TAB_CLOSING,
                 content::Source<content::NavigationController>(resource));
  registrar_.Add(this, chrome::NOTIFICATION_EXTERNAL_TAB_CLOSED,
                 content::Source<content::NavigationController>(resource));
  // We also want to know about navigations so we can keep track of the last
  // navigation time.
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<content::NavigationController>(resource));
}

void AutomationTabTracker::RemoveObserver(
    content::NavigationController* resource) {
  registrar_.Remove(this, content::NOTIFICATION_TAB_CLOSING,
                    content::Source<content::NavigationController>(resource));
  registrar_.Remove(this, chrome::NOTIFICATION_EXTERNAL_TAB_CLOSED,
                    content::Source<content::NavigationController>(resource));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_STOP,
                    content::Source<content::NavigationController>(resource));
}

void AutomationTabTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_STOP:
      last_navigation_times_[
          content::Source<content::NavigationController>(source).ptr()] =
              base::Time::Now();
      return;
    case chrome::NOTIFICATION_EXTERNAL_TAB_CLOSED:
    case content::NOTIFICATION_TAB_CLOSING:
      {
        std::map<content::NavigationController*, base::Time>::iterator iter =
            last_navigation_times_.find(
                content::Source<content::NavigationController>(source).ptr());
        if (iter != last_navigation_times_.end())
          last_navigation_times_.erase(iter);
      }
      break;
    default:
      NOTREACHED();
  }
  AutomationResourceTracker<content::NavigationController*>::Observe(
      type, source, details);
}

base::Time AutomationTabTracker::GetLastNavigationTime(int handle) {
  if (ContainsHandle(handle)) {
    content::NavigationController* controller = GetResource(handle);
    if (controller) {
      std::map<content::NavigationController*, base::Time>::const_iterator iter
          = last_navigation_times_.find(controller);
      if (iter != last_navigation_times_.end())
        return iter->second;
    }
  }
  return base::Time();
}
