// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_api_factory.h"

#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"

namespace extensions {

// static
PushMessagingAPI* PushMessagingAPIFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PushMessagingAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
PushMessagingAPIFactory* PushMessagingAPIFactory::GetInstance() {
  return Singleton<PushMessagingAPIFactory>::get();
}

PushMessagingAPIFactory::PushMessagingAPIFactory()
    : ProfileKeyedServiceFactory("PushMessagingAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

PushMessagingAPIFactory::~PushMessagingAPIFactory() {
}

ProfileKeyedService* PushMessagingAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new PushMessagingAPI(profile);
}

bool PushMessagingAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
