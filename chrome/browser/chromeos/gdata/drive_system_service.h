// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_SYSTEM_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_SYSTEM_SERVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FilePath;

namespace gdata {

class DriveCache;
class DriveDownloadObserver;
class DriveFileSystemInterface;
class DriveServiceInterface;
class DriveUploader;
class DriveWebAppsRegistry;
class FileWriteHelper;
class DriveSyncClient;
class StaleCacheFilesRemover;

// DriveSystemService runs the Drive system, including the Drive file system
// implementation for the file manager, and some other sub systems.
//
// The class is essentially a container that manages lifetime of the objects
// that are used to run the Drive system. The DriveSystemService object is
// created per-profile.
class DriveSystemService : public ProfileKeyedService  {
 public:
  DriveServiceInterface* drive_service() { return drive_service_.get(); }
  DriveCache* cache() { return cache_; }
  DriveFileSystemInterface* file_system() { return file_system_.get(); }
  FileWriteHelper* file_write_helper() { return file_write_helper_.get(); }
  DriveUploader* uploader() { return uploader_.get(); }
  DriveWebAppsRegistry* webapps_registry() { return webapps_registry_.get(); }

  // Clears all the local cache files and in-memory data, and remounts the file
  // system.
  void ClearCacheAndRemountFileSystem(
      const base::Callback<void(bool)>& callback);

  // ProfileKeyedService override:
  virtual void Shutdown() OVERRIDE;

 private:
  explicit DriveSystemService(Profile* profile);
  virtual ~DriveSystemService();

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

  friend class DriveSystemServiceFactory;

  Profile* profile_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  DriveCache* cache_;
  scoped_ptr<DriveServiceInterface> drive_service_;
  scoped_ptr<DriveUploader> uploader_;
  scoped_ptr<DriveWebAppsRegistry> webapps_registry_;
  scoped_ptr<DriveFileSystemInterface> file_system_;
  scoped_ptr<FileWriteHelper> file_write_helper_;
  scoped_ptr<DriveDownloadObserver> download_observer_;
  scoped_ptr<DriveSyncClient> sync_client_;
  scoped_ptr<StaleCacheFilesRemover> stale_cache_files_remover_;
  base::WeakPtrFactory<DriveSystemService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveSystemService);
};

// Singleton that owns all DriveSystemServices and associates them with
// Profiles.
class DriveSystemServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the DriveSystemService for |profile|, creating it if it is not
  // yet created.
  static DriveSystemService* GetForProfile(Profile* profile);
  // Returns the DriveSystemService that is already associated with |profile|,
  // if it is not yet created it will return NULL.
  static DriveSystemService* FindForProfile(Profile* profile);

  // Returns the DriveSystemServiceFactory instance.
  static DriveSystemServiceFactory* GetInstance();

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
  friend struct DefaultSingletonTraits<DriveSystemServiceFactory>;

  DriveSystemServiceFactory();
  virtual ~DriveSystemServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_SYSTEM_SERVICE_H_
