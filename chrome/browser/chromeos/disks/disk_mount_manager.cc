// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/disks/disk_mount_manager.h"

#include <map>
#include <set>

#include <sys/statvfs.h>

#include "base/bind.h"
#include "base/observer_list.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {
namespace disks {

namespace {

const char kDeviceNotFound[] = "Device could not be found";

DiskMountManager* g_disk_mount_manager = NULL;

// The DiskMountManager implementation.
class DiskMountManagerImpl : public DiskMountManager {
 public:
  DiskMountManagerImpl() : weak_ptr_factory_(this) {
    DBusThreadManager* dbus_thread_manager = DBusThreadManager::Get();
    DCHECK(dbus_thread_manager);
    cros_disks_client_ = dbus_thread_manager->GetCrosDisksClient();
    DCHECK(cros_disks_client_);

    cros_disks_client_->SetUpConnections(
        base::Bind(&DiskMountManagerImpl::OnMountEvent,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&DiskMountManagerImpl::OnMountCompleted,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~DiskMountManagerImpl() {
  }

  // DiskMountManager override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  // DiskMountManager override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  // DiskMountManager override.
  virtual void MountPath(const std::string& source_path,
                         MountType type) OVERRIDE {
    // Hidden and non-existent devices should not be mounted.
    if (type == MOUNT_TYPE_DEVICE) {
      DiskMap::const_iterator it = disks_.find(source_path);
      if (it == disks_.end() || it->second->is_hidden()) {
        OnMountCompleted(MOUNT_ERROR_INTERNAL, source_path, type, "");
        return;
      }
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    cros_disks_client_->Mount(
        source_path,
        type,
        // When succeeds, OnMountCompleted will be called by
        // "MountCompleted" signal instead.
        base::Bind(&DoNothing),
        base::Bind(&DiskMountManagerImpl::OnMountCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   MOUNT_ERROR_INTERNAL,
                   source_path,
                   type,
                   ""));
  }

  // DiskMountManager override.
  virtual void UnmountPath(const std::string& mount_path) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    cros_disks_client_->Unmount(mount_path,
                                base::Bind(&DiskMountManagerImpl::OnUnmountPath,
                                           weak_ptr_factory_.GetWeakPtr()),
                                base::Bind(&DoNothing));
  }

  // DiskMountManager override.
  virtual void GetSizeStatsOnFileThread(const std::string& mount_path,
                                        size_t* total_size_kb,
                                        size_t* remaining_size_kb) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    uint64_t total_size_in_bytes = 0;
    uint64_t remaining_size_in_bytes = 0;

    struct statvfs stat = {};  // Zero-clear
    if (statvfs(mount_path.c_str(), &stat) == 0) {
      total_size_in_bytes =
          static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
      remaining_size_in_bytes =
          static_cast<uint64_t>(stat.f_bfree) * stat.f_frsize;
    }
    *total_size_kb = static_cast<size_t>(total_size_in_bytes / 1024);
    *remaining_size_kb = static_cast<size_t>(remaining_size_in_bytes / 1024);
  }

  // DiskMountManager override.
  virtual void FormatUnmountedDevice(const std::string& file_path) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    for (DiskMountManager::DiskMap::iterator it = disks_.begin();
         it != disks_.end(); ++it) {
      if (it->second->file_path() == file_path &&
          !it->second->mount_path().empty()) {
        LOG(ERROR) << "Device is still mounted: " << file_path;
        OnFormatDevice(file_path, false);
        return;
      }
    }
    const char kFormatVFAT[] = "vfat";
    cros_disks_client_->FormatDevice(
        file_path,
        kFormatVFAT,
        base::Bind(&DiskMountManagerImpl::OnFormatDevice,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&DiskMountManagerImpl::OnFormatDevice,
                   weak_ptr_factory_.GetWeakPtr(),
                   file_path,
                   false));
  }

  // DiskMountManager override.
  virtual void FormatMountedDevice(const std::string& mount_path) OVERRIDE {
    Disk* disk = NULL;
    for (DiskMountManager::DiskMap::iterator it = disks_.begin();
         it != disks_.end(); ++it) {
      if (it->second->mount_path() == mount_path) {
        disk = it->second;
        break;
      }
    }
    if (!disk) {
      LOG(ERROR) << "Device with this mount path not found: " << mount_path;
      OnFormatDevice(mount_path, false);
      return;
    }
    if (formatting_pending_.find(disk->device_path()) !=
        formatting_pending_.end()) {
      LOG(ERROR) << "Formatting is already pending: " << mount_path;
      OnFormatDevice(mount_path, false);
      return;
    }
    // Formatting process continues, after unmounting.
    formatting_pending_[disk->device_path()] = disk->file_path();
    UnmountPath(disk->mount_path());
  }

  // DiskMountManager override.
  virtual void UnmountDeviceRecursive(
      const std::string& device_path,
      UnmountDeviceRecursiveCallbackType callback,
      void* user_data) OVERRIDE {
    bool success = true;
    std::string error_message;
    std::vector<std::string> devices_to_unmount;

    // Get list of all devices to unmount.
    int device_path_len = device_path.length();
    for (DiskMap::iterator it = disks_.begin(); it != disks_.end(); ++it) {
      if (!it->second->mount_path().empty() &&
          strncmp(device_path.c_str(), it->second->device_path().c_str(),
                  device_path_len) == 0) {
        devices_to_unmount.push_back(it->second->mount_path());
      }
    }
    // We should detect at least original device.
    if (devices_to_unmount.empty()) {
      if (disks_.find(device_path) == disks_.end()) {
        success = false;
        error_message = kDeviceNotFound;
      } else {
        // Nothing to unmount.
        callback(user_data, true);
        return;
      }
    }
    if (success) {
      // We will send the same callback data object to all Unmount calls and use
      // it to syncronize callbacks.
      UnmountDeviceRecursiveCallbackData* cb_data =
          new UnmountDeviceRecursiveCallbackData(user_data, callback,
                                                 devices_to_unmount.size());
      for (size_t i = 0; i < devices_to_unmount.size(); ++i) {
        cros_disks_client_->Unmount(
            devices_to_unmount[i],
            base::Bind(&DiskMountManagerImpl::OnUnmountDeviceRecursive,
                       weak_ptr_factory_.GetWeakPtr(), cb_data, true),
            base::Bind(&DiskMountManagerImpl::OnUnmountDeviceRecursive,
                       weak_ptr_factory_.GetWeakPtr(),
                       cb_data,
                       false,
                       devices_to_unmount[i]));
      }
    } else {
      LOG(WARNING) << "Unmount recursive request failed for device "
                   << device_path << ", with error: " << error_message;
      callback(user_data, false);
    }
  }

  // DiskMountManager override.
  virtual void RequestMountInfoRefresh() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    cros_disks_client_->EnumerateAutoMountableDevices(
        base::Bind(&DiskMountManagerImpl::OnRequestMountInfo,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&DoNothing));
  }

  // DiskMountManager override.
  const DiskMap& disks() const OVERRIDE { return disks_; }


  // DiskMountManager override.
  const MountPointMap& mount_points() const OVERRIDE { return mount_points_; }

 private:
  struct UnmountDeviceRecursiveCallbackData {
    void* user_data;
    UnmountDeviceRecursiveCallbackType callback;
    size_t pending_callbacks_count;

    UnmountDeviceRecursiveCallbackData(void* ud,
                                       UnmountDeviceRecursiveCallbackType cb,
                                       int count)
        : user_data(ud),
          callback(cb),
          pending_callbacks_count(count) {
    }
  };

  // Callback for UnmountDeviceRecursive.
  void OnUnmountDeviceRecursive(UnmountDeviceRecursiveCallbackData* cb_data,
                                bool success,
                                const std::string& mount_path) {
    if (success) {
      // Do standard processing for Unmount event.
      OnUnmountPath(mount_path);
      LOG(INFO) << mount_path <<  " unmounted.";
    }
    // This is safe as long as all callbacks are called on the same thread as
    // UnmountDeviceRecursive.
    cb_data->pending_callbacks_count--;

    if (cb_data->pending_callbacks_count == 0) {
      cb_data->callback(cb_data->user_data, success);
      delete cb_data;
    }
  }

  // Callback to handle MountCompleted signal and Mount method call failure.
  void OnMountCompleted(MountError error_code,
                        const std::string& source_path,
                        MountType mount_type,
                        const std::string& mount_path) {
    MountCondition mount_condition = MOUNT_CONDITION_NONE;
    if (mount_type == MOUNT_TYPE_DEVICE) {
      if (error_code == MOUNT_ERROR_UNKNOWN_FILESYSTEM)
        mount_condition = MOUNT_CONDITION_UNKNOWN_FILESYSTEM;
      if (error_code == MOUNT_ERROR_UNSUPORTED_FILESYSTEM)
        mount_condition = MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM;
    }
    const MountPointInfo mount_info(source_path, mount_path, mount_type,
                                    mount_condition);

    NotifyMountCompleted(MOUNTING, error_code, mount_info);

    // If the device is corrupted but it's still possible to format it, it will
    // be fake mounted.
    if ((error_code == MOUNT_ERROR_NONE || mount_info.mount_condition) &&
        mount_points_.find(mount_info.mount_path) == mount_points_.end()) {
      mount_points_.insert(MountPointMap::value_type(mount_info.mount_path,
                                                     mount_info));
    }
    if ((error_code == MOUNT_ERROR_NONE || mount_info.mount_condition) &&
        mount_info.mount_type == MOUNT_TYPE_DEVICE &&
        !mount_info.source_path.empty() &&
        !mount_info.mount_path.empty()) {
      DiskMap::iterator iter = disks_.find(mount_info.source_path);
      if (iter == disks_.end()) {
        // disk might have been removed by now?
        return;
      }
      Disk* disk = iter->second;
      DCHECK(disk);
      disk->set_mount_path(mount_info.mount_path);
      NotifyDiskStatusUpdate(MOUNT_DISK_MOUNTED, disk);
    }
  }

  // Callback for UnmountPath.
  void OnUnmountPath(const std::string& mount_path) {
    MountPointMap::iterator mount_points_it = mount_points_.find(mount_path);
    if (mount_points_it == mount_points_.end())
      return;
    // TODO(tbarzic): Add separate, PathUnmounted event to Observer.
    NotifyMountCompleted(UNMOUNTING,
                         MOUNT_ERROR_NONE,
                         MountPointInfo(mount_points_it->second.source_path,
                                        mount_points_it->second.mount_path,
                                        mount_points_it->second.mount_type,
                                        mount_points_it->second.mount_condition)
                         );
    std::string path(mount_points_it->second.source_path);
    mount_points_.erase(mount_points_it);
    DiskMap::iterator iter = disks_.find(path);
    if (iter == disks_.end()) {
      // disk might have been removed by now.
      return;
    }
    Disk* disk = iter->second;
    DCHECK(disk);
    disk->clear_mount_path();
    // Check if there is a formatting scheduled.
    PathMap::iterator it = formatting_pending_.find(disk->device_path());
    if (it != formatting_pending_.end()) {
      // Copy the string before it gets erased.
      const std::string file_path = it->second;
      formatting_pending_.erase(it);
      FormatUnmountedDevice(file_path);
    }
  }

  // Callback for FormatDevice.
  void OnFormatDevice(const std::string& device_path, bool success) {
    if (success) {
      NotifyDeviceStatusUpdate(MOUNT_FORMATTING_STARTED, device_path);
    } else {
      NotifyDeviceStatusUpdate(MOUNT_FORMATTING_STARTED,
                             std::string("!") + device_path);
      LOG(WARNING) << "Format request failed for device " << device_path;
    }
  }

  // Callbcak for GetDeviceProperties.
  void OnGetDeviceProperties(const DiskInfo& disk_info) {
    // TODO(zelidrag): Find a better way to filter these out before we
    // fetch the properties:
    // Ignore disks coming from the device we booted the system from.
    if (disk_info.on_boot_device())
      return;

    LOG(WARNING) << "Found disk " << disk_info.device_path();
    // Delete previous disk info for this path:
    bool is_new = true;
    DiskMap::iterator iter = disks_.find(disk_info.device_path());
    if (iter != disks_.end()) {
      delete iter->second;
      disks_.erase(iter);
      is_new = false;
    }
    Disk* disk = new Disk(disk_info.device_path(),
                          disk_info.mount_path(),
                          disk_info.system_path(),
                          disk_info.file_path(),
                          disk_info.label(),
                          disk_info.drive_label(),
                          FindSystemPathPrefix(disk_info.system_path()),
                          disk_info.device_type(),
                          disk_info.total_size_in_bytes(),
                          disk_info.is_drive(),
                          disk_info.is_read_only(),
                          disk_info.has_media(),
                          disk_info.on_boot_device(),
                          disk_info.is_hidden());
    disks_.insert(std::make_pair(disk_info.device_path(), disk));
    NotifyDiskStatusUpdate(is_new ? MOUNT_DISK_ADDED : MOUNT_DISK_CHANGED,
                           disk);
  }

  // Callbcak for RequestMountInfo.
  void OnRequestMountInfo(const std::vector<std::string>& devices) {
    std::set<std::string> current_device_set;
    if (!devices.empty()) {
      // Initiate properties fetch for all removable disks,
      for (size_t i = 0; i < devices.size(); i++) {
        current_device_set.insert(devices[i]);
        // Initiate disk property retrieval for each relevant device path.
        cros_disks_client_->GetDeviceProperties(
            devices[i],
            base::Bind(&DiskMountManagerImpl::OnGetDeviceProperties,
                       weak_ptr_factory_.GetWeakPtr()),
            base::Bind(&DoNothing));
      }
    }
    // Search and remove disks that are no longer present.
    for (DiskMap::iterator iter = disks_.begin(); iter != disks_.end(); ) {
      if (current_device_set.find(iter->first) == current_device_set.end()) {
        Disk* disk = iter->second;
        NotifyDiskStatusUpdate(MOUNT_DISK_REMOVED, disk);
        delete iter->second;
        disks_.erase(iter++);
      } else {
        ++iter;
      }
    }
  }

  // Callback to handle mount event signals.
  void OnMountEvent(MountEventType event, std::string device_path) {
    DiskMountManagerEventType type = MOUNT_DEVICE_ADDED;
    switch (event) {
      case DISK_ADDED: {
        cros_disks_client_->GetDeviceProperties(
            device_path,
            base::Bind(&DiskMountManagerImpl::OnGetDeviceProperties,
                       weak_ptr_factory_.GetWeakPtr()),
            base::Bind(&DoNothing));
        return;
      }
      case DISK_REMOVED: {
        // Search and remove disks that are no longer present.
        DiskMountManager::DiskMap::iterator iter = disks_.find(device_path);
        if (iter != disks_.end()) {
          Disk* disk = iter->second;
          NotifyDiskStatusUpdate(MOUNT_DISK_REMOVED, disk);
          delete iter->second;
          disks_.erase(iter);
        }
        return;
      }
      case DEVICE_ADDED: {
        type = MOUNT_DEVICE_ADDED;
        system_path_prefixes_.insert(device_path);
        break;
      }
      case DEVICE_REMOVED: {
        type = MOUNT_DEVICE_REMOVED;
        system_path_prefixes_.erase(device_path);
        break;
      }
      case DEVICE_SCANNED: {
        type = MOUNT_DEVICE_SCANNED;
        break;
      }
      case FORMATTING_FINISHED: {
        // FORMATTING_FINISHED actually returns file path instead of device
        // path.
        device_path = FilePathToDevicePath(device_path);
        if (device_path.empty()) {
          LOG(ERROR) << "Error while handling disks metadata. Cannot find "
                     << "device that is being formatted.";
          return;
        }
        type = MOUNT_FORMATTING_FINISHED;
        break;
      }
      default: {
        LOG(ERROR) << "Unknown event: " << event;
        return;
      }
    }
    NotifyDeviceStatusUpdate(type, device_path);
  }

  // Notifies all observers about disk status update.
  void NotifyDiskStatusUpdate(DiskMountManagerEventType event,
                              const Disk* disk) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    FOR_EACH_OBSERVER(Observer, observers_, DiskChanged(event, disk));
  }

  // Notifies all observers about device status update.
  void NotifyDeviceStatusUpdate(DiskMountManagerEventType event,
                                const std::string& device_path) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    FOR_EACH_OBSERVER(Observer, observers_, DeviceChanged(event, device_path));
  }

  // Notifies all observers about mount completion.
  void NotifyMountCompleted(MountEvent event_type,
                            MountError error_code,
                            const MountPointInfo& mount_info) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    FOR_EACH_OBSERVER(Observer, observers_,
                      MountCompleted(event_type, error_code, mount_info));
  }

  // Converts file path to device path.
  std::string FilePathToDevicePath(const std::string& file_path) {
    // TODO(hashimoto): Refactor error handling code like here.
    // Appending "!" is not the best way to indicate error.  This kind of trick
    // also makes it difficult to simplify the code paths.  crosbug.com/22972
    const int failed = StartsWithASCII(file_path, "!", true);
    for (DiskMountManager::DiskMap::iterator it = disks_.begin();
         it != disks_.end(); ++it) {
      // Skip the leading '!' on the failure case.
      if (it->second->file_path() == file_path.substr(failed)) {
        if (failed)
          return std::string("!") + it->second->device_path();
        else
          return it->second->device_path();
      }
    }
    return "";
  }

  // Finds system path prefix from |system_path|.
  const std::string& FindSystemPathPrefix(const std::string& system_path) {
    if (system_path.empty())
      return EmptyString();
    for (SystemPathPrefixSet::const_iterator it = system_path_prefixes_.begin();
         it != system_path_prefixes_.end();
         ++it) {
      const std::string& prefix = *it;
      if (StartsWithASCII(system_path, prefix, true))
        return prefix;
    }
    return EmptyString();
  }

  // A function to be used as an empty callback.
  static void DoNothing() {
  }

  // Mount event change observers.
  ObserverList<Observer> observers_;

  CrosDisksClient* cros_disks_client_;

  // The list of disks found.
  DiskMountManager::DiskMap disks_;

  DiskMountManager::MountPointMap mount_points_;

  typedef std::set<std::string> SystemPathPrefixSet;
  SystemPathPrefixSet system_path_prefixes_;

  // A map from device path (e.g. /sys/devices/pci0000:00/.../sdb/sdb1)) to file
  // path (e.g. /dev/sdb).
  // Devices in this map are supposed to be formatted, but are currently waiting
  // to be unmounted. When device is in this map, the formatting process HAVEN'T
  // started yet.
  typedef std::map<std::string, std::string> PathMap;
  PathMap formatting_pending_;

  base::WeakPtrFactory<DiskMountManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DiskMountManagerImpl);
};

} // namespace

DiskMountManager::Disk::Disk(const std::string& device_path,
                             const std::string& mount_path,
                             const std::string& system_path,
                             const std::string& file_path,
                             const std::string& device_label,
                             const std::string& drive_label,
                             const std::string& system_path_prefix,
                             DeviceType device_type,
                             uint64 total_size_in_bytes,
                             bool is_parent,
                             bool is_read_only,
                             bool has_media,
                             bool on_boot_device,
                             bool is_hidden)
    : device_path_(device_path),
      mount_path_(mount_path),
      system_path_(system_path),
      file_path_(file_path),
      device_label_(device_label),
      drive_label_(drive_label),
      system_path_prefix_(system_path_prefix),
      device_type_(device_type),
      total_size_in_bytes_(total_size_in_bytes),
      is_parent_(is_parent),
      is_read_only_(is_read_only),
      has_media_(has_media),
      on_boot_device_(on_boot_device),
      is_hidden_(is_hidden) {
}

DiskMountManager::Disk::~Disk() {}

// static
std::string DiskMountManager::MountTypeToString(MountType type) {
  switch (type) {
    case MOUNT_TYPE_DEVICE:
      return "device";
    case MOUNT_TYPE_ARCHIVE:
      return "file";
    case MOUNT_TYPE_NETWORK_STORAGE:
      return "network";
    case MOUNT_TYPE_INVALID:
      return "invalid";
    default:
      NOTREACHED();
  }
  return "";
}

// static
std::string DiskMountManager::MountConditionToString(MountCondition condition) {
  switch (condition) {
    case MOUNT_CONDITION_NONE:
      return "";
    case MOUNT_CONDITION_UNKNOWN_FILESYSTEM:
      return "unknown_filesystem";
    case MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM:
      return "unsupported_filesystem";
    default:
      NOTREACHED();
  }
  return "";
}

// static
MountType DiskMountManager::MountTypeFromString(const std::string& type_str) {
  if (type_str == "device")
    return MOUNT_TYPE_DEVICE;
  else if (type_str == "network")
    return MOUNT_TYPE_NETWORK_STORAGE;
  else if (type_str == "file")
    return MOUNT_TYPE_ARCHIVE;
  else
    return MOUNT_TYPE_INVALID;
}

// static
void DiskMountManager::Initialize() {
  if (g_disk_mount_manager) {
    LOG(WARNING) << "DiskMountManager was already initialized";
    return;
  }
  g_disk_mount_manager = new DiskMountManagerImpl();
  VLOG(1) << "DiskMountManager initialized";
}

// static
void DiskMountManager::InitializeForTesting(
    DiskMountManager* disk_mount_manager) {
  if (g_disk_mount_manager) {
    LOG(WARNING) << "DiskMountManager was already initialized";
    return;
  }
  g_disk_mount_manager = disk_mount_manager;
  VLOG(1) << "DiskMountManager initialized";
}

// static
void DiskMountManager::Shutdown() {
  if (!g_disk_mount_manager) {
    LOG(WARNING) << "DiskMountManager::Shutdown() called with NULL manager";
    return;
  }
  delete g_disk_mount_manager;
  g_disk_mount_manager = NULL;
  VLOG(1) << "DiskMountManager Shutdown completed";
}

// static
DiskMountManager* DiskMountManager::GetInstance() {
  return g_disk_mount_manager;
}

}  // namespace disks
}  // namespace chromeos
