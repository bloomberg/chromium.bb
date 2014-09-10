// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/volume_manager.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/snapshot_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/external_mount_points.h"

namespace file_manager {
namespace {

const char kFileManagerMTPMountNamePrefix[] = "fileman-mtp-";
const char kMtpVolumeIdPrefix [] = "mtp:";

// Registers |path| as the "Downloads" folder to the FileSystem API backend.
// If another folder is already mounted. It revokes and overrides the old one.
bool RegisterDownloadsMountPoint(Profile* profile, const base::FilePath& path) {
  // Although we show only profile's own "Downloads" folder in Files.app,
  // in the backend we need to mount all profile's download directory globally.
  // Otherwise, Files.app cannot support cross-profile file copies, etc.
  // For this reason, we need to register to the global GetSystemInstance().
  const std::string mount_point_name =
      file_manager::util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  // In some tests we want to override existing Downloads mount point, so we
  // first revoke the existing mount point (if any).
  mount_points->RevokeFileSystem(mount_point_name);
  return mount_points->RegisterFileSystem(mount_point_name,
                                          storage::kFileSystemTypeNativeLocal,
                                          storage::FileSystemMountOption(),
                                          path);
}

// Finds the path register as the "Downloads" folder to FileSystem API backend.
// Returns false if it is not registered.
bool FindDownloadsMountPointPath(Profile* profile, base::FilePath* path) {
  const std::string mount_point_name =
      util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  return mount_points->GetRegisteredPath(mount_point_name, path);
}

VolumeType MountTypeToVolumeType(chromeos::MountType type) {
  switch (type) {
    case chromeos::MOUNT_TYPE_INVALID:
      // We don't expect this value, but list here, so that when any value
      // is added to the enum definition but this is not edited, the compiler
      // warns it.
      break;
    case chromeos::MOUNT_TYPE_DEVICE:
      return VOLUME_TYPE_REMOVABLE_DISK_PARTITION;
    case chromeos::MOUNT_TYPE_ARCHIVE:
      return VOLUME_TYPE_MOUNTED_ARCHIVE_FILE;
  }

  NOTREACHED();
  return VOLUME_TYPE_DOWNLOADS_DIRECTORY;
}

// Returns a string representation of the given volume type.
std::string VolumeTypeToString(VolumeType type) {
  switch (type) {
    case VOLUME_TYPE_GOOGLE_DRIVE:
      return "drive";
    case VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      return "downloads";
    case VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      return "removable";
    case VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      return "archive";
    case VOLUME_TYPE_CLOUD_DEVICE:
      return "cloud_device";
    case VOLUME_TYPE_PROVIDED:
      return "provided";
    case VOLUME_TYPE_MTP:
      return "mtp";
    case VOLUME_TYPE_TESTING:
      return "testing";
    case NUM_VOLUME_TYPE:
      break;
  }
  NOTREACHED();
  return "";
}

// Generates a unique volume ID for the given volume info.
std::string GenerateVolumeId(const VolumeInfo& volume_info) {
  // For the same volume type, base names are unique, as mount points are
  // flat for the same volume type.
  return (VolumeTypeToString(volume_info.type) + ":" +
          volume_info.mount_path.BaseName().AsUTF8Unsafe());
}

// Returns the VolumeInfo for Drive file system.
VolumeInfo CreateDriveVolumeInfo(Profile* profile) {
  const base::FilePath& drive_path =
      drive::util::GetDriveMountPointPath(profile);

  VolumeInfo volume_info;
  volume_info.type = VOLUME_TYPE_GOOGLE_DRIVE;
  volume_info.device_type = chromeos::DEVICE_TYPE_UNKNOWN;
  volume_info.source_path = drive_path;
  volume_info.mount_path = drive_path;
  volume_info.mount_condition = chromeos::disks::MOUNT_CONDITION_NONE;
  volume_info.is_parent = false;
  volume_info.is_read_only = false;
  volume_info.volume_id = GenerateVolumeId(volume_info);
  return volume_info;
}

VolumeInfo CreateDownloadsVolumeInfo(const base::FilePath& downloads_path) {
  VolumeInfo volume_info;
  volume_info.type = VOLUME_TYPE_DOWNLOADS_DIRECTORY;
  volume_info.device_type = chromeos::DEVICE_TYPE_UNKNOWN;
  // Keep source_path empty.
  volume_info.mount_path = downloads_path;
  volume_info.mount_condition = chromeos::disks::MOUNT_CONDITION_NONE;
  volume_info.is_parent = false;
  volume_info.is_read_only = false;
  volume_info.volume_id = GenerateVolumeId(volume_info);
  return volume_info;
}

VolumeInfo CreateTestingVolumeInfo(const base::FilePath& path,
                                   VolumeType volume_type,
                                   chromeos::DeviceType device_type) {
  VolumeInfo volume_info;
  volume_info.type = volume_type;
  volume_info.device_type = device_type;
  // Keep source_path empty.
  volume_info.mount_path = path;
  volume_info.mount_condition = chromeos::disks::MOUNT_CONDITION_NONE;
  volume_info.is_parent = false;
  volume_info.is_read_only = false;
  volume_info.volume_id = GenerateVolumeId(volume_info);
  return volume_info;
}

VolumeInfo CreateVolumeInfoFromMountPointInfo(
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_point,
    const chromeos::disks::DiskMountManager::Disk* disk) {
  VolumeInfo volume_info;
  volume_info.type = MountTypeToVolumeType(mount_point.mount_type);
  volume_info.source_path = base::FilePath(mount_point.source_path);
  volume_info.mount_path = base::FilePath(mount_point.mount_path);
  volume_info.mount_condition = mount_point.mount_condition;
  volume_info.volume_label = volume_info.mount_path.BaseName().AsUTF8Unsafe();
  if (disk) {
    volume_info.device_type = disk->device_type();
    volume_info.system_path_prefix =
        base::FilePath(disk->system_path_prefix());
    volume_info.is_parent = disk->is_parent();
    volume_info.is_read_only = disk->is_read_only();
  } else {
    volume_info.device_type = chromeos::DEVICE_TYPE_UNKNOWN;
    volume_info.is_parent = false;
    volume_info.is_read_only =
        (mount_point.mount_type == chromeos::MOUNT_TYPE_ARCHIVE);
  }
  volume_info.volume_id = GenerateVolumeId(volume_info);

  return volume_info;
}

VolumeInfo CreateProvidedFileSystemVolumeInfo(
    const chromeos::file_system_provider::ProvidedFileSystemInfo&
        file_system_info) {
  VolumeInfo volume_info;
  volume_info.file_system_id = file_system_info.file_system_id();
  volume_info.extension_id = file_system_info.extension_id();
  volume_info.volume_label = file_system_info.display_name();
  volume_info.type = VOLUME_TYPE_PROVIDED;
  volume_info.mount_path = file_system_info.mount_path();
  volume_info.mount_condition = chromeos::disks::MOUNT_CONDITION_NONE;
  volume_info.is_parent = true;
  volume_info.is_read_only = !file_system_info.writable();
  volume_info.volume_id = GenerateVolumeId(volume_info);
  return volume_info;
}

std::string GetMountPointNameForMediaStorage(
    const storage_monitor::StorageInfo& info) {
  std::string name(kFileManagerMTPMountNamePrefix);
  name += info.device_id();
  return name;
}

}  // namespace

VolumeInfo::VolumeInfo()
    : type(VOLUME_TYPE_GOOGLE_DRIVE),
      device_type(chromeos::DEVICE_TYPE_UNKNOWN),
      mount_condition(chromeos::disks::MOUNT_CONDITION_NONE),
      is_parent(false),
      is_read_only(false) {
}

VolumeInfo::~VolumeInfo() {
}

VolumeManager::VolumeManager(
    Profile* profile,
    drive::DriveIntegrationService* drive_integration_service,
    chromeos::PowerManagerClient* power_manager_client,
    chromeos::disks::DiskMountManager* disk_mount_manager,
    chromeos::file_system_provider::Service* file_system_provider_service)
    : profile_(profile),
      drive_integration_service_(drive_integration_service),
      disk_mount_manager_(disk_mount_manager),
      file_system_provider_service_(file_system_provider_service),
      snapshot_manager_(new SnapshotManager(profile_)),
      weak_ptr_factory_(this) {
  DCHECK(disk_mount_manager);
}

VolumeManager::~VolumeManager() {
}

VolumeManager* VolumeManager::Get(content::BrowserContext* context) {
  return VolumeManagerFactory::Get(context);
}

void VolumeManager::Initialize() {
  // If in Sign in profile, then skip mounting and listening for mount events.
  if (chromeos::ProfileHelper::IsSigninProfile(profile_))
    return;

  // Path to mount user folders have changed several times. We need to migrate
  // the old preferences on paths to the new format when needed. For the detail,
  // see the comments in file_manager::util::MigratePathFromOldFormat,
  // Note: Preferences related to downloads are handled in download_prefs.cc.
  // TODO(kinaba): Remove this after several rounds of releases.
  const base::FilePath old_path =
      profile_->GetPrefs()->GetFilePath(prefs::kSelectFileLastDirectory);
  base::FilePath new_path;
  if (!old_path.empty() &&
      file_manager::util::MigratePathFromOldFormat(profile_,
                                                   old_path, &new_path)) {
    profile_->GetPrefs()->SetFilePath(prefs::kSelectFileLastDirectory,
                                      new_path);
  }

  // Register 'Downloads' folder for the profile to the file system.
  const base::FilePath downloads =
      file_manager::util::GetDownloadsFolderForProfile(profile_);
  const bool success = RegisterDownloadsMountPoint(profile_, downloads);
  DCHECK(success);

  DoMountEvent(chromeos::MOUNT_ERROR_NONE,
               CreateDownloadsVolumeInfo(downloads));

  // Subscribe to DriveIntegrationService.
  if (drive_integration_service_) {
    drive_integration_service_->AddObserver(this);
    if (drive_integration_service_->IsMounted()) {
      DoMountEvent(chromeos::MOUNT_ERROR_NONE,
                   CreateDriveVolumeInfo(profile_));
    }
  }

  // Subscribe to DiskMountManager.
  disk_mount_manager_->AddObserver(this);
  disk_mount_manager_->EnsureMountInfoRefreshed(
      base::Bind(&VolumeManager::OnDiskMountManagerRefreshed,
                 weak_ptr_factory_.GetWeakPtr()));

  // Subscribe to FileSystemProviderService and register currently mounted
  // volumes for the profile.
  if (file_system_provider_service_) {
    using chromeos::file_system_provider::ProvidedFileSystemInfo;
    file_system_provider_service_->AddObserver(this);

    std::vector<ProvidedFileSystemInfo> file_system_info_list =
        file_system_provider_service_->GetProvidedFileSystemInfoList();
    for (size_t i = 0; i < file_system_info_list.size(); ++i) {
      VolumeInfo volume_info =
          CreateProvidedFileSystemVolumeInfo(file_system_info_list[i]);
      DoMountEvent(chromeos::MOUNT_ERROR_NONE, volume_info);
    }
  }

  // Subscribe to Profile Preference change.
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kExternalStorageDisabled,
      base::Bind(&VolumeManager::OnExternalStorageDisabledChanged,
                 weak_ptr_factory_.GetWeakPtr()));

  // Subscribe to storage monitor for MTP notifications.
  if (storage_monitor::StorageMonitor::GetInstance()) {
    storage_monitor::StorageMonitor::GetInstance()->EnsureInitialized(
        base::Bind(&VolumeManager::OnStorageMonitorInitialized,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void VolumeManager::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  snapshot_manager_.reset();
  pref_change_registrar_.RemoveAll();
  disk_mount_manager_->RemoveObserver(this);
  if (storage_monitor::StorageMonitor::GetInstance())
    storage_monitor::StorageMonitor::GetInstance()->RemoveObserver(this);

  if (drive_integration_service_)
    drive_integration_service_->RemoveObserver(this);

  if (file_system_provider_service_)
    file_system_provider_service_->RemoveObserver(this);
}

void VolumeManager::AddObserver(VolumeManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void VolumeManager::RemoveObserver(VolumeManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::vector<VolumeInfo> VolumeManager::GetVolumeInfoList() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<VolumeInfo> result;
  for (std::map<std::string, VolumeInfo>::const_iterator iter =
           mounted_volumes_.begin();
       iter != mounted_volumes_.end();
       ++iter) {
    result.push_back(iter->second);
  }
  return result;
}

bool VolumeManager::FindVolumeInfoById(const std::string& volume_id,
                                       VolumeInfo* result) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(result);

  std::map<std::string, VolumeInfo>::const_iterator iter =
      mounted_volumes_.find(volume_id);
  if (iter == mounted_volumes_.end())
    return false;
  *result = iter->second;
  return true;
}

bool VolumeManager::RegisterDownloadsDirectoryForTesting(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath old_path;
  if (FindDownloadsMountPointPath(profile_, &old_path)) {
    DoUnmountEvent(chromeos::MOUNT_ERROR_NONE,
                   CreateDownloadsVolumeInfo(old_path));
  }

  bool success = RegisterDownloadsMountPoint(profile_, path);
  DoMountEvent(
      success ? chromeos::MOUNT_ERROR_NONE : chromeos::MOUNT_ERROR_INVALID_PATH,
      CreateDownloadsVolumeInfo(path));
  return success;
}

void VolumeManager::AddVolumeInfoForTesting(const base::FilePath& path,
                                            VolumeType volume_type,
                                            chromeos::DeviceType device_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoMountEvent(chromeos::MOUNT_ERROR_NONE,
               CreateTestingVolumeInfo(path, volume_type, device_type));
}

void VolumeManager::OnFileSystemMounted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Raise mount event.
  // We can pass chromeos::MOUNT_ERROR_NONE even when authentication is failed
  // or network is unreachable. These two errors will be handled later.
  VolumeInfo volume_info = CreateDriveVolumeInfo(profile_);
  DoMountEvent(chromeos::MOUNT_ERROR_NONE, volume_info);
}

void VolumeManager::OnFileSystemBeingUnmounted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VolumeInfo volume_info = CreateDriveVolumeInfo(profile_);
  DoUnmountEvent(chromeos::MOUNT_ERROR_NONE, volume_info);
}

void VolumeManager::OnDiskEvent(
    chromeos::disks::DiskMountManager::DiskEvent event,
    const chromeos::disks::DiskMountManager::Disk* disk) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Disregard hidden devices.
  if (disk->is_hidden())
    return;

  switch (event) {
    case chromeos::disks::DiskMountManager::DISK_ADDED:
    case chromeos::disks::DiskMountManager::DISK_CHANGED: {
      if (disk->device_path().empty()) {
        DVLOG(1) << "Empty system path for " << disk->device_path();
        return;
      }

      bool mounting = false;
      if (disk->mount_path().empty() && disk->has_media() &&
          !profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
        // If disk is not mounted yet and it has media and there is no policy
        // forbidding external storage, give it a try.
        // Initiate disk mount operation. MountPath auto-detects the filesystem
        // format if the second argument is empty. The third argument (mount
        // label) is not used in a disk mount operation.
        disk_mount_manager_->MountPath(
            disk->device_path(), std::string(), std::string(),
            chromeos::MOUNT_TYPE_DEVICE);
        mounting = true;
      }

      // Notify to observers.
      FOR_EACH_OBSERVER(VolumeManagerObserver, observers_,
                        OnDiskAdded(*disk, mounting));
      return;
    }

    case chromeos::disks::DiskMountManager::DISK_REMOVED:
      // If the disk is already mounted, unmount it.
      if (!disk->mount_path().empty()) {
        disk_mount_manager_->UnmountPath(
            disk->mount_path(),
            chromeos::UNMOUNT_OPTIONS_LAZY,
            chromeos::disks::DiskMountManager::UnmountPathCallback());
      }

      // Notify to observers.
      FOR_EACH_OBSERVER(VolumeManagerObserver, observers_,
                        OnDiskRemoved(*disk));
      return;
  }
  NOTREACHED();
}

void VolumeManager::OnDeviceEvent(
    chromeos::disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << "OnDeviceEvent: " << event << ", " << device_path;
  switch (event) {
    case chromeos::disks::DiskMountManager::DEVICE_ADDED:
      FOR_EACH_OBSERVER(VolumeManagerObserver, observers_,
                        OnDeviceAdded(device_path));
      return;
    case chromeos::disks::DiskMountManager::DEVICE_REMOVED: {
      FOR_EACH_OBSERVER(
          VolumeManagerObserver, observers_, OnDeviceRemoved(device_path));
      return;
    }
    case chromeos::disks::DiskMountManager::DEVICE_SCANNED:
      DVLOG(1) << "Ignore SCANNED event: " << device_path;
      return;
  }
  NOTREACHED();
}

void VolumeManager::OnMountEvent(
    chromeos::disks::DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(chromeos::MOUNT_TYPE_INVALID, mount_info.mount_type);

  if (mount_info.mount_type == chromeos::MOUNT_TYPE_ARCHIVE) {
    // If the file is not mounted now, tell it to drive file system so that
    // it can handle file caching correctly.
    // Note that drive file system knows if the file is managed by drive file
    // system or not, so here we report all paths.
    if ((event == chromeos::disks::DiskMountManager::MOUNTING &&
         error_code != chromeos::MOUNT_ERROR_NONE) ||
        (event == chromeos::disks::DiskMountManager::UNMOUNTING &&
         error_code == chromeos::MOUNT_ERROR_NONE)) {
      drive::FileSystemInterface* const file_system =
          drive::util::GetFileSystemByProfile(profile_);
      if (file_system) {
        file_system->MarkCacheFileAsUnmounted(
            base::FilePath(mount_info.source_path),
            base::Bind(&drive::util::EmptyFileOperationCallback));
      }
    }
  }

  // Notify a mounting/unmounting event to observers.
  const chromeos::disks::DiskMountManager::Disk* const disk =
      disk_mount_manager_->FindDiskBySourcePath(mount_info.source_path);
  const VolumeInfo volume_info =
      CreateVolumeInfoFromMountPointInfo(mount_info, disk);
  switch (event) {
    case chromeos::disks::DiskMountManager::MOUNTING: {
      DoMountEvent(error_code, volume_info);
      return;
    }
    case chromeos::disks::DiskMountManager::UNMOUNTING:
      DoUnmountEvent(error_code, volume_info);
      return;
  }
  NOTREACHED();
}

void VolumeManager::OnFormatEvent(
    chromeos::disks::DiskMountManager::FormatEvent event,
    chromeos::FormatError error_code,
    const std::string& device_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "OnDeviceEvent: " << event << ", " << error_code
           << ", " << device_path;

  switch (event) {
    case chromeos::disks::DiskMountManager::FORMAT_STARTED:
      FOR_EACH_OBSERVER(
          VolumeManagerObserver, observers_,
          OnFormatStarted(device_path,
                          error_code == chromeos::FORMAT_ERROR_NONE));
      return;
    case chromeos::disks::DiskMountManager::FORMAT_COMPLETED:
      if (error_code == chromeos::FORMAT_ERROR_NONE) {
        // If format is completed successfully, try to mount the device.
        // MountPath auto-detects filesystem format if second argument is
        // empty. The third argument (mount label) is not used in a disk mount
        // operation.
        disk_mount_manager_->MountPath(
            device_path, std::string(), std::string(),
            chromeos::MOUNT_TYPE_DEVICE);
      }

      FOR_EACH_OBSERVER(
          VolumeManagerObserver, observers_,
          OnFormatCompleted(device_path,
                            error_code == chromeos::FORMAT_ERROR_NONE));

      return;
  }
  NOTREACHED();
}

void VolumeManager::OnProvidedFileSystemMount(
    const chromeos::file_system_provider::ProvidedFileSystemInfo&
        file_system_info,
    base::File::Error error) {
  VolumeInfo volume_info = CreateProvidedFileSystemVolumeInfo(file_system_info);
  // TODO(mtomasz): Introduce own type, and avoid using MountError internally,
  // since it is related to cros disks only.
  const chromeos::MountError mount_error = error == base::File::FILE_OK
                                               ? chromeos::MOUNT_ERROR_NONE
                                               : chromeos::MOUNT_ERROR_UNKNOWN;
  DoMountEvent(mount_error, volume_info);
}

void VolumeManager::OnProvidedFileSystemUnmount(
    const chromeos::file_system_provider::ProvidedFileSystemInfo&
        file_system_info,
    base::File::Error error) {
  // TODO(mtomasz): Introduce own type, and avoid using MountError internally,
  // since it is related to cros disks only.
  const chromeos::MountError mount_error = error == base::File::FILE_OK
                                               ? chromeos::MOUNT_ERROR_NONE
                                               : chromeos::MOUNT_ERROR_UNKNOWN;
  VolumeInfo volume_info = CreateProvidedFileSystemVolumeInfo(file_system_info);
  DoUnmountEvent(mount_error, volume_info);
}

void VolumeManager::OnExternalStorageDisabledChanged() {
  // If the policy just got disabled we have to unmount every device currently
  // mounted. The opposite is fine - we can let the user re-plug her device to
  // make it available.
  if (profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    // We do not iterate on mount_points directly, because mount_points can
    // be changed by UnmountPath().
    // TODO(hidehiko): Is it necessary to unmount mounted archives, too, here?
    while (!disk_mount_manager_->mount_points().empty()) {
      std::string mount_path =
          disk_mount_manager_->mount_points().begin()->second.mount_path;
      disk_mount_manager_->UnmountPath(
          mount_path,
          chromeos::UNMOUNT_OPTIONS_NONE,
          chromeos::disks::DiskMountManager::UnmountPathCallback());
    }
  }
}

void VolumeManager::OnRemovableStorageAttached(
    const storage_monitor::StorageInfo& info) {
  if (!storage_monitor::StorageInfo::IsMTPDevice(info.device_id()))
    return;
  if (profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled))
    return;

  const base::FilePath path = base::FilePath::FromUTF8Unsafe(info.location());
  const std::string fsid = GetMountPointNameForMediaStorage(info);
  const std::string base_name = base::UTF16ToUTF8(info.model_name());

  // Assign a fresh volume ID based on the volume name.
  std::string label = base_name;
  for (int i = 2; mounted_volumes_.count(kMtpVolumeIdPrefix + label); ++i)
    label = base_name + base::StringPrintf(" (%d)", i);

  bool result =
      storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          fsid,
          storage::kFileSystemTypeDeviceMediaAsFileStorage,
          storage::FileSystemMountOption(),
          path);
  DCHECK(result);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE, base::Bind(
          &MTPDeviceMapService::RegisterMTPFileSystem,
          base::Unretained(MTPDeviceMapService::GetInstance()),
          info.location(), fsid));

  VolumeInfo volume_info;
  volume_info.type = VOLUME_TYPE_MTP;
  volume_info.mount_path = path;
  volume_info.mount_condition = chromeos::disks::MOUNT_CONDITION_NONE;
  volume_info.is_parent = true;
  volume_info.is_read_only = true;
  volume_info.volume_id = kMtpVolumeIdPrefix + label;
  volume_info.volume_label = label;
  volume_info.source_path = path;
  volume_info.device_type = chromeos::DEVICE_TYPE_MOBILE;
  DoMountEvent(chromeos::MOUNT_ERROR_NONE, volume_info);
}

void VolumeManager::OnRemovableStorageDetached(
    const storage_monitor::StorageInfo& info) {
  if (!storage_monitor::StorageInfo::IsMTPDevice(info.device_id()))
    return;

  for (std::map<std::string, VolumeInfo>::iterator it =
           mounted_volumes_.begin(); it != mounted_volumes_.end(); ++it) {
    if (it->second.source_path.value() == info.location()) {
      DoUnmountEvent(chromeos::MOUNT_ERROR_NONE, VolumeInfo(it->second));

      const std::string fsid = GetMountPointNameForMediaStorage(info);
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(fsid);
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE, base::Bind(
              &MTPDeviceMapService::RevokeMTPFileSystem,
              base::Unretained(MTPDeviceMapService::GetInstance()),
              fsid));
      return;
    }
  }
}

void VolumeManager::OnDiskMountManagerRefreshed(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to refresh disk mount manager";
    return;
  }

  std::vector<VolumeInfo> archives;

  const chromeos::disks::DiskMountManager::MountPointMap& mount_points =
      disk_mount_manager_->mount_points();
  for (chromeos::disks::DiskMountManager::MountPointMap::const_iterator it =
           mount_points.begin();
       it != mount_points.end();
       ++it) {
    if (it->second.mount_type == chromeos::MOUNT_TYPE_ARCHIVE) {
      // Archives are mounted after other types of volume. See below.
      archives.push_back(CreateVolumeInfoFromMountPointInfo(it->second, NULL));
      continue;
    }
    DoMountEvent(
        chromeos::MOUNT_ERROR_NONE,
        CreateVolumeInfoFromMountPointInfo(
            it->second,
            disk_mount_manager_->FindDiskBySourcePath(it->second.source_path)));
  }

  // We mount archives only if they are opened from currently mounted volumes.
  // To check the condition correctly in DoMountEvent, we care about the order.
  std::vector<bool> done(archives.size(), false);
  for (size_t i = 0; i < archives.size(); ++i) {
    if (done[i])
      continue;

    std::vector<VolumeInfo> chain;
    done[i] = true;
    chain.push_back(archives[i]);

    // If archives[i]'s source_path is in another archive, mount it first.
    for (size_t parent = i + 1; parent < archives.size(); ++parent) {
      if (!done[parent] &&
          archives[parent].mount_path.IsParent(chain.back().source_path)) {
        done[parent] = true;
        chain.push_back(archives[parent]);
        parent = i + 1;  // Search archives[parent]'s parent from the beginning.
      }
    }

    // Mount from the tail of chain.
    for (size_t i = chain.size(); i > 0; --i)
      DoMountEvent(chromeos::MOUNT_ERROR_NONE, chain[i - 1]);
  }
}

void VolumeManager::OnStorageMonitorInitialized() {
  std::vector<storage_monitor::StorageInfo> storages =
      storage_monitor::StorageMonitor::GetInstance()->GetAllAvailableStorages();
  for (size_t i = 0; i < storages.size(); ++i)
    OnRemovableStorageAttached(storages[i]);
  storage_monitor::StorageMonitor::GetInstance()->AddObserver(this);
}

void VolumeManager::DoMountEvent(chromeos::MountError error_code,
                                 const VolumeInfo& volume_info) {
  // Archive files are mounted globally in system. We however don't want to show
  // archives from profile-specific folders (Drive/Downloads) of other users in
  // multi-profile session. To this end, we filter out archives not on the
  // volumes already mounted on this VolumeManager instance.
  if (volume_info.type == VOLUME_TYPE_MOUNTED_ARCHIVE_FILE) {
    // Source may be in Drive cache folder under the current profile directory.
    bool from_current_profile =
        profile_->GetPath().IsParent(volume_info.source_path);
    for (std::map<std::string, VolumeInfo>::const_iterator iter =
             mounted_volumes_.begin();
         !from_current_profile && iter != mounted_volumes_.end();
         ++iter) {
      if (iter->second.mount_path.IsParent(volume_info.source_path))
        from_current_profile = true;
    }
    if (!from_current_profile)
      return;
  }

  // Filter out removable disks if forbidden by policy for this profile.
  if (volume_info.type == VOLUME_TYPE_REMOVABLE_DISK_PARTITION &&
      profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    return;
  }

  if (error_code == chromeos::MOUNT_ERROR_NONE || volume_info.mount_condition) {
    mounted_volumes_[volume_info.volume_id] = volume_info;


    UMA_HISTOGRAM_ENUMERATION("FileBrowser.VolumeType",
                              volume_info.type,
                              NUM_VOLUME_TYPE);
  }

  FOR_EACH_OBSERVER(VolumeManagerObserver,
                    observers_,
                    OnVolumeMounted(error_code, volume_info));
}

void VolumeManager::DoUnmountEvent(chromeos::MountError error_code,
                                   const VolumeInfo& volume_info) {
  if (mounted_volumes_.find(volume_info.volume_id) == mounted_volumes_.end())
    return;
  if (error_code == chromeos::MOUNT_ERROR_NONE)
    mounted_volumes_.erase(volume_info.volume_id);

  FOR_EACH_OBSERVER(VolumeManagerObserver,
                    observers_,
                    OnVolumeUnmounted(error_code, volume_info));
}

}  // namespace file_manager
