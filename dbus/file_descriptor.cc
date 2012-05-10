// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/platform_file.h"
#include "dbus/file_descriptor.h"

namespace dbus {

FileDescriptor::~FileDescriptor() {
  if (owner_)
    base::ClosePlatformFile(value_);
}

int FileDescriptor::value() const {
  CHECK(valid_);
  return value_;
}

int FileDescriptor::TakeValue() {
  CHECK(valid_);  // NB: check first so owner_ is unchanged if this triggers
  owner_ = false;
  return value_;
}

void FileDescriptor::CheckValidity() {
  base::PlatformFileInfo info;
  bool ok = base::GetPlatformFileInfo(value_, &info);
  valid_ = (ok && !info.is_directory);
}

}  // namespace dbus
