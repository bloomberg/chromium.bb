// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/fake_mount_library.h"

namespace chromeos {

void FakeMountLibrary::AddObserver(Observer* observer) {
}

void FakeMountLibrary::RemoveObserver(Observer* observer) {
}

bool FakeMountLibrary::MountPath(const char* device_path) {
  return true;
}

}  // namespace chromeos
