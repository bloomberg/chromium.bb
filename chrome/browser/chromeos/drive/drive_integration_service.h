// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_INTEGRATION_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_INTEGRATION_SERVICE_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/drive/drive_notification_observer.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace drive {

class DebugInfoCollector;
class DownloadHandler;
class DriveAppRegistry;
class DriveServiceInterface;
class FileSystemInterface;
class JobListInterface;

namespace internal {
class FileCache;
class ResourceMetadata;
class ResourceMetadataStorage;
}  // namespace internal

// Interface for classes that need to observe events from
// DriveIntegrationService.  All events are notified on UI thread.
class DriveIntegrationServiceObserver {
 public:
  // Triggered when the file system is mounted.
  virtual void OnFileSystemMounted() {
  }

  // Triggered when the file system is being unmounted.
  virtual void OnFileSystemBeingUnmounted() {
  }

 protected:
  virtual ~DriveIntegrationServiceObserver() {}
};

// DriveIntegrationService is used to integrate Drive to Chrome. This class
// exposes the file system representation built on top of Drive and some
// other Drive related objects to the file manager, and some other sub
// systems.
//
// The class is essentially a container that manages lifetime of the objects
// that are used to integrate Drive to Chrome. The object of this class is
// created per-profile.
class DriveIntegrationService
    : public BrowserContextKeyedService,
      public DriveNotificationObserver {
 public:
  class PreferenceWatcher;

  // test_drive_service, test_cache_root and test_file_system are used by tests
  // to inject customized instances.
  // Pass NULL or the empty value when not interested.
  // |preference_watcher| observes the drive enable preference, and sets the
  // enable state when changed. It can be NULL. The ownership is taken by
  // the DriveIntegrationService.
  DriveIntegrationService(
      Profile* profile,
      PreferenceWatcher* preference_watcher,
      DriveServiceInterface* test_drive_service,
      const base::FilePath& test_cache_root,
      FileSystemInterface* test_file_system);
  virtual ~DriveIntegrationService();

  // BrowserContextKeyedService override:
  virtual void Shutdown() OVERRIDE;

  void SetEnabled(bool enabled);
  bool is_enabled() const { return enabled_; }

  bool IsMounted() const;

  // Adds and removes the observer.
  void AddObserver(DriveIntegrationServiceObserver* observer);
  void RemoveObserver(DriveIntegrationServiceObserver* observer);

  // DriveNotificationObserver implementation.
  virtual void OnNotificationReceived() OVERRIDE;
  virtual void OnPushNotificationEnabled(bool enabled) OVERRIDE;

  DriveServiceInterface* drive_service() {
    return drive_service_.get();
  }

  DebugInfoCollector* debug_info_collector() {
    return debug_info_collector_.get();
  }
  FileSystemInterface* file_system() { return file_system_.get(); }
  DownloadHandler* download_handler() { return download_handler_.get(); }
  DriveAppRegistry* drive_app_registry() { return drive_app_registry_.get(); }
  JobListInterface* job_list() { return scheduler_.get(); }

  // Clears all the local cache file, the local resource metadata, and
  // in-memory Drive app registry, and remounts the file system. |callback|
  // is called with true when this operation is done successfully. Otherwise,
  // |callback| is called with false. |callback| must not be null.
  void ClearCacheAndRemountFileSystem(
      const base::Callback<void(bool)>& callback);

 private:
  enum State {
    NOT_INITIALIZED,
    INITIALIZING,
    INITIALIZED,
    REMOUNTING,
  };

  // Returns true if Drive is enabled.
  // Must be called on UI thread.
  bool IsDriveEnabled();

  // Registers remote file system for drive mount point.
  void AddDriveMountPoint();
  // Unregisters drive mount point from File API.
  void RemoveDriveMountPoint();

  // Adds back the drive mount point.
  // Used to implement ClearCacheAndRemountFileSystem().
  void AddBackDriveMountPoint(const base::Callback<void(bool)>& callback,
                              FileError error);

  // Initializes the object. This function should be called before any
  // other functions.
  void Initialize();

  // Called when metadata initialization is done. Continues initialization if
  // the metadata initialization is successful.
  void InitializeAfterMetadataInitialized(FileError error);

  friend class DriveIntegrationServiceFactory;

  Profile* profile_;
  State state_;
  bool enabled_;

  base::FilePath cache_root_directory_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<internal::ResourceMetadataStorage,
             util::DestroyHelper> metadata_storage_;
  scoped_ptr<internal::FileCache, util::DestroyHelper> cache_;
  scoped_ptr<DriveServiceInterface> drive_service_;
  scoped_ptr<JobScheduler> scheduler_;
  scoped_ptr<DriveAppRegistry> drive_app_registry_;
  scoped_ptr<internal::ResourceMetadata,
             util::DestroyHelper> resource_metadata_;
  scoped_ptr<FileSystemInterface> file_system_;
  scoped_ptr<DownloadHandler> download_handler_;
  scoped_ptr<DebugInfoCollector> debug_info_collector_;

  ObserverList<DriveIntegrationServiceObserver> observers_;
  scoped_ptr<PreferenceWatcher> preference_watcher_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveIntegrationService> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveIntegrationService);
};

// Singleton that owns all instances of DriveIntegrationService and
// associates them with Profiles.
class DriveIntegrationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Factory function used by tests.
  typedef base::Callback<DriveIntegrationService*(Profile* profile)>
      FactoryCallback;

  // Returns the DriveIntegrationService for |profile|, creating it if it is
  // not yet created.
  static DriveIntegrationService* GetForProfile(Profile* profile);

  // Same as GetForProfile. TODO(hidehiko): Remove this.
  static DriveIntegrationService* GetForProfileRegardlessOfStates(
      Profile* profile);

  // Returns the DriveIntegrationService that is already associated with
  // |profile|, if it is not yet created it will return NULL.
  static DriveIntegrationService* FindForProfile(Profile* profile);

  // Same as FindForProfile. TODO(hidehiko): Remove this.
  static DriveIntegrationService* FindForProfileRegardlessOfStates(
      Profile* profile);

  // Returns the DriveIntegrationServiceFactory instance.
  static DriveIntegrationServiceFactory* GetInstance();

  // Sets a factory function for tests.
  static void SetFactoryForTest(const FactoryCallback& factory_for_test);

 private:
  friend struct DefaultSingletonTraits<DriveIntegrationServiceFactory>;

  DriveIntegrationServiceFactory();
  virtual ~DriveIntegrationServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  FactoryCallback factory_for_test_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_INTEGRATION_SERVICE_H_
