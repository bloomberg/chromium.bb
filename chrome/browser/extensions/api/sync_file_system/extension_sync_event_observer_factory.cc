// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer_factory.h"

#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"

namespace extensions {

// static
ExtensionSyncEventObserver*
ExtensionSyncEventObserverFactory::GetForProfile(Profile* profile) {
  return static_cast<ExtensionSyncEventObserver*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ExtensionSyncEventObserverFactory*
ExtensionSyncEventObserverFactory::GetInstance() {
  return Singleton<ExtensionSyncEventObserverFactory>::get();
}

ExtensionSyncEventObserverFactory::ExtensionSyncEventObserverFactory()
    : ProfileKeyedServiceFactory("ExtensionSyncEventObserver",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(sync_file_system::SyncFileSystemServiceFactory::GetInstance());
  DependsOn(ExtensionSystemFactory::GetInstance());
}

ExtensionSyncEventObserverFactory::~ExtensionSyncEventObserverFactory() {}

ProfileKeyedService* ExtensionSyncEventObserverFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ExtensionSyncEventObserver(static_cast<Profile*>(profile));
}

}  // namespace extensions
