// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_context_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
DurableStoragePermissionContext*
DurableStoragePermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<DurableStoragePermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DurableStoragePermissionContextFactory*
DurableStoragePermissionContextFactory::GetInstance() {
  return base::Singleton<DurableStoragePermissionContextFactory>::get();
}

DurableStoragePermissionContextFactory::DurableStoragePermissionContextFactory()
    : PermissionContextFactoryBase(
          "DurableStoragePermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

KeyedService* DurableStoragePermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new DurableStoragePermissionContext(static_cast<Profile*>(profile));
}
