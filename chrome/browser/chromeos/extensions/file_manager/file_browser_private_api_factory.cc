// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api_factory.h"

#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
FileBrowserPrivateAPI*
FileBrowserPrivateAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<FileBrowserPrivateAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
FileBrowserPrivateAPIFactory*
FileBrowserPrivateAPIFactory::GetInstance() {
  return Singleton<FileBrowserPrivateAPIFactory>::get();
}

FileBrowserPrivateAPIFactory::FileBrowserPrivateAPIFactory()
    : ProfileKeyedServiceFactory(
          "FileBrowserPrivateAPI",
          ProfileDependencyManager::GetInstance()) {
  DependsOn(drive::DriveSystemServiceFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

FileBrowserPrivateAPIFactory::~FileBrowserPrivateAPIFactory() {
}

ProfileKeyedService* FileBrowserPrivateAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new FileBrowserPrivateAPI(static_cast<Profile*>(profile));
}

content::BrowserContext* FileBrowserPrivateAPIFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Explicitly and always allow this router in guest login mode.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool FileBrowserPrivateAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool FileBrowserPrivateAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
