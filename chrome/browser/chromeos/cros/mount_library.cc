// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mount_library.h"

#include <set>
#include <sys/statvfs.h>
#include <vector>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

const char* kLibraryNotLoaded = "Cros Library not loaded";
const char* kDeviceNotFound = "Device could not be found";

namespace chromeos {

// static
std::string MountLibrary::MountTypeToString(MountType type) {
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

std::string MountLibrary::MountConditionToString(MountCondition condition) {
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
MountType MountLibrary::MountTypeFromString(
    const std::string& type_str) {
  if (type_str == "device") {
    return MOUNT_TYPE_DEVICE;
  } else if (type_str == "network") {
    return MOUNT_TYPE_NETWORK_STORAGE;
  } else if (type_str == "file") {
    return MOUNT_TYPE_ARCHIVE;
  } else {
    return MOUNT_TYPE_INVALID;
  }
}

MountLibrary::Disk::Disk(const std::string& device_path,
    const std::string& mount_path,
    const std::string& system_path,
    const std::string& file_path,
    const std::string& device_label,
    const std::string& drive_label,
    const std::string& parent_path,
    const std::string& system_path_prefix,
    DeviceType device_type,
    uint64 total_size,
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
      parent_path_(parent_path),
      system_path_prefix_(system_path_prefix),
      device_type_(device_type),
      total_size_(total_size),
      is_parent_(is_parent),
      is_read_only_(is_read_only),
      has_media_(has_media),
      on_boot_device_(on_boot_device),
      is_hidden_(is_hidden) {
}

MountLibrary::Disk::~Disk() {}

class MountLibcrosProxyImpl : public MountLibcrosProxy {
 public:
  virtual void CallMountPath(const char* source_path,
                             MountType type,
                             const MountPathOptions& options,
                             MountCompletedMonitor callback,
                             void* object) OVERRIDE {
    chromeos::MountSourcePath(source_path, type, options, callback, object);
  }

  virtual void CallUnmountPath(const char* path,
                               UnmountRequestCallback callback,
                               void* object) OVERRIDE {
    chromeos::UnmountMountPoint(path, callback, object);
  }

  virtual void CallRequestMountInfo(RequestMountInfoCallback callback,
                                    void* object) OVERRIDE {
    chromeos::RequestMountInfo(callback, object);
  }

  virtual void CallFormatDevice(const char* file_path,
                                const char* filesystem,
                                FormatRequestCallback callback,
                                void* object) OVERRIDE {
    chromeos::FormatDevice(file_path, filesystem, callback, object);
  }

  virtual void CallGetDiskProperties(const char* device_path,
                                     GetDiskPropertiesCallback callback,
                                     void* object) OVERRIDE {
    chromeos::GetDiskProperties(device_path, callback, object);
  }

  virtual MountEventConnection MonitorCrosDisks(
      MountEventMonitor monitor,
      MountCompletedMonitor mount_completed_monitor,
      void* object) OVERRIDE {
    return chromeos::MonitorAllMountEvents(
        monitor, mount_completed_monitor, object);
  }

  virtual void DisconnectCrosDisksMonitorIfSet(
      MountEventConnection conn) OVERRIDE {
    if (conn)
      chromeos::DisconnectMountEventMonitor(conn);
  }
};

class MountLibraryImpl : public MountLibrary {

  struct UnmountDeviceRecursiveCallbackData {
    MountLibraryImpl* const object;
    void* user_data;
    UnmountDeviceRecursiveCallbackType callback;
    size_t pending_callbacks_count;
    bool success;

    UnmountDeviceRecursiveCallbackData(MountLibraryImpl* const o, void* ud,
        UnmountDeviceRecursiveCallbackType cb, int count)
         : object(o),
          user_data(ud),
          callback(cb),
          pending_callbacks_count(count),
          success(true) {
    }
  };

 public:
  MountLibraryImpl() : libcros_proxy_(new MountLibcrosProxyImpl()),
                       mount_status_connection_(NULL) {
  }

  virtual ~MountLibraryImpl() {
      libcros_proxy_->DisconnectCrosDisksMonitorIfSet(
          mount_status_connection_);
  }

  // MountLibrary overrides.
  virtual void Init() OVERRIDE {
    DCHECK(CrosLibrary::Get()->libcros_loaded());
    // Getting the monitor status so that the daemon starts up.
    mount_status_connection_ = libcros_proxy_->MonitorCrosDisks(
        &MonitorMountEventsHandler, &MountCompletedHandler, this);
  }

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual void MountPath(const char* source_path,
                         MountType type,
                         const MountPathOptions& options) OVERRIDE {
    // Hidden and non-existent devices should not be mounted.
    if (type == MOUNT_TYPE_DEVICE) {
      DiskMap::const_iterator it = disks_.find(source_path);
      if (it == disks_.end() || it->second->is_hidden()) {
        MountCompletedHandler(this, MOUNT_ERROR_INTERNAL, source_path, type,
            NULL);
        return;
      }
    }
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    libcros_proxy_->CallMountPath(source_path, type, options,
                                  &MountCompletedHandler, this);
  }

  virtual void UnmountPath(const char* mount_path) OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    libcros_proxy_->CallUnmountPath(mount_path, &UnmountMountPointCallback,
                                    this);
  }

  virtual void GetSizeStatsOnFileThread(const char* mount_path,
      size_t* total_size_kb, size_t* remaining_size_kb) OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    uint64_t total_size_uint64 = 0;
    uint64_t remaining_size_uint64 = 0;

    struct statvfs stat;
    if (statvfs(mount_path, &stat) == 0) {
      total_size_uint64 =
          static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
      remaining_size_uint64 =
          static_cast<uint64_t>(stat.f_bfree) * stat.f_frsize;
    }

    *total_size_kb = static_cast<size_t>(total_size_uint64 / 1024);
    *remaining_size_kb = static_cast<size_t>(remaining_size_uint64 / 1024);
  }

  virtual void FormatUnmountedDevice(const char* file_path) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    for (MountLibrary::DiskMap::iterator it = disks_.begin();
        it != disks_.end(); ++it) {
      if (it->second->file_path().compare(file_path) == 0 &&
          !it->second->mount_path().empty()) {
        OnFormatDevice(file_path,
                       false,
                       MOUNT_METHOD_ERROR_LOCAL,
                       "Device is still mounted.");
        return;
      }
    }
    libcros_proxy_->CallFormatDevice(file_path, "vfat", &FormatDeviceCallback,
                                     this);
  }

  virtual void FormatMountedDevice(const char* mount_path) OVERRIDE {
    DCHECK(mount_path);
    Disk* disk = NULL;
    for (MountLibrary::DiskMap::iterator it = disks_.begin();
         it != disks_.end(); ++it) {
      if (it->second->mount_path().compare(mount_path) == 0) {
        disk = it->second;
        break;
      }
    }
    if (!disk) {
      OnFormatDevice(mount_path,
                     false,
                     MOUNT_METHOD_ERROR_LOCAL,
                     "Device with this mount path not found.");
      return;
    }

    if (formatting_pending_.find(disk->device_path()) !=
        formatting_pending_.end()) {
      OnFormatDevice(mount_path,
                     false,
                     MOUNT_METHOD_ERROR_LOCAL,
                     "Formatting is already pending.");
      return;
    }
    // Formatting process continues, after unmounting.
    formatting_pending_[disk->device_path()] = disk->file_path();
    UnmountPath(disk->mount_path().c_str());
  }

  virtual void UnmountDeviceRecursive(const char* device_path,
      UnmountDeviceRecursiveCallbackType callback, void* user_data)
      OVERRIDE {
    bool success = true;
    const char* error_message = NULL;
    std::vector<const char*> devices_to_unmount;

    // Get list of all devices to unmount.
    int device_path_len = strlen(device_path);
    for (DiskMap::iterator it = disks_.begin(); it != disks_.end(); ++it) {
      if (!it->second->mount_path().empty() &&
          strncmp(device_path, it->second->device_path().c_str(),
                  device_path_len) == 0) {
        devices_to_unmount.push_back(it->second->mount_path().c_str());
      }
    }

    // We should detect at least original device.
    if (devices_to_unmount.size() == 0) {
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
      UnmountDeviceRecursiveCallbackData*
          cb_data = new UnmountDeviceRecursiveCallbackData(this, user_data,
          callback, devices_to_unmount.size());
      for (std::vector<const char*>::iterator it = devices_to_unmount.begin();
           it != devices_to_unmount.end();
           ++it) {
        libcros_proxy_->CallUnmountPath(*it, &UnmountDeviceRecursiveCallback,
                                        cb_data);
      }
    } else {
      LOG(WARNING) << "Unmount recursive request failed for device "
                   << device_path << ", with error: " << error_message;
      callback(user_data, false);
    }
  }

  virtual void RequestMountInfoRefresh() OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    libcros_proxy_->CallRequestMountInfo(RequestMountInfoCallback, this);
  }

  const DiskMap& disks() const OVERRIDE { return disks_; }
  const MountPointMap& mount_points() const OVERRIDE { return mount_points_; }

  virtual void SetLibcrosProxy(MountLibcrosProxy* proxy) OVERRIDE {
    libcros_proxy_->DisconnectCrosDisksMonitorIfSet(mount_status_connection_);
    libcros_proxy_.reset(proxy);
    mount_status_connection_ = libcros_proxy_->MonitorCrosDisks(
        &MonitorMountEventsHandler, &MountCompletedHandler, this);
  }
 private:
  // Callback for MountComplete signal and MountSourcePath method.
  static void MountCompletedHandler(void* object,
                                    MountError error_code,
                                    const char* source_path,
                                    MountType type,
                                    const char* mount_path) {
    DCHECK(object);
    MountCondition mount_condition = MOUNT_CONDITION_NONE;
    if (type == MOUNT_TYPE_DEVICE) {
      if (error_code == MOUNT_ERROR_UNKNOWN_FILESYSTEM)
          mount_condition = MOUNT_CONDITION_UNKNOWN_FILESYSTEM;
      if (error_code == MOUNT_ERROR_UNSUPORTED_FILESYSTEM)
          mount_condition = MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM;
    }

    MountLibraryImpl* self = static_cast<MountLibraryImpl*>(object);
    self->OnMountCompleted(static_cast<MountError>(error_code),
                           MountPointInfo(source_path,
                                          mount_path,
                                          type,
                                          mount_condition));
  }

  // Callback for UnmountRemovableDevice method.
  static void UnmountMountPointCallback(void* object,
                                        const char* mount_path,
                                        MountMethodErrorType error,
                                        const char* error_message) {
    DCHECK(object);
    MountLibraryImpl* self = static_cast<MountLibraryImpl*>(object);
    self->OnUnmountPath(mount_path, error, error_message);
  }

  // Callback for FormatRemovableDevice method.
  static void FormatDeviceCallback(void* object,
                                   const char* file_path,
                                   bool success,
                                   MountMethodErrorType error,
                                   const char* error_message) {
    DCHECK(object);
    MountLibraryImpl* self = static_cast<MountLibraryImpl*>(object);
    const char* device_path = self->FilePathToDevicePath(file_path);
    if (!device_path) {
          LOG(ERROR) << "Error while handling disks metadata. Cannot find "
                     << "device that is being formatted.";
      return;
    }
    self->OnFormatDevice(device_path, success, error, error_message);
  }

  // Callback for UnmountDeviceRecursive.
  static void UnmountDeviceRecursiveCallback(void* object,
                                             const char* mount_path,
                                             MountMethodErrorType error,
                                             const char* error_message) {
    DCHECK(object);
    UnmountDeviceRecursiveCallbackData* cb_data =
        static_cast<UnmountDeviceRecursiveCallbackData*>(object);

    // Do standard processing for Unmount event.
    cb_data->object->OnUnmountPath(mount_path,
                                   error,
                                   error_message);
    if (error == MOUNT_METHOD_ERROR_LOCAL) {
      cb_data->success = false;
    } else if (error == MOUNT_METHOD_ERROR_NONE) {
      LOG(INFO) << mount_path <<  " unmounted.";
    }

    // This is safe as long as all callbacks are called on the same thread as
    // UnmountDeviceRecursive.
    cb_data->pending_callbacks_count--;

    if (cb_data->pending_callbacks_count == 0) {
      cb_data->callback(cb_data->user_data, cb_data->success);
      delete cb_data;
    }
  }

  // Callback for disk information retrieval calls.
  static void GetDiskPropertiesCallback(void* object,
                                        const char* device_path,
                                        const DiskInfo* disk,
                                        MountMethodErrorType error,
                                        const char* error_message) {
    DCHECK(object);
    MountLibraryImpl* self = static_cast<MountLibraryImpl*>(object);
    self->OnGetDiskProperties(device_path,
                              disk,
                              error,
                              error_message);
  }

  // Callback for RequestMountInfo call.
  static void RequestMountInfoCallback(void* object,
                                       const char** devices,
                                       size_t device_len,
                                       MountMethodErrorType error,
                                       const char* error_message) {
    DCHECK(object);
    MountLibraryImpl* self = static_cast<MountLibraryImpl*>(object);
    self->OnRequestMountInfo(devices,
                             device_len,
                             error,
                             error_message);
  }

  // This method will receive events that are caused by drive status changes.
  static void MonitorMountEventsHandler(void* object,
                                        MountEventType evt,
                                        const char* device_path) {
    DCHECK(object);
    MountLibraryImpl* self = static_cast<MountLibraryImpl*>(object);
    self->OnMountEvent(evt, device_path);
  }


  void OnMountCompleted(MountError error_code,
                        const MountPointInfo& mount_info) {
    DCHECK(!mount_info.source_path.empty());

    FireMountCompleted(MOUNTING, error_code, mount_info);

    // If the device is corrupted but it's still possible to format it, it will
    // be fake mounted.
    if ((error_code == MOUNT_ERROR_NONE || mount_info.mount_condition) &&
        mount_points_.find(mount_info.mount_path) == mount_points_.end()) {
      mount_points_.insert(MountPointMap::value_type(
                               mount_info.mount_path.c_str(),
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
      disk->set_mount_path(mount_info.mount_path.c_str());
      FireDiskStatusUpdate(MOUNT_DISK_MOUNTED, disk);
    }
  }

  void OnUnmountPath(const char* mount_path,
                     MountMethodErrorType error,
                     const char* error_message) {
    DCHECK(mount_path);
    if (error == MOUNT_METHOD_ERROR_NONE && mount_path) {
      MountPointMap::iterator mount_points_it = mount_points_.find(mount_path);
      if (mount_points_it == mount_points_.end())
        return;
      // TODO(tbarzic): Add separate, PathUnmounted event to Observer.
      FireMountCompleted(
          UNMOUNTING,
          MOUNT_ERROR_NONE,
          MountPointInfo(mount_points_it->second.source_path.c_str(),
                         mount_points_it->second.mount_path.c_str(),
                         mount_points_it->second.mount_type,
                         mount_points_it->second.mount_condition));
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
      // Check if there is a formatting scheduled
      PathMap::iterator it = formatting_pending_.find(disk->device_path());
      if (it != formatting_pending_.end()) {
        const std::string file_path = it->second;
        formatting_pending_.erase(it);
        FormatUnmountedDevice(file_path.c_str());
      }
    } else {
      LOG(WARNING) << "Unmount request failed for device "
                   << mount_path << ", with error: "
                   << (error_message ? error_message : "Unknown");
    }
  }

  void OnFormatDevice(const char* device_path,
                      bool success,
                      MountMethodErrorType error,
                      const char* error_message) {
    DCHECK(device_path);
    if (error == MOUNT_METHOD_ERROR_NONE && device_path && success) {
      FireDeviceStatusUpdate(MOUNT_FORMATTING_STARTED, device_path);
    } else {
      FireDeviceStatusUpdate(MOUNT_FORMATTING_STARTED,
                             std::string("!") + device_path);
      LOG(WARNING) << "Format request failed for device "
                   << device_path << ", with error: "
                   << (error_message ? error_message : "Unknown");
    }
  }

  void OnGetDiskProperties(const char* device_path,
                           const DiskInfo* disk,
                           MountMethodErrorType error,
                           const char* error_message) {
    DCHECK(device_path);
    if (error == MOUNT_METHOD_ERROR_NONE && device_path) {
      // TODO(zelidrag): Find a better way to filter these out before we
      // fetch the properties:
      // Ignore disks coming from the device we booted the system from.

      // This cast is temporal solution, until we merge DiskInfo and
      // DiskInfoAdvanced into single interface.
      if (disk->on_boot_device())
        return;

      LOG(WARNING) << "Found disk " << device_path;
      // Delete previous disk info for this path:
      bool is_new = true;
      std::string device_path_string(device_path);
      DiskMap::iterator iter = disks_.find(device_path_string);
      if (iter != disks_.end()) {
        delete iter->second;
        disks_.erase(iter);
        is_new = false;
      }

      std::string path;
      std::string mountpath;
      std::string systempath;
      std::string filepath;
      std::string devicelabel;
      std::string drivelabel;
      std::string parentpath;

      if (disk->path() != NULL)
        path = disk->path();

      if (disk->mount_path() != NULL)
        mountpath = disk->mount_path();

      if (disk->system_path() != NULL)
        systempath = disk->system_path();

      if (disk->file_path() != NULL)
        filepath = disk->file_path();

      if (disk->label() != NULL)
        devicelabel = disk->label();

      if (disk->drive_label() != NULL)
        drivelabel = disk->drive_label();

      if (disk->partition_slave() != NULL)
        parentpath = disk->partition_slave();

      Disk* new_disk = new Disk(path,
                                mountpath,
                                systempath,
                                filepath,
                                devicelabel,
                                drivelabel,
                                parentpath,
                                FindSystemPathPrefix(systempath),
                                disk->device_type(),
                                disk->size(),
                                disk->is_drive(),
                                disk->is_read_only(),
                                disk->has_media(),
                                disk->on_boot_device(),
                                disk->is_hidden());
      disks_.insert(
          std::pair<std::string, Disk*>(device_path_string, new_disk));
      FireDiskStatusUpdate(is_new ? MOUNT_DISK_ADDED : MOUNT_DISK_CHANGED,
                           new_disk);
    } else {
      LOG(WARNING) << "Property retrieval request failed for device "
                   << device_path << ", with error: "
                   << (error_message ? error_message : "Unknown");
    }
  }

  void OnRequestMountInfo(const char** devices,
                          size_t devices_len,
                          MountMethodErrorType error,
                          const char* error_message) {
    std::set<std::string> current_device_set;
    if (error == MOUNT_METHOD_ERROR_NONE && devices && devices_len) {
      // Initiate properties fetch for all removable disks,
      for (size_t i = 0; i < devices_len; i++) {
        if (!devices[i]) {
          NOTREACHED();
          continue;
        }
        current_device_set.insert(std::string(devices[i]));
        // Initiate disk property retrieval for each relevant device path.
        libcros_proxy_->CallGetDiskProperties(devices[i],
            &GetDiskPropertiesCallback, this);
      }
    } else if (error != MOUNT_METHOD_ERROR_NONE) {
      LOG(WARNING) << "Request mount info retrieval request failed with error: "
                   << (error_message ? error_message : "Unknown");
    }
    // Search and remove disks that are no longer present.
    for (MountLibrary::DiskMap::iterator iter = disks_.begin();
         iter != disks_.end(); ) {
      if (current_device_set.find(iter->first) == current_device_set.end()) {
        Disk* disk = iter->second;
        FireDiskStatusUpdate(MOUNT_DISK_REMOVED, disk);
        delete iter->second;
        disks_.erase(iter++);
      } else {
        ++iter;
      }
    }
  }

  void OnMountEvent(MountEventType evt,
                    const char* device_path) {
    if (!device_path)
      return;
    MountLibraryEventType type = MOUNT_DEVICE_ADDED;
    switch (evt) {
      case DISK_ADDED: {
        libcros_proxy_->CallGetDiskProperties(device_path,
            &MountLibraryImpl::GetDiskPropertiesCallback, this);
        return;
      }
      case DISK_REMOVED: {
        // Search and remove disks that are no longer present.
        MountLibrary::DiskMap::iterator iter =
            disks_.find(std::string(device_path));
        if (iter != disks_.end()) {
            Disk* disk = iter->second;
            FireDiskStatusUpdate(MOUNT_DISK_REMOVED, disk);
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
        if (!device_path) {
          LOG(ERROR) << "Error while handling disks metadata. Cannot find "
                     << "device that is being formatted.";
          return;
        }
        type = MOUNT_FORMATTING_FINISHED;
        break;
      }
      default: {
        return;
      }
    }
    FireDeviceStatusUpdate(type, std::string(device_path));
  }

  void FireDiskStatusUpdate(MountLibraryEventType evt,
                            const Disk* disk) {
    // Make sure we run on UI thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    FOR_EACH_OBSERVER(
        Observer, observers_, DiskChanged(evt, disk));
  }

  void FireDeviceStatusUpdate(MountLibraryEventType evt,
                              const std::string& device_path) {
    // Make sure we run on UI thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    FOR_EACH_OBSERVER(
        Observer, observers_, DeviceChanged(evt, device_path));
  }

  void FireMountCompleted(MountEvent event_type,
                          MountError error_code,
                          const MountPointInfo& mount_info) {
    // Make sure we run on UI thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    FOR_EACH_OBSERVER(
        Observer, observers_, MountCompleted(event_type,
                                             error_code,
                                             mount_info));
  }

  const char* FilePathToDevicePath(const char* file_path) {
    int failed = (file_path[0] == '!') ? 1 : 0;
    for (MountLibrary::DiskMap::iterator it = disks_.begin();
         it != disks_.end(); ++it) {
      if (it->second->file_path().compare(file_path + failed) == 0) {
        if (failed) {
          return (std::string("!") + it->second->device_path()).c_str();
        } else {
          return it->second->device_path().c_str();
        }
      }
    }
    return NULL;
  }

  const std::string& FindSystemPathPrefix(const std::string& system_path) {
    if (system_path.empty())
      return EmptyString();
    for (SystemPathPrefixSet::const_iterator it = system_path_prefixes_.begin();
         it != system_path_prefixes_.end();
         ++it) {
      if (system_path.find(*it, 0) == 0)
        return *it;
    }
    return EmptyString();
  }

  // Mount event change observers.
  ObserverList<Observer> observers_;

  scoped_ptr<MountLibcrosProxy> libcros_proxy_;

  // A reference to the  mount api, to allow callbacks when the mount
  // status changes.
  MountEventConnection mount_status_connection_;

  // The list of disks found.
  MountLibrary::DiskMap disks_;

  MountLibrary::MountPointMap mount_points_;

  typedef std::set<std::string> SystemPathPrefixSet;
  SystemPathPrefixSet system_path_prefixes_;

  // Set of devices that are supposed to be formated, but are currently waiting
  // to be unmounted. When device is in this map, the formatting process HAVEN'T
  // started yet.
  PathMap formatting_pending_;

  DISALLOW_COPY_AND_ASSIGN(MountLibraryImpl);
};

class MountLibraryStubImpl : public MountLibrary {
 public:
  MountLibraryStubImpl() {}
  virtual ~MountLibraryStubImpl() {}

  // MountLibrary overrides.
  virtual void Init() OVERRIDE {}
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual const DiskMap& disks() const OVERRIDE { return disks_; }
  virtual const MountPointMap& mount_points() const OVERRIDE {
    return mount_points_;
  }
  virtual void RequestMountInfoRefresh() OVERRIDE {}
  virtual void MountPath(const char* source_path, MountType type,
                         const MountPathOptions& options) OVERRIDE {}
  virtual void UnmountPath(const char* mount_path) OVERRIDE {}
  virtual void GetSizeStatsOnFileThread(const char* mount_path,
      size_t* total_size_kb, size_t* remaining_size_kb) OVERRIDE {}
  virtual void FormatUnmountedDevice(const char* device_path) OVERRIDE {}
  virtual void FormatMountedDevice(const char* mount_path) OVERRIDE {}
  virtual void UnmountDeviceRecursive(const char* device_path,
      UnmountDeviceRecursiveCallbackType callback, void* user_data)
      OVERRIDE {}
 private:
  // The list of disks found.
  DiskMap disks_;
  MountPointMap mount_points_;

  DISALLOW_COPY_AND_ASSIGN(MountLibraryStubImpl);
};

// static
MountLibrary* MountLibrary::GetImpl(bool stub) {
  MountLibrary* impl;
  if (stub)
    impl = new MountLibraryStubImpl();
  else
    impl = new MountLibraryImpl();
  impl->Init();
  return impl;
}

}  // namespace chromeos
