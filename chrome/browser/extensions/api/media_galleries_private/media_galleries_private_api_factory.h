// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERIES_PRIVATE_API_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERIES_PRIVATE_API_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace extensions {

class MediaGalleriesPrivateAPI;

// Singleton that associate MediaGalleriesPrivateAPI objects with Profiles.
class MediaGalleriesPrivateAPIFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the MediaGalleriesPrivateAPI for |profile|, creating it if
  // it is not yet created.
  static MediaGalleriesPrivateAPI* GetForProfile(Profile* profile);

  static MediaGalleriesPrivateAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<MediaGalleriesPrivateAPIFactory>;

  // Use GetInstance().
  MediaGalleriesPrivateAPIFactory();
  virtual ~MediaGalleriesPrivateAPIFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPrivateAPIFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_MEDIA_GALLERIES_PRIVATE_API_FACTORY_H_
