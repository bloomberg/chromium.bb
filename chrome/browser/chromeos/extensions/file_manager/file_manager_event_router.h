// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_EVENT_ROUTER_H_

#include <map>
#include <string>

#include "base/files/file_path_watcher.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/drive/file_system_observer.h"
#include "chrome/browser/chromeos/net/connectivity_state_helper_observer.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chromeos/disks/disk_mount_manager.h"

class FileManagerNotifications;
class PrefChangeRegistrar;
class Profile;

// Monitors changes in disk mounts, network connection state and preferences
// affecting File Manager. Dispatches appropriate File Browser events.
class FileManagerEventRouter
    : public chromeos::disks::DiskMountManager::Observer,
      public chromeos::ConnectivityStateHelperObserver,
      public drive::DriveSystemServiceObserver,
      public drive::FileSystemObserver,
      public drive::JobListObserver,
      public google_apis::DriveServiceObserver {
 public:
  // Interface that should keep track of the system state in regards to system
  // suspend and resume events.
  // When the |IsResuming()| returns true, it should be able to check if a
  // removable device was present before the was system suspended.
  class SuspendStateDelegate {
   public:
    virtual ~SuspendStateDelegate() {}

    // Returns true if the system has recently woken up.
    virtual bool SystemIsResuming() const = 0;
    // If system is resuming, returns true if the disk was present before the
    // system suspend. Should return false if the system is not resuming.
    virtual bool DiskWasPresentBeforeSuspend(
        const chromeos::disks::DiskMountManager::Disk& disk) const = 0;
  };

  explicit FileManagerEventRouter(Profile* profile);
  virtual ~FileManagerEventRouter();

  void Shutdown();

  // Starts observing file system change events.
  void ObserveFileSystemEvents();

  typedef base::Callback<void(bool success)> BoolCallback;

  // Adds a file watch at |local_path|, associated with |virtual_path|, for
  // an extension with |extension_id|.
  //
  // |callback| will be called with true on success, or false on failure.
  // |callback| must not be null.
  void AddFileWatch(const base::FilePath& local_path,
                    const base::FilePath& virtual_path,
                    const std::string& extension_id,
                    const BoolCallback& callback);

  // Removes a file watch at |local_path| for an extension with |extension_id|.
  void RemoveFileWatch(const base::FilePath& local_path,
                       const std::string& extension_id);

  // Mounts Drive on File browser. |callback| will be called after raising a
  // mount request event to file manager on JS-side.
  void MountDrive(const base::Closure& callback);

  // CrosDisksClient::Observer overrides.
  virtual void OnDiskEvent(
      chromeos::disks::DiskMountManager::DiskEvent event,
      const chromeos::disks::DiskMountManager::Disk* disk) OVERRIDE;
  virtual void OnDeviceEvent(
      chromeos::disks::DiskMountManager::DeviceEvent event,
      const std::string& device_path) OVERRIDE;
  virtual void OnMountEvent(
      chromeos::disks::DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info)
      OVERRIDE;
  virtual void OnFormatEvent(
      chromeos::disks::DiskMountManager::FormatEvent event,
      chromeos::FormatError error_code,
      const std::string& device_path) OVERRIDE;

  // chromeos::ConnectivityStateHelperObserver override.
  virtual void NetworkManagerChanged() OVERRIDE;
  virtual void DefaultNetworkChanged() OVERRIDE;

  // drive::JobListObserver overrides.
  virtual void OnJobAdded(const drive::JobInfo& job_info) OVERRIDE;
  virtual void OnJobUpdated(const drive::JobInfo& job_info) OVERRIDE;
  virtual void OnJobDone(const drive::JobInfo& job_info,
                         drive::FileError error) OVERRIDE;

  // drive::DriveServiceObserver overrides.
  virtual void OnRefreshTokenInvalid() OVERRIDE;

  // drive::FileSystemObserver overrides.
  virtual void OnDirectoryChanged(
      const base::FilePath& directory_path) OVERRIDE;

  // drive::DriveSystemServiceObserver overrides.
  virtual void OnFileSystemMounted() OVERRIDE;
  virtual void OnFileSystemBeingUnmounted() OVERRIDE;

 private:
  typedef std::map<std::string, int> ExtensionUsageRegistry;

  // This class is used to remember what extensions are watching |virtual_path|.
  class FileWatcherExtensions {
   public:
    FileWatcherExtensions(const base::FilePath& virtual_path,
        const std::string& extension_id,
        bool is_remote_file_system);

    ~FileWatcherExtensions();

    void AddExtension(const std::string& extension_id);

    void RemoveExtension(const std::string& extension_id);

    const ExtensionUsageRegistry& GetExtensions() const;

    unsigned int GetRefCount() const;

    const base::FilePath& GetVirtualPath() const;

    // Starts a file watch at |local_path|. |file_watcher_callback| will be
    // called when changes are notified.
    //
    // |callback| will be called with true, if the file watch is started
    // successfully, or false if failed. |callback| must not be null.
    void Watch(const base::FilePath& local_path,
               const base::FilePathWatcher::Callback& file_watcher_callback,
               const BoolCallback& callback);

   private:
    // Called when a FilePathWatcher is created and started.
    // |file_path_watcher| is NULL, if the watcher wasn't started successfully.
    void OnWatcherStarted(const BoolCallback& callback,
                          base::FilePathWatcher* file_path_watcher);

    base::FilePathWatcher* file_watcher_;
    base::FilePath local_path_;
    base::FilePath virtual_path_;
    ExtensionUsageRegistry extensions_;
    unsigned int ref_count_;
    bool is_remote_file_system_;

    // Note: This should remain the last member so it'll be destroyed and
    // invalidate the weak pointers before any other members are destroyed.
    base::WeakPtrFactory<FileWatcherExtensions> weak_ptr_factory_;
  };

  typedef std::map<base::FilePath, FileWatcherExtensions*> WatcherMap;

  // USB mount event handlers.
  void OnDiskAdded(const chromeos::disks::DiskMountManager::Disk* disk);
  void OnDiskRemoved(const chromeos::disks::DiskMountManager::Disk* disk);
  void OnDiskMounted(const chromeos::disks::DiskMountManager::Disk* disk);
  void OnDiskUnmounted(const chromeos::disks::DiskMountManager::Disk* disk);
  void OnDeviceAdded(const std::string& device_path);
  void OnDeviceRemoved(const std::string& device_path);
  void OnDeviceScanned(const std::string& device_path);
  void OnFormatStarted(const std::string& device_path, bool success);
  void OnFormatCompleted(const std::string& device_path, bool success);

  // Called on change to kExternalStorageDisabled pref.
  void OnExternalStorageDisabledChanged();

  // Called when prefs related to file manager change.
  void OnFileManagerPrefsChanged();

  // Process file watch notifications.
  void HandleFileWatchNotification(const base::FilePath& path,
                                   bool got_error);

  // Sends directory change event.
  void DispatchDirectoryChangeEvent(const base::FilePath& path, bool error,
                                    const ExtensionUsageRegistry& extensions);

  void DispatchMountEvent(
      chromeos::disks::DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info);

  // If needed, opens a file manager window for the removable device mounted at
  // |mount_path|. Disk.mount_path() is empty, since it is being filled out
  // after calling notifying observers by DiskMountManager.
  void ShowRemovableDeviceInFileManager(
      const chromeos::disks::DiskMountManager::Disk& disk,
      const base::FilePath& mount_path);

  // Sends onFileTranferUpdated to extensions if needed. If |always| is true,
  // it sends the event always. Otherwise, it sends the event if enough time has
  // passed from the previous event so as not to make extension busy.
  void SendDriveFileTransferEvent(bool always);

  // Manages the list of currently active Drive file transfer jobs.
  struct DriveJobInfoWithStatus {
    DriveJobInfoWithStatus();
    DriveJobInfoWithStatus(const drive::JobInfo& info,
                           const std::string& status);
    drive::JobInfo job_info;
    std::string status;
  };
  std::map<drive::JobID, DriveJobInfoWithStatus> drive_jobs_;
  base::Time last_file_transfer_event_;

  base::FilePathWatcher::Callback file_watcher_callback_;
  WatcherMap file_watchers_;
  scoped_ptr<FileManagerNotifications> notifications_;
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;
  scoped_ptr<SuspendStateDelegate> suspend_state_delegate_;
  Profile* profile_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileManagerEventRouter> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FileManagerEventRouter);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_EVENT_ROUTER_H_
