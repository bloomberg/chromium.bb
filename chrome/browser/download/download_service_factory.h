// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DownloadService;
class Profile;

// Singleton that owns all DownloadServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated DownloadService.
class DownloadServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the DownloadService for |profile|, creating if not yet created.
  static DownloadService* GetForProfile(Profile* profile);

  static DownloadServiceFactory* GetInstance();

 protected:
  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceHasOwnInstanceInIncognito() const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<DownloadServiceFactory>;

  DownloadServiceFactory();
  virtual ~DownloadServiceFactory();
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_FACTORY_H_
