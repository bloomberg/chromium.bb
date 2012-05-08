// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYSTEM_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYSTEM_SERVICE_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace gdata {

class DriveWebAppsRegistry;
class GDataDownloadObserver;
class GDataFileSystem;
class GDataSyncClient;
class GDataUploader;

// GDataSystemService runs the GData system, including the GData file system
// implementation for the file manager, and some other sub systems.
//
// The class is essentially a container that manages lifetime of the objects
// that are used to run the GData system. The GDataSystemService object is
// created per-profile.
class GDataSystemService : public ProfileKeyedService  {
 public:
  // Returns the file system instance.
  GDataFileSystem* file_system() { return file_system_.get(); }

  // Returns the uploader instance.
  GDataUploader* uploader() { return uploader_.get(); }

  // Returns the file system instance.
  DriveWebAppsRegistry* webapps_registry() { return webapps_registry_.get(); }

  // ProfileKeyedService override:
  virtual void Shutdown() OVERRIDE;

 private:
  explicit GDataSystemService(Profile* profile);
  virtual ~GDataSystemService();

  // Initializes the object. This function should be called before any
  // other functions.
  void Initialize();

  friend class GDataSystemServiceFactory;

  Profile* profile_;
  scoped_ptr<GDataFileSystem> file_system_;
  scoped_ptr<GDataUploader> uploader_;
  scoped_ptr<GDataDownloadObserver> download_observer_;
  scoped_ptr<GDataSyncClient> sync_client_;
  scoped_ptr<DriveWebAppsRegistry> webapps_registry_;

  DISALLOW_COPY_AND_ASSIGN(GDataSystemService);
};

// Singleton that owns all GDataSystemServices and associates them with
// Profiles.
class GDataSystemServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the GDataSystemService for |profile|, creating it if it is not
  // yet created.
  static GDataSystemService* GetForProfile(Profile* profile);
  // Returns the GDataSystemService that is already associated with |profile|,
  // if it is not yet created it will return NULL.
  static GDataSystemService* FindForProfile(Profile* profile);

  // Returns the GDataSystemServiceFactory instance.
  static GDataSystemServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GDataSystemServiceFactory>;

  GDataSystemServiceFactory();
  virtual ~GDataSystemServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYSTEM_SERVICE_H_
