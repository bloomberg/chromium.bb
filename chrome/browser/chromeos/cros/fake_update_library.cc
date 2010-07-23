// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/fake_update_library.h"

namespace chromeos {

void FakeUpdateLibrary::AddObserver(Observer* observer) {
}

void FakeUpdateLibrary::RemoveObserver(Observer* observer) {
}

const UpdateLibrary::Status& FakeUpdateLibrary::status() const {
  return status_;
}

}  // namespace chromeos
