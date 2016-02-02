// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_VOLUME_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_VOLUME_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/storage_monitor/removable_storage_observer.h"
#include "device/media_transfer_protocol/mtp_storage_info.pb.h"

class Profile;

namespace chromeos {
class PowerManagerClient;
}  // namespace chromeos

namespace content {
class BrowserContext;
}  // namespace content

namespace file_manager {

class MountedDiskMonitor;
class SnapshotManager;
class VolumeManagerObserver;

// Identifiers for volume types managed by Chrome OS file manager.
enum VolumeType {
  VOLUME_TYPE_TESTING = -1,  // Used only in tests.
  VOLUME_TYPE_GOOGLE_DRIVE = 0,
  VOLUME_TYPE_DOWNLOADS_DIRECTORY,
  VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
  VOLUME_TYPE_MOUNTED_ARCHIVE_FILE,
  VOLUME_TYPE_PROVIDED,  // File system provided by the FileSystemProvider API.
  VOLUME_TYPE_MTP,
  // The enum values must be kept in sync with FileManagerVolumeType in
  // tools/metrics/histograms/histograms.xml. Since enums for histograms are
  // append-only (for keeping the number consistent across versions), new values
  // for this enum also has to be always appended at the end (i.e., here).
  NUM_VOLUME_TYPE,
};

// Says how was the mount performed, whether due to user interaction, or
// automatic. User interaction includes both hardware (pluggins a USB stick)
// or software (mounting a ZIP archive) interaction.
enum MountContext {
  MOUNT_CONTEXT_USER,
  MOUNT_CONTEXT_AUTO,
  MOUNT_CONTEXT_UNKNOWN
};

// Source of a volume's data.
enum Source { SOURCE_FILE, SOURCE_DEVICE, SOURCE_NETWORK, SOURCE_SYSTEM };

// Represents a volume (mount point) in the volume manager. Validity of the data
// is guaranteed by the weak pointer. Simply saying, the weak pointer should be
// valid as long as the volume is mounted.
class Volume : public base::SupportsWeakPtr<Volume> {
 public:
  ~Volume();

  // Factory static methods for different volume types.
  static Volume* CreateForDrive(Profile* profile);
  static Volume* CreateForDownloads(const base::FilePath& downloads_path);
  static Volume* CreateForRemovable(
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_point,
      const chromeos::disks::DiskMountManager::Disk* disk);
  static Volume* CreateForProvidedFileSystem(
      const chromeos::file_system_provider::ProvidedFileSystemInfo&
          file_system_info,
      MountContext mount_context);
  static Volume* CreateForMTP(const base::FilePath& mount_path,
                              const std::string& label,
                              bool read_only);
  static Volume* CreateForTesting(const base::FilePath& path,
                                  VolumeType volume_type,
                                  chromeos::DeviceType device_type,
                                  bool read_only);
  static Volume* CreateForTesting(const base::FilePath& device_path,
                                  const base::FilePath& mount_path);

  // Getters for all members. See below for details.
  const std::string& volume_id() const { return volume_id_; }
  const std::string& file_system_id() const { return file_system_id_; }
  const std::string& extension_id() const { return extension_id_; }
  Source source() const { return source_; }
  VolumeType type() const { return type_; }
  chromeos::DeviceType device_type() const { return device_type_; }
  const base::FilePath& source_path() const { return source_path_; }
  const base::FilePath& mount_path() const { return mount_path_; }
  chromeos::disks::MountCondition mount_condition() const {
    return mount_condition_;
  }
  MountContext mount_context() const { return mount_context_; }
  const base::FilePath& system_path_prefix() const {
    return system_path_prefix_;
  }
  const std::string& volume_label() const { return volume_label_; }
  bool is_parent() const { return is_parent_; }
  bool is_read_only() const { return is_read_only_; }
  bool has_media() const { return has_media_; }
  bool configurable() const { return configurable_; }
  bool watchable() const { return watchable_; }

 private:
  Volume();

  // The ID of the volume.
  std::string volume_id_;

  // The ID for provided file systems. If other type, then empty string. Unique
  // per providing extension.
  std::string file_system_id_;

  // The ID of an extension providing the file system. If other type, then equal
  // to an empty string.
  std::string extension_id_;

  // The source of the volume's data.
  Source source_;

  // The type of mounted volume.
  VolumeType type_;

  // The type of device. (e.g. USB, SD card, DVD etc.)
  chromeos::DeviceType device_type_;

  // The source path of the volume.
  // E.g.:
  // - /home/chronos/user/Downloads/zipfile_path.zip
  base::FilePath source_path_;

  // The mount path of the volume.
  // E.g.:
  // - /home/chronos/user/Downloads
  // - /media/removable/usb1
  // - /media/archive/zip1
  base::FilePath mount_path_;

  // The mounting condition. See the enum for the details.
  chromeos::disks::MountCondition mount_condition_;

  // The context of the mount. Whether mounting was performed due to a user
  // interaction or not.
  MountContext mount_context_;

  // Path of the system device this device's block is a part of.
  // (e.g. /sys/devices/pci0000:00/.../8:0:0:0/)
  base::FilePath system_path_prefix_;

  // Label for the volume if the volume is either removable or a provided
  // file system. In case of removables, if disk is a parent, then its label,
  // else parents label (e.g. "TransMemory").
  std::string volume_label_;

  // Is the device is a parent device (i.e. sdb rather than sdb1).
  bool is_parent_;

  // True if the volume is read only.
  bool is_read_only_;

  // True if the volume contains media.
  bool has_media_;

  // True if the volume is configurable.
  bool configurable_;

  // True if the volume notifies about changes via file/directory watchers.
  bool watchable_;

  DISALLOW_COPY_AND_ASSIGN(Volume);
};

// Manages "Volume"s for file manager. Here are "Volume"s.
// - Drive File System (not yet supported).
// - Downloads directory.
// - Removable disks (volume will be created for each partition, not only one
//   for a device).
// - Mounted zip archives.
class VolumeManager : public KeyedService,
                      public drive::DriveIntegrationServiceObserver,
                      public chromeos::disks::DiskMountManager::Observer,
                      public chromeos::file_system_provider::Observer,
                      public storage_monitor::RemovableStorageObserver {
 public:
  // Returns MediaTransferProtocolManager. Used for injecting
  // FakeMediaTransferProtocolManager for testing.
  typedef base::Callback<const MtpStorageInfo*(const std::string&)>
      GetMtpStorageInfoCallback;

  VolumeManager(
      Profile* profile,
      drive::DriveIntegrationService* drive_integration_service,
      chromeos::PowerManagerClient* power_manager_client,
      chromeos::disks::DiskMountManager* disk_mount_manager,
      chromeos::file_system_provider::Service* file_system_provider_service,
      GetMtpStorageInfoCallback get_mtp_storage_info_callback);
  ~VolumeManager() override;

  // Returns the instance corresponding to the |context|.
  static VolumeManager* Get(content::BrowserContext* context);

  // Initializes this instance.
  void Initialize();

  // Disposes this instance.
  void Shutdown() override;

  // Adds an observer.
  void AddObserver(VolumeManagerObserver* observer);

  // Removes the observer.
  void RemoveObserver(VolumeManagerObserver* observer);

  // Returns the information about all volumes currently mounted. The returned
  // weak pointers are valid as long as the volumes are mounted.
  std::vector<base::WeakPtr<Volume>> GetVolumeList();

  // Finds Volume for the given volume ID. If found, then the returned weak
  // pointer is valid. It is invalidated as soon as the volume is removed from
  // the volume manager.
  base::WeakPtr<Volume> FindVolumeById(const std::string& volume_id);

  // For testing purpose, registers a native local file system pointing to
  // |path| with DOWNLOADS type, and adds its volume info.
  bool RegisterDownloadsDirectoryForTesting(const base::FilePath& path);

  // For testing purpose, adds a volume info pointing to |path|, with TESTING
  // type. Assumes that the mount point is already registered.
  void AddVolumeForTesting(const base::FilePath& path,
                           VolumeType volume_type,
                           chromeos::DeviceType device_type,
                           bool read_only);

  // For testing purpose, adds the volume info to the volume manager.
  void AddVolumeForTesting(const linked_ptr<Volume>& volume);

  // drive::DriveIntegrationServiceObserver overrides.
  void OnFileSystemMounted() override;
  void OnFileSystemBeingUnmounted() override;

  // chromeos::disks::DiskMountManager::Observer overrides.
  void OnDiskEvent(
      chromeos::disks::DiskMountManager::DiskEvent event,
      const chromeos::disks::DiskMountManager::Disk* disk) override;
  void OnDeviceEvent(chromeos::disks::DiskMountManager::DeviceEvent event,
                     const std::string& device_path) override;
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override;
  void OnFormatEvent(chromeos::disks::DiskMountManager::FormatEvent event,
                     chromeos::FormatError error_code,
                     const std::string& device_path) override;

  // chromeos::file_system_provider::Observer overrides.
  void OnProvidedFileSystemMount(
      const chromeos::file_system_provider::ProvidedFileSystemInfo&
          file_system_info,
      chromeos::file_system_provider::MountContext context,
      base::File::Error error) override;
  void OnProvidedFileSystemUnmount(
      const chromeos::file_system_provider::ProvidedFileSystemInfo&
          file_system_info,
      base::File::Error error) override;

  // Called on change to kExternalStorageDisabled pref.
  void OnExternalStorageDisabledChanged();

  // RemovableStorageObserver overrides.
  void OnRemovableStorageAttached(
      const storage_monitor::StorageInfo& info) override;
  void OnRemovableStorageDetached(
      const storage_monitor::StorageInfo& info) override;

  SnapshotManager* snapshot_manager() { return snapshot_manager_.get(); }

 private:
  void OnDiskMountManagerRefreshed(bool success);
  void OnStorageMonitorInitialized();
  void DoMountEvent(chromeos::MountError error_code,
                    const linked_ptr<Volume>& volume);
  void DoUnmountEvent(chromeos::MountError error_code,
                      const linked_ptr<Volume>& volume);

  Profile* profile_;
  drive::DriveIntegrationService* drive_integration_service_;  // Not owned.
  chromeos::disks::DiskMountManager* disk_mount_manager_;      // Not owned.
  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<VolumeManagerObserver> observers_;
  chromeos::file_system_provider::Service*
      file_system_provider_service_;  // Not owned by this class.
  GetMtpStorageInfoCallback get_mtp_storage_info_callback_;
  std::map<std::string, linked_ptr<Volume>> mounted_volumes_;
  scoped_ptr<SnapshotManager> snapshot_manager_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<VolumeManager> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(VolumeManager);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_VOLUME_MANAGER_H_
