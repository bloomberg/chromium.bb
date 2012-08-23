// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYSTEM_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYSTEM_SERVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FilePath;

namespace gdata {

class DriveCache;
class DriveFileSystemInterface;
class DriveServiceInterface;
class DriveWebAppsRegistry;
class FileWriteHelper;
class GDataContactsService;
class GDataDownloadObserver;
class GDataSyncClient;
class GDataUploader;

// GDataSystemService runs the GData system, including the Drive file system
// implementation for the file manager, and some other sub systems.
//
// The class is essentially a container that manages lifetime of the objects
// that are used to run the GData system. The GDataSystemService object is
// created per-profile.
class GDataSystemService : public ProfileKeyedService  {
 public:
  DriveServiceInterface* drive_service() { return drive_service_.get(); }
  DriveCache* cache() { return cache_; }
  DriveFileSystemInterface* file_system() { return file_system_.get(); }
  FileWriteHelper* file_write_helper() { return file_write_helper_.get(); }
  GDataUploader* uploader() { return uploader_.get(); }
  GDataContactsService* contacts_service() { return contacts_service_.get(); }
  DriveWebAppsRegistry* webapps_registry() { return webapps_registry_.get(); }

  // Clears all the local cache files and in-memory data, and remounts the file
  // system.
  void ClearCacheAndRemountFileSystem(
      const base::Callback<void(bool)>& callback);

  // ProfileKeyedService override:
  virtual void Shutdown() OVERRIDE;

 private:
  explicit GDataSystemService(Profile* profile);
  virtual ~GDataSystemService();

  // Initializes the object. This function should be called before any
  // other functions.
  void Initialize(DriveServiceInterface* drive_service,
                  const FilePath& cache_root);

  // Registers remote file system proxy for drive mount point.
  void AddDriveMountPoint();
  // Unregisters drive mount point from File API.
  void RemoveDriveMountPoint();

  // Adds back the drive mount point. Used to implement ClearCache().
  void AddBackDriveMountPoint(const base::Callback<void(bool)>& callback,
                              DriveFileError error,
                              const FilePath& file_path);

  friend class GDataSystemServiceFactory;

  Profile* profile_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  DriveCache* cache_;
  scoped_ptr<DriveServiceInterface> drive_service_;
  scoped_ptr<GDataUploader> uploader_;
  scoped_ptr<DriveWebAppsRegistry> webapps_registry_;
  scoped_ptr<DriveFileSystemInterface> file_system_;
  scoped_ptr<FileWriteHelper> file_write_helper_;
  scoped_ptr<GDataDownloadObserver> download_observer_;
  scoped_ptr<GDataSyncClient> sync_client_;
  scoped_ptr<GDataContactsService> contacts_service_;
  base::WeakPtrFactory<GDataSystemService> weak_ptr_factory_;

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

  // Sets drive service that should be used to initialize file system in test.
  // Should be called before the service is created.
  // Please, make sure |drive_service| gets deleted if no system service is
  // created (e.g. by calling this method with NULL).
  static void set_drive_service_for_test(DriveServiceInterface* drive_service);

  // Sets root path for the cache used in test. Should be called before the
  // service is created.
  // If |cache_root| is not empty, new string object will be created. Please,
  // make sure it gets deleted if no system service is created (e.g. by calling
  // this method with empty string).
  static void set_cache_root_for_test(const std::string& cache_root);

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
