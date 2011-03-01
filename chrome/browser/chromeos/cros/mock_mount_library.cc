// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mock_mount_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

using testing::_;
using testing::Invoke;

const char* kTestSystemPath = "/this/system/path";
const char* kTestDevicePath = "/this/device/path";
const char* kTestMountPath = "/media/foofoo";

void MockMountLibrary::AddObserverInternal(MountLibrary::Observer* observer) {
  observers_.AddObserver(observer);
}

void MockMountLibrary::RemoveObserverInternal(
    MountLibrary::Observer* observer) {
  observers_.RemoveObserver(observer);
}

MockMountLibrary::MockMountLibrary() {
  ON_CALL(*this, AddObserver(_))
      .WillByDefault(Invoke(this, &MockMountLibrary::AddObserverInternal));
  ON_CALL(*this, RemoveObserver(_))
      .WillByDefault(Invoke(this, &MockMountLibrary::RemoveObserverInternal));
  ON_CALL(*this, disks())
      .WillByDefault(Invoke(this, &MockMountLibrary::disksInternal));
}

MockMountLibrary::~MockMountLibrary() {

}

void MockMountLibrary::FireDeviceInsertEvents() {

  disks_.clear();

  disks_.push_back(Disk(kTestDevicePath, "",  kTestSystemPath, false, true));

  // Device Added
  chromeos::MountEventType evt;
  evt = chromeos::DEVICE_ADDED;
  UpdateMountStatus(evt, kTestSystemPath);

  // Disk Added
  evt = chromeos::DISK_ADDED;
  UpdateMountStatus(evt, kTestDevicePath);

  // Disk Changed
  disks_.clear();
  disks_.push_back(Disk(
      kTestDevicePath, kTestMountPath, kTestSystemPath, false, true));
  evt = chromeos::DISK_CHANGED;
  UpdateMountStatus(evt, kTestDevicePath);
}

void MockMountLibrary::FireDeviceRemoveEvents() {
  disks_.clear();
  chromeos::MountEventType evt;
  evt = chromeos::DISK_REMOVED;
  UpdateMountStatus(evt, kTestDevicePath);
}

void MockMountLibrary::UpdateMountStatus(MountEventType evt,
                                         const std::string& path) {
  // Make sure we run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, MountChanged(this, evt, path));
}

}  // namespace chromeos
