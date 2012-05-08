// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_EVENT_ROUTER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/files/file_path_watcher.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

class FileBrowserNotifications;
class Profile;

// Used to monitor disk mount changes and signal when new mounted usb device is
// found.
class FileBrowserEventRouter
    : public RefcountedProfileKeyedService,
      public chromeos::disks::DiskMountManager::Observer,
      public gdata::GDataOperationRegistry::Observer,
      public gdata::GDataFileSystem::Observer {
 public:
  // RefcountedProfileKeyedService overrides.
  virtual void ShutdownOnUIThread() OVERRIDE;

  // Starts observing file system change events. Currently only
  // CrosDisksClient events are being observed.
  void ObserveFileSystemEvents();

  // File watch setup routines.
  bool AddFileWatch(const FilePath& file_path,
                    const FilePath& virtual_path,
                    const std::string& extension_id);
  void RemoveFileWatch(const FilePath& file_path,
                       const std::string& extension_id);

  // CrosDisksClient::Observer overrides.
  virtual void DiskChanged(chromeos::disks::DiskMountManagerEventType event,
                           const chromeos::disks::DiskMountManager::Disk* disk)
      OVERRIDE;
  virtual void DeviceChanged(chromeos::disks::DiskMountManagerEventType event,
                             const std::string& device_path) OVERRIDE;
  virtual void MountCompleted(
      chromeos::disks::DiskMountManager::MountEvent event_type,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info)
      OVERRIDE;

  // GDataOperationRegistry::Observer overrides.
  virtual void OnProgressUpdate(
      const std::vector<gdata::GDataOperationRegistry::ProgressStatus>& list)
          OVERRIDE;

  // gdata::GDataFileSystem::Observer overrides.
  virtual void OnDirectoryChanged(const FilePath& directory_path) OVERRIDE;
  virtual void OnDocumentFeedFetched(int num_accumulated_entries) OVERRIDE;

 private:
  friend class FileBrowserEventRouterFactory;

  // Helper class for passing through file watch notification events.
  class FileWatcherDelegate : public base::files::FilePathWatcher::Delegate {
   public:
    explicit FileWatcherDelegate(FileBrowserEventRouter* router);

   private:
    // base::files::FilePathWatcher::Delegate overrides.
    virtual void OnFilePathChanged(const FilePath& path) OVERRIDE;
    virtual void OnFilePathError(const FilePath& path) OVERRIDE;

    void HandleFileWatchOnUIThread(const FilePath& local_path, bool got_error);

    FileBrowserEventRouter* router_;
  };

  typedef std::map<std::string, int> ExtensionUsageRegistry;

  class FileWatcherExtensions {
   public:
    FileWatcherExtensions(const FilePath& path,
        const std::string& extension_id,
        bool is_remote_file_system);

    ~FileWatcherExtensions() {}

    void AddExtension(const std::string& extension_id);

    void RemoveExtension(const std::string& extension_id);

    const ExtensionUsageRegistry& GetExtensions() const;

    unsigned int GetRefCount() const;

    const FilePath& GetVirtualPath() const;

    bool Watch(const FilePath& path, FileWatcherDelegate* delegate);

   private:
    linked_ptr<base::files::FilePathWatcher> file_watcher_;
    FilePath local_path_;
    FilePath virtual_path_;
    ExtensionUsageRegistry extensions_;
    unsigned int ref_count_;
    bool is_remote_file_system_;
  };

  typedef std::map<FilePath, FileWatcherExtensions*> WatcherMap;

  explicit FileBrowserEventRouter(Profile* profile);
  virtual ~FileBrowserEventRouter();

  // USB mount event handlers.
  void OnDiskAdded(const chromeos::disks::DiskMountManager::Disk* disk);
  void OnDiskRemoved(const chromeos::disks::DiskMountManager::Disk* disk);
  void OnDiskMounted(const chromeos::disks::DiskMountManager::Disk* disk);
  void OnDiskUnmounted(const chromeos::disks::DiskMountManager::Disk* disk);
  void OnDeviceAdded(const std::string& device_path);
  void OnDeviceRemoved(const std::string& device_path);
  void OnDeviceScanned(const std::string& device_path);
  void OnFormattingStarted(const std::string& device_path, bool success);
  void OnFormattingFinished(const std::string& device_path, bool success);

  // Process file watch notifications.
  void HandleFileWatchNotification(const FilePath& path,
                                   bool got_error);

  // Sends folder change event.
  void DispatchFolderChangeEvent(const FilePath& path, bool error,
                                 const ExtensionUsageRegistry& extensions);

  // Sends filesystem changed extension message to all renderers.
  void DispatchDiskEvent(const chromeos::disks::DiskMountManager::Disk* disk,
                         bool added);

  void DispatchMountCompletedEvent(
      chromeos::disks::DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info);

  void RemoveBrowserFromVector(const std::string& path);

  // Used to create a window of a standard size, and add it to a list
  // of tracked browser windows in case that device goes away.
  void OpenFileBrowse(const std::string& url,
                      const std::string& device_path,
                      bool small);

  // Returns the GDataFileSystem for the current profile.
  gdata::GDataFileSystem* GetRemoteFileSystem() const;

  // Handles requests to start and stop periodic updates on remote file system.
  // When |start| is set to true, this function starts periodic updates only if
  // it is not yet started; when |start| is set to false, this function stops
  // periodic updates only if the number of outstanding update requests reaches
  // zero.
  void HandleRemoteUpdateRequestOnUIThread(bool start);

  scoped_refptr<FileWatcherDelegate> delegate_;
  WatcherMap file_watchers_;
  scoped_ptr<FileBrowserNotifications> notifications_;
  Profile* profile_;
  base::Lock lock_;

  bool current_gdata_operation_failed_;
  int last_active_gdata_operation_count_;

  // Number of active update requests on the remote file system.
  int num_remote_update_requests_;

  DISALLOW_COPY_AND_ASSIGN(FileBrowserEventRouter);
};

// Singleton that owns all FileBrowserEventRouter and associates
// them with Profiles.
class FileBrowserEventRouterFactory
    : public RefcountedProfileKeyedServiceFactory {
 public:
  // Returns the FileBrowserEventRouter for |profile|, creating it if
  // it is not yet created.
  static scoped_refptr<FileBrowserEventRouter> GetForProfile(Profile* profile);

  // Returns the FileBrowserEventRouterFactory instance.
  static FileBrowserEventRouterFactory* GetInstance();

 protected:
  // ProfileKeyedBasedFactory overrides:
  virtual bool ServiceHasOwnInstanceInIncognito() OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<FileBrowserEventRouterFactory>;

  FileBrowserEventRouterFactory();
  virtual ~FileBrowserEventRouterFactory();

  // ProfileKeyedServiceFactory:
  virtual scoped_refptr<RefcountedProfileKeyedService> BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_EVENT_ROUTER_H_
