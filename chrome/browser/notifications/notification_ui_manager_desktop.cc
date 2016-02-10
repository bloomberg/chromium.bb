// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager.h"

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/message_center_settings_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

// static
NotificationUIManager* NotificationUIManager::Create(PrefService* local_state) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;

  scoped_ptr<message_center::NotifierSettingsProvider> settings_provider(
      new MessageCenterSettingsController(
          profile_manager->GetProfileAttributesStorage()));
  return new MessageCenterNotificationManager(
      g_browser_process->message_center(),
      std::move(settings_provider));
}
