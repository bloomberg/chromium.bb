// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/disks/disk_mount_manager.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"

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
    STLDeleteContainerPairSecondPointers(disks_.begin(), disks_.end());
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
                         const std::string& source_format,
                         const std::string& mount_label,
                         MountType type) OVERRIDE {
    // Hidden and non-existent devices should not be mounted.
    if (type == MOUNT_TYPE_DEVICE) {
      DiskMap::const_iterator it = disks_.find(source_path);
      if (it == disks_.end() || it->second->is_hidden()) {
        OnMountCompleted(MOUNT_ERROR_INTERNAL, source_path, type, "");
        return;
      }
    }
    cros_disks_client_->Mount(
        source_path,
        source_format,
        mount_label,
        type,
        // When succeeds, OnMountCompleted will be called by
        // "MountCompleted" signal instead.
        base::Bind(&base::DoNothing),
        base::Bind(&DiskMountManagerImpl::OnMountCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   MOUNT_ERROR_INTERNAL,
                   source_path,
                   type,
                   ""));
  }

  // DiskMountManager override.
  virtual void UnmountPath(const std::string& mount_path,
                           UnmountOptions options) OVERRIDE {
    UnmountChildMounts(mount_path);
    cros_disks_client_->Unmount(mount_path, options,
                                base::Bind(&DiskMountManagerImpl::OnUnmountPath,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           true),
                                base::Bind(&DiskMountManagerImpl::OnUnmountPath,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           false));
  }

  // DiskMountManager override.
  virtual void FormatMountedDevice(const std::string& mount_path) OVERRIDE {
    MountPointMap::const_iterator mount_point = mount_points_.find(mount_path);
    if (mount_point == mount_points_.end()) {
      LOG(ERROR) << "Mount point with path \"" << mount_path << "\" not found.";
      OnFormatDevice(mount_path, false);
      return;
    }

    std::string device_path = mount_point->second.source_path;
    DiskMap::const_iterator disk = disks_.find(device_path);
    if (disk == disks_.end()) {
      LOG(ERROR) << "Device with path \"" << device_path << "\" not found.";
      OnFormatDevice(device_path, false);
      return;
    }

    if (formatting_pending_.find(device_path) != formatting_pending_.end()) {
      LOG(ERROR) << "Formatting is already pending: " << mount_path;
      OnFormatDevice(device_path, false);
      return;
    }

    // Formatting process continues, after unmounting.
    formatting_pending_.insert(device_path);
    UnmountPath(disk->second->mount_path(), UNMOUNT_OPTIONS_NONE);
  }

  // DiskMountManager override.
  virtual void UnmountDeviceRecursively(
      const std::string& device_path,
      const UnmountDeviceRecursivelyCallbackType& callback) OVERRIDE {
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
        LOG(WARNING) << "Unmount recursive request failed for device "
                     << device_path << ", with error: " << kDeviceNotFound;
        callback.Run(false);
        return;
      }

      // Nothing to unmount.
      callback.Run(true);
      return;
    }

    // We will send the same callback data object to all Unmount calls and use
    // it to syncronize callbacks.
    // Note: this implementation has a potential memory leak issue. For
    // example if this instance is destructed before all the callbacks for
    // Unmount are invoked, the memory pointed by |cb_data| will be leaked.
    // It is because the UnmountDeviceRecursivelyCallbackData keeps how
    // many times OnUnmountDeviceRecursively callback is called and when
    // all the callbacks are called, |cb_data| will be deleted in the method.
    // However destructing the instance before all callback invocations will
    // cancel all pending callbacks, so that the |cb_data| would never be
    // deleted.
    // Fortunately, in the real scenario, the instance will be destructed
    // only for ShutDown. So, probably the memory would rarely be leaked.
    // TODO(hidehiko): Fix the issue.
    UnmountDeviceRecursivelyCallbackData* cb_data =
        new UnmountDeviceRecursivelyCallbackData(
            callback, devices_to_unmount.size());
    for (size_t i = 0; i < devices_to_unmount.size(); ++i) {
      cros_disks_client_->Unmount(
          devices_to_unmount[i],
          UNMOUNT_OPTIONS_NONE,
          base::Bind(&DiskMountManagerImpl::OnUnmountDeviceRecursively,
                     weak_ptr_factory_.GetWeakPtr(), cb_data, true),
          base::Bind(&DiskMountManagerImpl::OnUnmountDeviceRecursively,
                     weak_ptr_factory_.GetWeakPtr(), cb_data, false));
    }
  }

  // DiskMountManager override.
  virtual void RequestMountInfoRefresh() OVERRIDE {
    cros_disks_client_->EnumerateAutoMountableDevices(
        base::Bind(&DiskMountManagerImpl::OnRequestMountInfo,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&base::DoNothing));
  }

  // DiskMountManager override.
  virtual const DiskMap& disks() const OVERRIDE { return disks_; }

  // DiskMountManager override.
  virtual const Disk* FindDiskBySourcePath(const std::string& source_path)
      const OVERRIDE {
    DiskMap::const_iterator disk_it = disks_.find(source_path);
    return disk_it == disks_.end() ? NULL : disk_it->second;
  }

  // DiskMountManager override.
  virtual const MountPointMap& mount_points() const OVERRIDE {
    return mount_points_;
  }

  // DiskMountManager override.
  virtual bool AddDiskForTest(Disk* disk) OVERRIDE {
    if (disks_.find(disk->device_path()) != disks_.end()) {
      LOG(ERROR) << "Attempt to add a duplicate disk";
      return false;
    }

    disks_.insert(std::make_pair(disk->device_path(), disk));
    return true;
  }

  // DiskMountManager override.
  // Corresponding disk should be added to the manager before this is called.
  virtual bool AddMountPointForTest(
      const MountPointInfo& mount_point) OVERRIDE {
    if (mount_points_.find(mount_point.mount_path) != mount_points_.end()) {
      LOG(ERROR) << "Attempt to add a duplicate mount point";
      return false;
    }
    if (mount_point.mount_type == chromeos::MOUNT_TYPE_DEVICE &&
        disks_.find(mount_point.source_path) == disks_.end()) {
      LOG(ERROR) << "Device mount points must have a disk entry.";
      return false;
    }

    mount_points_.insert(std::make_pair(mount_point.mount_path, mount_point));
    return true;
  }

 private:
  struct UnmountDeviceRecursivelyCallbackData {
    UnmountDeviceRecursivelyCallbackData(
        const UnmountDeviceRecursivelyCallbackType& in_callback,
        int in_num_pending_callbacks)
        : callback(in_callback),
          num_pending_callbacks(in_num_pending_callbacks) {
    }

    const UnmountDeviceRecursivelyCallbackType callback;
    size_t num_pending_callbacks;
  };

  // Unmounts all mount points whose source path is transitively parented by
  // |mount_path|.
  void UnmountChildMounts(const std::string& mount_path_in) {
    std::string mount_path = mount_path_in;
    // Let's make sure mount path has trailing slash.
    if (mount_path[mount_path.length() - 1] != '/')
      mount_path += '/';

    for (MountPointMap::iterator it = mount_points_.begin();
         it != mount_points_.end();
         ++it) {
      if (StartsWithASCII(it->second.source_path, mount_path,
                          true /*case sensitive*/)) {
        UnmountPath(it->second.mount_path, UNMOUNT_OPTIONS_NONE);
      }
    }
  }

  // Callback for UnmountDeviceRecursively.
  void OnUnmountDeviceRecursively(
      UnmountDeviceRecursivelyCallbackData* cb_data,
      bool success,
      const std::string& mount_path) {
    if (success) {
      // Do standard processing for Unmount event.
      OnUnmountPath(true, mount_path);
      LOG(INFO) << mount_path <<  " unmounted.";
    }
    // This is safe as long as all callbacks are called on the same thread as
    // UnmountDeviceRecursively.
    cb_data->num_pending_callbacks--;

    if (cb_data->num_pending_callbacks == 0) {
      // This code has a problem that the |success| status used here is for the
      // last "unmount" callback, but not whether all unmounting is succeeded.
      // TODO(hidehiko): Fix the issue.
      cb_data->callback.Run(success);
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
      if (error_code == MOUNT_ERROR_UNKNOWN_FILESYSTEM) {
        mount_condition = MOUNT_CONDITION_UNKNOWN_FILESYSTEM;
      }
      if (error_code == MOUNT_ERROR_UNSUPPORTED_FILESYSTEM) {
        mount_condition = MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM;
      }
    }
    const MountPointInfo mount_info(source_path, mount_path, mount_type,
                                    mount_condition);

    NotifyMountStatusUpdate(MOUNTING, error_code, mount_info);

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
    }
  }

  // Callback for UnmountPath.
  void OnUnmountPath(bool success, const std::string& mount_path) {
    MountPointMap::iterator mount_points_it = mount_points_.find(mount_path);
    if (mount_points_it == mount_points_.end())
      return;

    NotifyMountStatusUpdate(
        UNMOUNTING,
        success ? MOUNT_ERROR_NONE : MOUNT_ERROR_INTERNAL,
        MountPointInfo(mount_points_it->second.source_path,
                       mount_points_it->second.mount_path,
                       mount_points_it->second.mount_type,
                       mount_points_it->second.mount_condition));

    std::string path(mount_points_it->second.source_path);
    if (success)
      mount_points_.erase(mount_points_it);

    DiskMap::iterator disk_iter = disks_.find(path);
    if (disk_iter != disks_.end()) {
      DCHECK(disk_iter->second);
      if (success)
        disk_iter->second->clear_mount_path();
    }

    FormatTaskSet::iterator format_iter = formatting_pending_.find(path);
    // Check if there is a formatting scheduled.
    if (format_iter != formatting_pending_.end()) {
      formatting_pending_.erase(format_iter);
      if (success && disk_iter != disks_.end()) {
        FormatUnmountedDevice(path);
      } else {
        OnFormatDevice(path, false);
      }
    }
  }

  // Starts device formatting.
  void FormatUnmountedDevice(const std::string& device_path) {
    DiskMap::const_iterator disk = disks_.find(device_path);
    DCHECK(disk != disks_.end() && disk->second->mount_path().empty());

    const char kFormatVFAT[] = "vfat";
    cros_disks_client_->FormatDevice(
        device_path,
        kFormatVFAT,
        base::Bind(&DiskMountManagerImpl::OnFormatDevice,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&DiskMountManagerImpl::OnFormatDevice,
                   weak_ptr_factory_.GetWeakPtr(),
                   device_path,
                   false));
  }

  // Callback for FormatDevice.
  // TODO(tbarzic): Pass FormatError instead of bool.
  void OnFormatDevice(const std::string& device_path, bool success) {
    FormatError error_code =
        success ? FORMAT_ERROR_NONE : FORMAT_ERROR_UNKNOWN;
    NotifyFormatStatusUpdate(FORMAT_STARTED, error_code, device_path);
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
                          disk_info.vendor_id(),
                          disk_info.vendor_name(),
                          disk_info.product_id(),
                          disk_info.product_name(),
                          disk_info.uuid(),
                          FindSystemPathPrefix(disk_info.system_path()),
                          disk_info.device_type(),
                          disk_info.total_size_in_bytes(),
                          disk_info.is_drive(),
                          disk_info.is_read_only(),
                          disk_info.has_media(),
                          disk_info.on_boot_device(),
                          disk_info.is_hidden());
    disks_.insert(std::make_pair(disk_info.device_path(), disk));
    NotifyDiskStatusUpdate(is_new ? DISK_ADDED : DISK_CHANGED, disk);
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
            base::Bind(&base::DoNothing));
      }
    }
    // Search and remove disks that are no longer present.
    for (DiskMap::iterator iter = disks_.begin(); iter != disks_.end(); ) {
      if (current_device_set.find(iter->first) == current_device_set.end()) {
        Disk* disk = iter->second;
        NotifyDiskStatusUpdate(DISK_REMOVED, disk);
        delete iter->second;
        disks_.erase(iter++);
      } else {
        ++iter;
      }
    }
  }

  // Callback to handle mount event signals.
  void OnMountEvent(MountEventType event, const std::string& device_path_arg) {
    // Take a copy of the argument so we can modify it below.
    std::string device_path = device_path_arg;
    switch (event) {
      case CROS_DISKS_DISK_ADDED: {
        cros_disks_client_->GetDeviceProperties(
            device_path,
            base::Bind(&DiskMountManagerImpl::OnGetDeviceProperties,
                       weak_ptr_factory_.GetWeakPtr()),
            base::Bind(&base::DoNothing));
        break;
      }
      case CROS_DISKS_DISK_REMOVED: {
        // Search and remove disks that are no longer present.
        DiskMountManager::DiskMap::iterator iter = disks_.find(device_path);
        if (iter != disks_.end()) {
          Disk* disk = iter->second;
          NotifyDiskStatusUpdate(DISK_REMOVED, disk);
          delete iter->second;
          disks_.erase(iter);
        }
        break;
      }
      case CROS_DISKS_DEVICE_ADDED: {
        system_path_prefixes_.insert(device_path);
        NotifyDeviceStatusUpdate(DEVICE_ADDED, device_path);
        break;
      }
      case CROS_DISKS_DEVICE_REMOVED: {
        system_path_prefixes_.erase(device_path);
        NotifyDeviceStatusUpdate(DEVICE_REMOVED, device_path);
        break;
      }
      case CROS_DISKS_DEVICE_SCANNED: {
        NotifyDeviceStatusUpdate(DEVICE_SCANNED, device_path);
        break;
      }
      case CROS_DISKS_FORMATTING_FINISHED: {
        std::string path;
        FormatError error_code;
        ParseFormatFinishedPath(device_path, &path, &error_code);

        if (!path.empty()) {
          NotifyFormatStatusUpdate(FORMAT_COMPLETED, error_code, path);
          break;
        }

        LOG(ERROR) << "Error while handling disks metadata. Cannot find "
                   << "device that is being formatted.";
        break;
      }
      default: {
        LOG(ERROR) << "Unknown event: " << event;
      }
    }
  }

  // Notifies all observers about disk status update.
  void NotifyDiskStatusUpdate(DiskEvent event,
                              const Disk* disk) {
    FOR_EACH_OBSERVER(Observer, observers_, OnDiskEvent(event, disk));
  }

  // Notifies all observers about device status update.
  void NotifyDeviceStatusUpdate(DeviceEvent event,
                                const std::string& device_path) {
    FOR_EACH_OBSERVER(Observer, observers_, OnDeviceEvent(event, device_path));
  }

  // Notifies all observers about mount completion.
  void NotifyMountStatusUpdate(MountEvent event,
                               MountError error_code,
                               const MountPointInfo& mount_info) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnMountEvent(event, error_code, mount_info));
  }

  void NotifyFormatStatusUpdate(FormatEvent event,
                                FormatError error_code,
                                const std::string& device_path) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnFormatEvent(event, error_code, device_path));
  }

  // Converts file path to device path.
  void ParseFormatFinishedPath(const std::string& received_path,
                               std::string* device_path,
                               FormatError* error_code) {
    // TODO(tbarzic): Refactor error handling code like here.
    // Appending "!" is not the best way to indicate error.  This kind of trick
    // also makes it difficult to simplify the code paths.
    bool success = !StartsWithASCII(received_path, "!", true);
    *error_code = success ? FORMAT_ERROR_NONE : FORMAT_ERROR_UNKNOWN;

    std::string path = received_path.substr(success ? 0 : 1);

    // Depending on cros disks implementation the event may return either file
    // path or device path. We want to use device path.
    for (DiskMountManager::DiskMap::iterator it = disks_.begin();
         it != disks_.end(); ++it) {
      // Skip the leading '!' on the failure case.
      if (it->second->file_path() == path ||
          it->second->device_path() == path) {
        *device_path = it->second->device_path();
        return;
      }
    }
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
  typedef std::set<std::string> FormatTaskSet;
  FormatTaskSet formatting_pending_;

  base::WeakPtrFactory<DiskMountManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DiskMountManagerImpl);
};

}  // namespace

DiskMountManager::Disk::Disk(const std::string& device_path,
                             const std::string& mount_path,
                             const std::string& system_path,
                             const std::string& file_path,
                             const std::string& device_label,
                             const std::string& drive_label,
                             const std::string& vendor_id,
                             const std::string& vendor_name,
                             const std::string& product_id,
                             const std::string& product_name,
                             const std::string& fs_uuid,
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
      vendor_id_(vendor_id),
      vendor_name_(vendor_name),
      product_id_(product_id),
      product_name_(product_name),
      fs_uuid_(fs_uuid),
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

bool DiskMountManager::AddDiskForTest(Disk* disk) {
  return false;
}

bool DiskMountManager::AddMountPointForTest(const MountPointInfo& mount_point) {
  return false;
}

// static
std::string DiskMountManager::MountTypeToString(MountType type) {
  switch (type) {
    case MOUNT_TYPE_DEVICE:
      return "device";
    case MOUNT_TYPE_ARCHIVE:
      return "file";
    case MOUNT_TYPE_NETWORK_STORAGE:
      return "network";
    case MOUNT_TYPE_GOOGLE_DRIVE:
      return "drive";
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
  else if (type_str == "drive")
    return MOUNT_TYPE_GOOGLE_DRIVE;
  else
    return MOUNT_TYPE_INVALID;
}

// static
std::string DiskMountManager::DeviceTypeToString(DeviceType type) {
  switch (type) {
    case DEVICE_TYPE_USB:
      return "usb";
    case DEVICE_TYPE_SD:
      return "sd";
    case DEVICE_TYPE_OPTICAL_DISC:
      return "optical";
    case DEVICE_TYPE_MOBILE:
      return "mobile";
    default:
      return "unknown";
  }
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
