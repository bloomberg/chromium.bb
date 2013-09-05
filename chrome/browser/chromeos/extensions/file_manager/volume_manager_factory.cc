// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/volume_manager_factory.h"

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/extensions/file_manager/volume_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace file_manager {

VolumeManager* VolumeManagerFactory::Get(content::BrowserContext* context) {
  return static_cast<VolumeManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

VolumeManagerFactory* VolumeManagerFactory::GetInstance() {
  return Singleton<VolumeManagerFactory>::get();
}

content::BrowserContext* VolumeManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Explicitly allow this manager in guest login mode.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool VolumeManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool VolumeManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

BrowserContextKeyedService* VolumeManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  VolumeManager* instance =
      new VolumeManager(Profile::FromBrowserContext(profile),
                        chromeos::disks::DiskMountManager::GetInstance());
  instance->Initialize();
  return instance;
}

VolumeManagerFactory::VolumeManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "VolumeManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {
}

VolumeManagerFactory::~VolumeManagerFactory() {
}

}  // namespace file_manager
