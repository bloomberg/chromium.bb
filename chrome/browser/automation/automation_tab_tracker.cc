// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_tab_tracker.h"

#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_source.h"

using content::NavigationController;

AutomationTabTracker::AutomationTabTracker(IPC::Message::Sender* automation)
    : AutomationResourceTracker<NavigationController*>(automation) {
}

AutomationTabTracker::~AutomationTabTracker() {
}

void AutomationTabTracker::AddObserver(NavigationController* resource) {
  // This tab could either be a regular tab or an external tab
  // Register for both notifications.
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CLOSING,
                 content::Source<NavigationController>(resource));
  registrar_.Add(this, chrome::NOTIFICATION_EXTERNAL_TAB_CLOSED,
                 content::Source<NavigationController>(resource));
}

void AutomationTabTracker::RemoveObserver(NavigationController* resource) {
  registrar_.Remove(this, chrome::NOTIFICATION_TAB_CLOSING,
                    content::Source<NavigationController>(resource));
  registrar_.Remove(this, chrome::NOTIFICATION_EXTERNAL_TAB_CLOSED,
                    content::Source<NavigationController>(resource));
}

void AutomationTabTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTERNAL_TAB_CLOSED:
    case chrome::NOTIFICATION_TAB_CLOSING:
      break;
    default:
      NOTREACHED();
  }
  AutomationResourceTracker<NavigationController*>::Observe(
      type, source, details);
}
