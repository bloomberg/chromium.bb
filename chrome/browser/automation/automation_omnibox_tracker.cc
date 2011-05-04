// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_omnibox_tracker.h"

#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

AutomationOmniboxTracker::AutomationOmniboxTracker(
    IPC::Message::Sender* automation)
    : AutomationResourceTracker<OmniboxView*>(automation) {
}

AutomationOmniboxTracker::~AutomationOmniboxTracker() {
}

void AutomationOmniboxTracker::AddObserver(OmniboxView* resource) {
  registrar_.Add(this, NotificationType::OMNIBOX_DESTROYED,
                 Source<OmniboxView>(resource));
}

void AutomationOmniboxTracker::RemoveObserver(OmniboxView* resource) {
  registrar_.Remove(this, NotificationType::OMNIBOX_DESTROYED,
                    Source<OmniboxView>(resource));
}
