// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERY_REGISTRY_FACTORY_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERY_REGISTRY_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class MediaGalleryRegistry;

// Singleton that owns all MediaGalleryRegistries and associates them with
// Profiles.
class MediaGalleryRegistryFactory : public ProfileKeyedServiceFactory {
 public:
  static MediaGalleryRegistry* GetForProfile(Profile* profile);

  static MediaGalleryRegistryFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<MediaGalleryRegistryFactory>;

  MediaGalleryRegistryFactory();
  virtual ~MediaGalleryRegistryFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefService* prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleryRegistryFactory);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERY_REGISTRY_FACTORY_H_
