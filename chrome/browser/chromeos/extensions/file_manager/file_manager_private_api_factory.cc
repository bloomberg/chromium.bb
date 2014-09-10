// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_manager_private_api_factory.h"

#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_private_api.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace file_manager {

// static
FileManagerPrivateAPI*
FileManagerPrivateAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<FileManagerPrivateAPI*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FileManagerPrivateAPIFactory*
FileManagerPrivateAPIFactory::GetInstance() {
  return Singleton<FileManagerPrivateAPIFactory>::get();
}

FileManagerPrivateAPIFactory::FileManagerPrivateAPIFactory()
    : BrowserContextKeyedServiceFactory(
          "FileManagerPrivateAPI",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(VolumeManagerFactory::GetInstance());
}

FileManagerPrivateAPIFactory::~FileManagerPrivateAPIFactory() {
}

KeyedService* FileManagerPrivateAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FileManagerPrivateAPI(Profile::FromBrowserContext(context));
}

content::BrowserContext* FileManagerPrivateAPIFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Explicitly and always allow this router in guest login mode.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool FileManagerPrivateAPIFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool FileManagerPrivateAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace file_manager
