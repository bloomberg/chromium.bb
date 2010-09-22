// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_window_tracker.h"

#include "chrome/common/native_window_notification_source.h"

AutomationWindowTracker::AutomationWindowTracker(
    IPC::Message::Sender* automation)
    : AutomationResourceTracker<gfx::NativeWindow>(automation) {
}

AutomationWindowTracker::~AutomationWindowTracker() {
}

void AutomationWindowTracker::AddObserver(gfx::NativeWindow resource) {
  registrar_.Add(this, NotificationType::WINDOW_CLOSED,
                 Source<gfx::NativeWindow>(resource));
}

void AutomationWindowTracker::RemoveObserver(gfx::NativeWindow resource) {
  registrar_.Remove(this, NotificationType::WINDOW_CLOSED,
                    Source<gfx::NativeWindow>(resource));
}
