// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mount_library.h"

#include <set>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

class MountLibraryImpl : public MountLibrary {
 public:
  MountLibraryImpl() : mount_status_connection_(NULL) {
    if (CrosLibrary::Get()->EnsureLoaded())
      Init();
    else
      LOG(ERROR) << "Cros Library has not been loaded";
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
    MountRemovableDevice(device_path,
                         &MountLibraryImpl::MountRemovableDeviceCallback,
                         this);
  }

  virtual void UnmountPath(const char* device_path) OVERRIDE {
    UnmountRemovableDevice(device_path,
                           &MountLibraryImpl::UnmountRemovableDeviceCallback,
                           this);
  }

  virtual void RequestMountInfoRefresh() OVERRIDE {
    RequestMountInfo(&MountLibraryImpl::RequestMountInfoCallback,
                     this);
  }

  virtual void RefreshDiskProperties(const Disk* disk) OVERRIDE {
    DCHECK(disk);
    GetDiskProperties(disk->device_path().c_str(),
                      &MountLibraryImpl::GetDiskPropertiesCallback,
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
  virtual bool IsBootPath(const char* device_path) OVERRIDE { return true; }

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

