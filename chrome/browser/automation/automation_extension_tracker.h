// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_TRACKER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_TRACKER_H_
#pragma once

#include "chrome/browser/automation/automation_resource_tracker.h"

class Extension;

// Tracks an Extension. An Extension is removed on uninstall, not on disable.
class AutomationExtensionTracker
    : public AutomationResourceTracker<const Extension*> {
 public:
  explicit AutomationExtensionTracker(IPC::Message::Sender* automation);

  virtual ~AutomationExtensionTracker();

  // This is empty because we do not want to add an observer for every
  // extension added to the tracker. This is because the profile, not the
  // extension, is the one who sends the notification about extension
  // uninstalls. Instead of using this method, one observer is added for all
  // extensions in the constructor.
  virtual void AddObserver(const Extension* resource);

  // See related comment above as to why this method is empty.
  virtual void RemoveObserver(const Extension* resource);

  // Overriding AutomationResourceTracker Observe. AutomationResourceTracker's
  // Observe expects the NotificationSource to be the object that is closing.
  // This is not true for the relevant extension notifications, so we have to
  // the observation ourselves.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_TRACKER_H_
