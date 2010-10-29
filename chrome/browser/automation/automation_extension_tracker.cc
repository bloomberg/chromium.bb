// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_extension_tracker.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"

AutomationExtensionTracker::AutomationExtensionTracker(
    IPC::Message::Sender* automation)
    : AutomationResourceTracker<const Extension*>(automation) {
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                 NotificationService::AllSources());
}

AutomationExtensionTracker::~AutomationExtensionTracker() {
}

void AutomationExtensionTracker::AddObserver(const Extension* resource) {}

void AutomationExtensionTracker::RemoveObserver(const Extension* resource) {}

void AutomationExtensionTracker::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  if (type != NotificationType::EXTENSION_UNLOADED &&
      type != NotificationType::EXTENSION_UNLOADED_DISABLED)
    return;

  const Extension* extension = Details<const Extension>(details).ptr();
  Profile* profile = Source<Profile>(source).ptr();
  if (profile) {
    ExtensionsService* service = profile->GetExtensionsService();
    if (service) {
      // Remove this extension only if it is uninstalled, not just disabled.
      // If it is being uninstalled, the extension will not be in the regular
      // or disabled list.
      if (!service->GetExtensionById(extension->id(), true))
        CloseResource(extension);
    }
  }
}
