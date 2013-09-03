// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api_factory.h"

#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api.h"
#include "chrome/browser/chromeos/extensions/file_manager/volume_manager_factory.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace file_manager {

// static
FileBrowserPrivateAPI*
FileBrowserPrivateAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<FileBrowserPrivateAPI*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FileBrowserPrivateAPIFactory*
FileBrowserPrivateAPIFactory::GetInstance() {
  return Singleton<FileBrowserPrivateAPIFactory>::get();
}

FileBrowserPrivateAPIFactory::FileBrowserPrivateAPIFactory()
    : BrowserContextKeyedServiceFactory(
          "FileBrowserPrivateAPI",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(VolumeManagerFactory::GetInstance());
}

FileBrowserPrivateAPIFactory::~FileBrowserPrivateAPIFactory() {
}

BrowserContextKeyedService*
FileBrowserPrivateAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new FileBrowserPrivateAPI(static_cast<Profile*>(profile));
}

content::BrowserContext* FileBrowserPrivateAPIFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Explicitly and always allow this router in guest login mode.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool FileBrowserPrivateAPIFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool FileBrowserPrivateAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace file_manager
