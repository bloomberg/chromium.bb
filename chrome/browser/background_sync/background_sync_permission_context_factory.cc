// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_permission_context_factory.h"

#include "chrome/browser/background_sync/background_sync_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
BackgroundSyncPermissionContext*
BackgroundSyncPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<BackgroundSyncPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

// static
BackgroundSyncPermissionContextFactory*
BackgroundSyncPermissionContextFactory::GetInstance() {
  return base::Singleton<BackgroundSyncPermissionContextFactory>::get();
}

BackgroundSyncPermissionContextFactory::BackgroundSyncPermissionContextFactory()
    : PermissionContextFactoryBase(
          "BackgroundSyncPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* BackgroundSyncPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new BackgroundSyncPermissionContext(static_cast<Profile*>(profile));
}
