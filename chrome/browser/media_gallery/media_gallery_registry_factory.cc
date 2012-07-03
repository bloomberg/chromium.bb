// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/media_gallery_registry_factory.h"

#include "chrome/browser/media_gallery/media_gallery_registry.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
MediaGalleryRegistry* MediaGalleryRegistryFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaGalleryRegistry*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
MediaGalleryRegistryFactory* MediaGalleryRegistryFactory::GetInstance() {
  return Singleton<MediaGalleryRegistryFactory>::get();
}

MediaGalleryRegistryFactory::MediaGalleryRegistryFactory()
    : ProfileKeyedServiceFactory("MediaGalleryRegistry",
                                 ProfileDependencyManager::GetInstance()) {}

MediaGalleryRegistryFactory::~MediaGalleryRegistryFactory() {}

ProfileKeyedService* MediaGalleryRegistryFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new MediaGalleryRegistry(profile);
}

void MediaGalleryRegistryFactory::RegisterUserPrefs(PrefService* prefs) {
  MediaGalleryRegistry::RegisterUserPrefs(prefs);
}

bool MediaGalleryRegistryFactory::ServiceRedirectedInIncognito() {
  return true;
}
