// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SYSTEM_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SYSTEM_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "sync/notifier/invalidation_handler.h"

namespace base {
class FilePath;
}

namespace google_apis {
class DriveServiceInterface;
class DriveUploader;
}

namespace drive {

class DriveCache;
class DriveDownloadHandler;
class DriveFileSystemInterface;
class DriveFileSystemProxy;
class DriveWebAppsRegistry;
class DriveSyncClient;
class DrivePrefetcher;
class DriveResourceMetadata;
class EventLogger;
class FileWriteHelper;
class StaleCacheFilesRemover;

// DriveSystemService runs the Drive system, including the Drive file system
// implementation for the file manager, and some other sub systems.
//
// The class is essentially a container that manages lifetime of the objects
// that are used to run the Drive system. The DriveSystemService object is
// created per-profile.
class DriveSystemService : public ProfileKeyedService,
                           public syncer::InvalidationHandler {
 public:
  // test_drive_service, test_cache_root and test_file_system are used by tests
  // to inject customized instances.
  // Pass NULL or the empty value when not interested.
  DriveSystemService(Profile* profile,
                     google_apis::DriveServiceInterface* test_drive_service,
                     const base::FilePath& test_cache_root,
                     DriveFileSystemInterface* test_file_system);
  virtual ~DriveSystemService();

  // Initializes the object. This function should be called before any
  // other functions.
  void Initialize();

  // ProfileKeyedService override:
  virtual void Shutdown() OVERRIDE;

  google_apis::DriveServiceInterface* drive_service() {
    return drive_service_.get();
  }

  DriveCache* cache() { return cache_.get(); }
  DriveFileSystemInterface* file_system() { return file_system_.get(); }
  FileWriteHelper* file_write_helper() { return file_write_helper_.get(); }
  DriveDownloadHandler* download_handler() { return download_handler_.get(); }
  google_apis::DriveUploader* uploader() { return uploader_.get(); }
  DriveWebAppsRegistry* webapps_registry() { return webapps_registry_.get(); }
  EventLogger* event_logger() { return event_logger_.get(); }

  // Clears all the local cache files and in-memory data, and remounts the
  // file system. |callback| is called with true when this operation is done
  // successfully. Otherwise, |callback| is called with false.
  // |callback| must not be null.
  void ClearCacheAndRemountFileSystem(
      const base::Callback<void(bool)>& callback);

  // Reloads and remounts the file system.
  void ReloadAndRemountFileSystem();

  // syncer::InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

 private:
  // Returns true if Drive is enabled.
  // Must be called on UI thread.
  bool IsDriveEnabled();

  // Registers remote file system proxy for drive mount point.
  void AddDriveMountPoint();
  // Unregisters drive mount point from File API.
  void RemoveDriveMountPoint();

  // Reinitializes |resource_metadata_|.
  // Used to implement ClearCacheAndRemountFileSystem().
  void ReinitializeResourceMetadataAfterClearCache(
      const base::Callback<void(bool)>& callback,
      bool success);

  // Adds back the drive mount point.
  // Used to implement ClearCacheAndRemountFileSystem().
  void AddBackDriveMountPoint(const base::Callback<void(bool)>& callback,
                              DriveFileError error);

  // Called when cache initialization is done. Continues initialization if
  // the cache initialization is successful.
  void InitializeAfterCacheInitialized(bool success);

  // Called when resource metadata initialization is done. Continues
  // initialization if resource metadata initialization is successful.
  void InitializeAfterResourceMetadataInitialized(DriveFileError error);

  // Disables Drive. Used to disable Drive when needed (ex. initialization of
  // the Drive cache failed).
  // Must be called on UI thread.
  void DisableDrive();

  friend class DriveSystemServiceFactory;

  Profile* profile_;
  // True if Drive is disabled due to initialization errors.
  bool drive_disabled_;

  // True once this is registered to listen to the Drive updates.
  bool push_notification_registered_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<EventLogger> event_logger_;
  scoped_ptr<DriveCache, util::DestroyHelper> cache_;
  scoped_ptr<google_apis::DriveServiceInterface> drive_service_;
  scoped_ptr<google_apis::DriveUploader> uploader_;
  scoped_ptr<DriveWebAppsRegistry> webapps_registry_;
  scoped_ptr<DriveResourceMetadata, util::DestroyHelper> resource_metadata_;
  scoped_ptr<DriveFileSystemInterface> file_system_;
  scoped_ptr<FileWriteHelper> file_write_helper_;
  scoped_ptr<DriveDownloadHandler> download_handler_;
  scoped_ptr<DriveSyncClient> sync_client_;
  scoped_ptr<DrivePrefetcher> prefetcher_;
  scoped_ptr<StaleCacheFilesRemover> stale_cache_files_remover_;
  scoped_refptr<DriveFileSystemProxy> file_system_proxy_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveSystemService> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveSystemService);
};

// Singleton that owns all DriveSystemServices and associates them with
// Profiles.
class DriveSystemServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Factory function used by tests.
  typedef base::Callback<DriveSystemService*(Profile* profile)> FactoryCallback;

  // Returns the DriveSystemService for |profile|, creating it if it is not
  // yet created.
  //
  // This function starts returning NULL if Drive is disabled, even if this
  // function previously returns a non-NULL object. In other words, clients
  // can assume that Drive is enabled if this function returns a non-NULL
  // object.
  static DriveSystemService* GetForProfile(Profile* profile);

  // Similar to GetForProfile(), but returns the instance regardless of if Drive
  // is enabled/disabled.
  static DriveSystemService* GetForProfileRegardlessOfStates(Profile* profile);

  // Returns the DriveSystemService that is already associated with |profile|,
  // if it is not yet created it will return NULL.
  //
  // This function starts returning NULL if Drive is disabled. See also the
  // comment at GetForProfile().
  static DriveSystemService* FindForProfile(Profile* profile);

  // Similar to FindForProfile(), but returns the instance regardless of if
  // Drive is enabled/disabled.
  static DriveSystemService* FindForProfileRegardlessOfStates(Profile* profile);

  // Returns the DriveSystemServiceFactory instance.
  static DriveSystemServiceFactory* GetInstance();

  // Sets a factory function for tests.
  static void SetFactoryForTest(const FactoryCallback& factory_for_test);

 private:
  friend struct DefaultSingletonTraits<DriveSystemServiceFactory>;

  DriveSystemServiceFactory();
  virtual ~DriveSystemServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  FactoryCallback factory_for_test_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SYSTEM_SERVICE_H_
