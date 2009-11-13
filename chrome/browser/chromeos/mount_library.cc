// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mount_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros_library.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<chromeos::MountLibrary> {
  void RetainCallee(chromeos::MountLibrary* obj) {}
  void ReleaseCallee(chromeos::MountLibrary* obj) {}
};

namespace chromeos {

// static
MountLibrary* MountLibrary::Get() {
  return Singleton<MountLibrary>::get();
}

// static
bool MountLibrary::EnsureLoaded() {
  return CrosLibrary::EnsureLoaded();
}

void MountLibrary::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MountLibrary::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MountLibrary::ParseDisks(const MountStatus& status) {
  disks_.clear();
  for (int i = 0; i < status.size; i++) {
    std::string path;
    std::string mountpath;
    if (status.disks[i].path != NULL) {
      path = status.disks[i].path;
    }
    if (status.disks[i].mountpath != NULL) {
      mountpath = status.disks[i].mountpath;
    }
    disks_.push_back(Disk(path, mountpath));
  }
}

MountLibrary::MountLibrary() {
  if (CrosLibrary::EnsureLoaded()) {
    Init();
  } else {
    LOG(ERROR) << "Cros Library has not been loaded";
  }
}

MountLibrary::~MountLibrary() {
  if (CrosLibrary::EnsureLoaded()) {
    DisconnectMountStatus(mount_status_connection_);
  }
}

// static
void MountLibrary::MountStatusChangedHandler(void* object,
                                             const MountStatus& status,
                                             MountEventType evt,
                                             const  char* path) {
  MountLibrary* mount = static_cast<MountLibrary*>(object);
  std::string devicepath = path;
  mount->ParseDisks(status);
  mount->UpdateMountStatus(status, evt, devicepath);
}

void MountLibrary::Init() {
  // Getting the monitor status so that the daemon starts up.
  MountStatus* mount = RetrieveMountInformation();
  FreeMountStatus(mount);

  mount_status_connection_ = MonitorMountStatus(
      &MountStatusChangedHandler, this);
}

void MountLibrary::UpdateMountStatus(const MountStatus& status,
                                     MountEventType evt,
                                     const std::string& path) {
  // Make sure we run on UI thread.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, MountChanged(this, evt, path));
}

}  // namespace chromeos
