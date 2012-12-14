// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"

namespace drive {

FakeFreeDiskSpaceGetter::FakeFreeDiskSpaceGetter() {
}

FakeFreeDiskSpaceGetter::~FakeFreeDiskSpaceGetter() {
}

int64 FakeFreeDiskSpaceGetter::AmountOfFreeDiskSpace() {
  if (fake_values_.empty())
    return 0;

  const int64 value = fake_values_[0];
  // We'll keep the last value.
  if (fake_values_.size() > 1)
    fake_values_.erase(fake_values_.begin());

  return value;
}

}  // namespace drive
