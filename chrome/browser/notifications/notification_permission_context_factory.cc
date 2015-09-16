// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context_factory.h"

#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
NotificationPermissionContext*
NotificationPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<NotificationPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NotificationPermissionContextFactory*
NotificationPermissionContextFactory::GetInstance() {
  return base::Singleton<NotificationPermissionContextFactory>::get();
}

NotificationPermissionContextFactory::NotificationPermissionContextFactory()
    : PermissionContextFactoryBase(
          "NotificationPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {}

NotificationPermissionContextFactory::~NotificationPermissionContextFactory() {}

KeyedService* NotificationPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new NotificationPermissionContext(static_cast<Profile*>(profile));
}
