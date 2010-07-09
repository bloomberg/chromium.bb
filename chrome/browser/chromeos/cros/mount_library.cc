// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mount_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::MountLibraryImpl);

namespace chromeos {

void MountLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MountLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MountLibraryImpl::MountPath(const char* device_path) {
  return MountDevicePath(device_path);
}

void MountLibraryImpl::ParseDisks(const MountStatus& status) {
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

MountLibraryImpl::MountLibraryImpl() : mount_status_connection_(NULL) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    Init();
  } else {
    LOG(ERROR) << "Cros Library has not been loaded";
  }
}

MountLibraryImpl::~MountLibraryImpl() {
  if (mount_status_connection_) {
    DisconnectMountStatus(mount_status_connection_);
  }
}

// static
void MountLibraryImpl::MountStatusChangedHandler(void* object,
                                                 const MountStatus& status,
                                                 MountEventType evt,
                                                 const  char* path) {
  MountLibraryImpl* mount = static_cast<MountLibraryImpl*>(object);
  std::string devicepath = path;
  mount->ParseDisks(status);
  mount->UpdateMountStatus(status, evt, devicepath);
}

void MountLibraryImpl::Init() {
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

void MountLibraryImpl::UpdateMountStatus(const MountStatus& status,
                                     MountEventType evt,
                                     const std::string& path) {
  // Make sure we run on UI thread.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, MountChanged(this, evt, path));
}

}  // namespace chromeos
