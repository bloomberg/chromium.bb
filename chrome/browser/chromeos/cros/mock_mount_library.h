// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_MOUNT_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_MOUNT_LIBRARY_H_

#include <string>

#include "chrome/browser/chromeos/cros/mount_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockMountLibrary : public MountLibrary {
 public:
  MockMountLibrary() {}
  virtual ~MockMountLibrary() {}
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_CONST_METHOD0(disks, const DiskVector&(void));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_MOUNT_LIBRARY_H_
