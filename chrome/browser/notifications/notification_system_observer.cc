// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_system_observer.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/extension.h"

NotificationSystemObserver::NotificationSystemObserver(
    NotificationUIManager* ui_manager)
    : ui_manager_(ui_manager) {
  DCHECK(ui_manager_);
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
}

NotificationSystemObserver::~NotificationSystemObserver() {
}

void NotificationSystemObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_APP_TERMINATING) {
    ui_manager_->CancelAll();
  } else if (type == extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED) {
    if (!content::Source<Profile>(source)->IsOffTheRecord()) {
      extensions::UnloadedExtensionInfo* extension_info =
          content::Details<extensions::UnloadedExtensionInfo>(details).ptr();
      const extensions::Extension* extension = extension_info->extension;
      ui_manager_->CancelAllBySourceOrigin(extension->url());
    }
  } else if (type == chrome::NOTIFICATION_PROFILE_DESTROYED) {
    // We only want to remove the incognito notifications.
    if (content::Source<Profile>(source)->IsOffTheRecord())
      ui_manager_->CancelAllByProfile(content::Source<Profile>(source).ptr());
  } else {
    NOTREACHED();
  }
}
