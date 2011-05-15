// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mount_library.h"

#include <set>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"

const char* kLibraryNotLoaded = "Cros Library not loaded";
const char* kDeviceNotFound = "Device could not be found";

namespace chromeos {

MountLibrary::Disk::Disk(const std::string& device_path,
     const std::string& mount_path,
     const std::string& system_path,
     const std::string& file_path,
     const std::string& device_label,
     const std::string& drive_label,
     const std::string& parent_path,
     DeviceType device_type,
     uint64 total_size,
     bool is_parent,
     bool is_read_only,
     bool has_media,
     bool on_boot_device)
    : device_path_(device_path),
      mount_path_(mount_path),
      system_path_(system_path),
      file_path_(file_path),
      device_label_(device_label),
      drive_label_(drive_label),
      parent_path_(parent_path),
      device_type_(device_type),
      total_size_(total_size),
      is_parent_(is_parent),
      is_read_only_(is_read_only),
      has_media_(has_media),
      on_boot_device_(on_boot_device) {
  // Add trailing slash to mount path.
  if (mount_path_.length() && mount_path_.at(mount_path_.length() -1) != '/')
    mount_path_ = mount_path_.append("/");
}

MountLibrary::Disk::~Disk() {}

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
  MountLibraryImpl() : mount_status_connection_(NULL) {
    if (CrosLibrary::Get()->EnsureLoaded())
      Init();
    else
      LOG(ERROR) << kLibraryNotLoaded;
  }

  virtual ~MountLibraryImpl() {
    if (mount_status_connection_)
      DisconnectMountEventMonitor(mount_status_connection_);
  }

  // MountLibrary overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual void MountPath(const char* device_path) OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!CrosLibrary::Get()->EnsureLoaded()) {
      OnMountRemovableDevice(device_path,
                             NULL,
                             MOUNT_METHOD_ERROR_LOCAL,
                             kLibraryNotLoaded);
      return;
    }
    MountRemovableDevice(device_path,
                         &MountLibraryImpl::MountRemovableDeviceCallback,
                         this);
  }

  virtual void UnmountPath(const char* device_path) OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!CrosLibrary::Get()->EnsureLoaded()) {
      OnUnmountRemovableDevice(device_path,
                               MOUNT_METHOD_ERROR_LOCAL,
                               kLibraryNotLoaded);
      return;
    }
    UnmountRemovableDevice(device_path,
                           &MountLibraryImpl::UnmountRemovableDeviceCallback,
                           this);
  }

  virtual void UnmountDeviceRecursive(const char* device_path,
      UnmountDeviceRecursiveCallbackType callback, void* user_data)
      OVERRIDE {
    bool success = true;
    const char* error_message = NULL;
    std::vector<const char*> devices_to_unmount;

    if (!CrosLibrary::Get()->EnsureLoaded()) {
      success = false;
      error_message = kLibraryNotLoaded;
    } else {
      // Get list of all devices to unmount.
      int device_path_len = strlen(device_path);
      for (DiskMap::iterator it = disks_.begin(); it != disks_.end(); ++it) {
        if (strncmp(device_path, it->second->device_path().c_str(),
            device_path_len) == 0) {
          devices_to_unmount.push_back(it->second->device_path().c_str());
        }
      }

      // We should detect at least original device.
      if (devices_to_unmount.size() == 0) {
        success = false;
        error_message = kDeviceNotFound;
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
        UnmountRemovableDevice(*it,
            &MountLibraryImpl::UnmountDeviceRecursiveCallback,
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
    if (!CrosLibrary::Get()->EnsureLoaded()) {
      OnRequestMountInfo(NULL,
                         0,
                         MOUNT_METHOD_ERROR_LOCAL,
                         kLibraryNotLoaded);
      return;
    }
    RequestMountInfo(&MountLibraryImpl::RequestMountInfoCallback,
                     this);
  }

  const DiskMap& disks() const OVERRIDE { return disks_; }

 private:
  // Callback for MountRemovableDevice method.
  static void MountRemovableDeviceCallback(void* object,
                                           const char* device_path,
                                           const char* mount_path,
                                           MountMethodErrorType error,
                                           const char* error_message) {
    DCHECK(object);
    MountLibraryImpl* self = static_cast<MountLibraryImpl*>(object);
    self->OnMountRemovableDevice(device_path,
                                 mount_path,
                                 error,
                                 error_message);
  }

  // Callback for UnmountRemovableDevice method.
  static void UnmountRemovableDeviceCallback(void* object,
                                             const char* device_path,
                                             const char* mount_path,
                                             MountMethodErrorType error,
                                             const char* error_message) {
    DCHECK(object);
    MountLibraryImpl* self = static_cast<MountLibraryImpl*>(object);
    self->OnUnmountRemovableDevice(device_path,
                                   error,
                                   error_message);
  }

  // Callback for UnmountDeviceRecursive.
  static void UnmountDeviceRecursiveCallback(void* object,
                                             const char* device_path,
                                             const char* mount_path,
                                             MountMethodErrorType error,
                                             const char* error_message) {
    DCHECK(object);
    UnmountDeviceRecursiveCallbackData* cb_data =
        static_cast<UnmountDeviceRecursiveCallbackData*>(object);

    // Do standard processing for Unmount event.
    cb_data->object->OnUnmountRemovableDevice(device_path,
                                              error,
                                              error_message);
    if (error == MOUNT_METHOD_ERROR_LOCAL) {
      cb_data->success = false;
    } else if (error == MOUNT_METHOD_ERROR_NONE) {
      LOG(WARNING) << device_path <<  " unmounted.";
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


  void OnMountRemovableDevice(const char* device_path,
                              const char* mount_path,
                              MountMethodErrorType error,
                              const char* error_message) {
    DCHECK(device_path);

    if (error == MOUNT_METHOD_ERROR_NONE && device_path && mount_path) {
      std::string path(device_path);
      DiskMap::iterator iter = disks_.find(path);
      if (iter == disks_.end()) {
        // disk might have been removed by now?
        return;
      }
      Disk* disk = iter->second;
      DCHECK(disk);
      disk->set_mount_path(mount_path);
      FireDiskStatusUpdate(MOUNT_DISK_MOUNTED, disk);
    } else {
      LOG(WARNING) << "Mount request failed for device "
                   << device_path << ", with error: "
                   << (error_message ? error_message : "Unknown");
    }
  }

  void OnUnmountRemovableDevice(const char* device_path,
                                MountMethodErrorType error,
                                const char* error_message) {
    DCHECK(device_path);
    if (error == MOUNT_METHOD_ERROR_NONE && device_path) {
      std::string path(device_path);
      DiskMap::iterator iter = disks_.find(path);
      if (iter == disks_.end()) {
        // disk might have been removed by now?
        return;
      }
      Disk* disk = iter->second;
      DCHECK(disk);
      disk->clear_mount_path();
      FireDiskStatusUpdate(MOUNT_DISK_UNMOUNTED, disk);
    } else {
      LOG(WARNING) << "Unmount request failed for device "
                   << device_path << ", with error: "
                   << (error_message ? error_message : "Unknown");
    }
  }

  void OnGetDiskProperties(const char* device_path,
                           const DiskInfo* disk1,
                           MountMethodErrorType error,
                           const char* error_message) {
    DCHECK(device_path);
    if (error == MOUNT_METHOD_ERROR_NONE && device_path) {
      // TODO(zelidrag): Find a better way to filter these out before we
      // fetch the properties:
      // Ignore disks coming from the device we booted the system from.

      // This cast is temporal solution, until we merge DiskInfo and
      // DiskInfoAdvanced into single interface.
      const DiskInfoAdvanced* disk =
          reinterpret_cast<const DiskInfoAdvanced*>(disk1);
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
                                disk->device_type(),
                                disk->size(),
                                disk->is_drive(),
                                disk->is_read_only(),
                                disk->has_media(),
                                disk->on_boot_device());
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
      bool found_disk = false;
      for (size_t i = 0; i < devices_len; i++) {
        if (!devices[i]) {
          NOTREACHED();
          continue;
        }
        current_device_set.insert(std::string(devices[i]));
        found_disk = true;
        // Initiate disk property retrieval for each relevant device path.
        GetDiskProperties(devices[i],
                          &MountLibraryImpl::GetDiskPropertiesCallback,
                          this);
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
      case DISK_ADDED:
      case DISK_CHANGED: {
        GetDiskProperties(device_path,
                          &MountLibraryImpl::GetDiskPropertiesCallback,
                          this);
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
        break;
      }
      case DEVICE_REMOVED: {
        type = MOUNT_DEVICE_REMOVED;
        break;
      }
      case DEVICE_SCANNED: {
        type = MOUNT_DEVICE_SCANNED;
        break;
      }
    }
    FireDeviceStatusUpdate(type, std::string(device_path));
  }

  void Init() {
    // Getting the monitor status so that the daemon starts up.
    mount_status_connection_ = MonitorMountEvents(
        &MonitorMountEventsHandler, this);
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

  // Mount event change observers.
  ObserverList<Observer> observers_;

  // A reference to the  mount api, to allow callbacks when the mount
  // status changes.
  MountEventConnection mount_status_connection_;

  // The list of disks found.
  MountLibrary::DiskMap disks_;

  DISALLOW_COPY_AND_ASSIGN(MountLibraryImpl);
};

class MountLibraryStubImpl : public MountLibrary {
 public:
  MountLibraryStubImpl() {}
  virtual ~MountLibraryStubImpl() {}

  // MountLibrary overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual const DiskMap& disks() const OVERRIDE { return disks_; }
  virtual void RequestMountInfoRefresh() OVERRIDE {}
  virtual void MountPath(const char* device_path) OVERRIDE {}
  virtual void UnmountPath(const char* device_path) OVERRIDE {}
  virtual void UnmountDeviceRecursive(const char* device_path,
      UnmountDeviceRecursiveCallbackType callback, void* user_data)
      OVERRIDE {}

 private:
  // The list of disks found.
  DiskMap disks_;

  DISALLOW_COPY_AND_ASSIGN(MountLibraryStubImpl);
};

// static
MountLibrary* MountLibrary::GetImpl(bool stub) {
  if (stub)
    return new MountLibraryStubImpl();
  else
    return new MountLibraryImpl();
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::MountLibraryImpl);

