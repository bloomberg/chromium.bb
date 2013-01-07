// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api_factory.h"

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
MediaGalleriesPrivateAPI* MediaGalleriesPrivateAPIFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaGalleriesPrivateAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
MediaGalleriesPrivateAPIFactory*
    MediaGalleriesPrivateAPIFactory::GetInstance() {
  return Singleton<MediaGalleriesPrivateAPIFactory>::get();
}

MediaGalleriesPrivateAPIFactory::MediaGalleriesPrivateAPIFactory()
    : ProfileKeyedServiceFactory("MediaGalleriesPrivateAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

MediaGalleriesPrivateAPIFactory::~MediaGalleriesPrivateAPIFactory() {
}

ProfileKeyedService* MediaGalleriesPrivateAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new MediaGalleriesPrivateAPI(profile);
}

bool MediaGalleriesPrivateAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool MediaGalleriesPrivateAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
