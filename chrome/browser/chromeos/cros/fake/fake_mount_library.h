// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_MOUNT_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_MOUNT_LIBRARY_H_

#include "chrome/browser/chromeos/cros/mount_library.h"
#include "third_party/cros/chromeos_mount.h"

namespace chromeos {

class FakeMountLibrary : public MountLibrary {
 public:
  FakeMountLibrary() {}
  virtual ~FakeMountLibrary() {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual const DiskVector& disks() const {
    return disks_;
  }
  virtual bool MountPath(const char* device_path) {
    return true;
  }

 private:
  DiskVector disks_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_MOUNT_LIBRARY_H_
