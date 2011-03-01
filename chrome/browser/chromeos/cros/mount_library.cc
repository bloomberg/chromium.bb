// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mount_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

class MountLibraryImpl : public MountLibrary {
 public:
  MountLibraryImpl() : mount_status_connection_(NULL) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      Init();
    } else {
      LOG(ERROR) << "Cros Library has not been loaded";
    }
  }

  ~MountLibraryImpl() {
    if (mount_status_connection_) {
      DisconnectMountStatus(mount_status_connection_);
    }
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  bool MountPath(const char* device_path) {
    return MountDevicePath(device_path);
  }

  bool IsBootPath(const char* device_path) {
    return IsBootDevicePath(device_path);
  }

  const DiskVector& disks() const { return disks_; }

 private:
  void ParseDisks(const MountStatus& status) {
    disks_.clear();
    for (int i = 0; i < status.size; i++) {
      std::string path;
      std::string mountpath;
      std::string systempath;
      bool parent;
      bool hasmedia;
      if (status.disks[i].path != NULL) {
        path = status.disks[i].path;
      }
      if (status.disks[i].mountpath != NULL) {
        mountpath = status.disks[i].mountpath;
      }
      if (status.disks[i].systempath != NULL) {
        systempath = status.disks[i].systempath;
      }
      parent = status.disks[i].isparent;
      hasmedia = status.disks[i].hasmedia;
      disks_.push_back(Disk(path,
                            mountpath,
                            systempath,
                            parent,
                            hasmedia));
    }
  }

  static void MountStatusChangedHandler(void* object,
                                        const MountStatus& status,
                                        MountEventType evt,
                                        const  char* path) {
    MountLibraryImpl* mount = static_cast<MountLibraryImpl*>(object);
    std::string devicepath = path;
    mount->ParseDisks(status);
    mount->UpdateMountStatus(status, evt, devicepath);
  }

  void Init() {
    // Getting the monitor status so that the daemon starts up.
    MountStatus* mount = RetrieveMountInformation();
    if (!mount) {
      LOG(ERROR) << "Failed to retrieve mount information";
      return;
    }
    ParseDisks(*mount);
    FreeMountStatus(mount);

    mount_status_connection_ = MonitorMountStatus(
        &MountStatusChangedHandler, this);
  }

  void UpdateMountStatus(const MountStatus& status,
                                       MountEventType evt,
                                       const std::string& path) {
    // Make sure we run on UI thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    FOR_EACH_OBSERVER(
        Observer, observers_, MountChanged(this, evt, path));
  }
  ObserverList<Observer> observers_;

  // A reference to the  mount api, to allow callbacks when the mount
  // status changes.
  MountStatusConnection mount_status_connection_;

  // The list of disks found.
  DiskVector disks_;

  DISALLOW_COPY_AND_ASSIGN(MountLibraryImpl);
};

class MountLibraryStubImpl : public MountLibrary {
 public:
  MountLibraryStubImpl() {}
  virtual ~MountLibraryStubImpl() {}

  // MountLibrary overrides.
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual const DiskVector& disks() const { return disks_; }
  virtual bool MountPath(const char* device_path) { return false; }
  virtual bool IsBootPath(const char* device_path) { return true; }

 private:
  // The list of disks found.
  DiskVector disks_;

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

