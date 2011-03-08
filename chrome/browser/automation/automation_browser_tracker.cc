// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_browser_tracker.h"

#include "content/common/notification_source.h"

AutomationBrowserTracker::AutomationBrowserTracker(
    IPC::Message::Sender* automation)
    : AutomationResourceTracker<Browser*>(automation) {
}

AutomationBrowserTracker::~AutomationBrowserTracker() {}

void AutomationBrowserTracker::AddObserver(Browser* resource) {
  registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                 Source<Browser>(resource));
}

void AutomationBrowserTracker::RemoveObserver(Browser* resource) {
  registrar_.Remove(this, NotificationType::BROWSER_CLOSED,
                    Source<Browser>(resource));
}
