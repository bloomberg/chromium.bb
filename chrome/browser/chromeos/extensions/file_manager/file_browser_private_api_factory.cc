// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api_factory.h"

#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

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
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(VolumeManagerFactory::GetInstance());
}

FileBrowserPrivateAPIFactory::~FileBrowserPrivateAPIFactory() {
}

KeyedService* FileBrowserPrivateAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FileBrowserPrivateAPI(Profile::FromBrowserContext(context));
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
