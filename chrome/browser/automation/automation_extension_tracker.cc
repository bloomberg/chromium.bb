// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_extension_tracker.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/common/notification_service.h"

AutomationExtensionTracker::AutomationExtensionTracker(
    IPC::Message::Sender* automation)
    : AutomationResourceTracker<const Extension*>(automation) {
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
}

AutomationExtensionTracker::~AutomationExtensionTracker() {
}

void AutomationExtensionTracker::AddObserver(const Extension* resource) {}

void AutomationExtensionTracker::RemoveObserver(const Extension* resource) {}

void AutomationExtensionTracker::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  if (type != NotificationType::EXTENSION_UNLOADED) {
    NOTREACHED();
    return;
  }
  UnloadedExtensionInfo* info = Details<UnloadedExtensionInfo>(details).ptr();
  const Extension* extension = info->extension;
  Profile* profile = Source<Profile>(source).ptr();
  if (profile) {
    ExtensionService* service = profile->GetExtensionService();
    if (service && info->reason == UnloadedExtensionInfo::UNINSTALL) {
      // Remove this extension only if it is uninstalled, not just disabled.
      CloseResource(extension);
    }
  }
}
