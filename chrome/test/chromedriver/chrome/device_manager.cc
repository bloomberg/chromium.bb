// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/device_manager.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/test/chromedriver/chrome/adb.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

std::string GetActivityForPackage(const std::string& package) {
  if (package == "org.chromium.chrome.testshell")
    return ".ChromiumTestShellActivity";
  return "com.google.android.apps.chrome.Main";
}

std::string GetDevtoolsSocket(const std::string& package) {
  if (package == "org.chromium.chrome.testshell")
    return "chromium_testshell_devtools_remote";
  return "chrome_devtools_remote";
}

}  // namespace

Device::Device(
    const std::string& device_serial, Adb* adb,
    base::Callback<void()> release_callback)
    : serial_(device_serial),
      adb_(adb),
      release_callback_(release_callback) {}

Device::~Device() {
  release_callback_.Run();
}

Status Device::StartChrome(const std::string& package,
                           int port,
                           const std::string& args) {
  if (!active_package_.empty())
    return Status(kUnknownError,
        active_package_ + " was launched and has not been quit");
  Status status = adb_->CheckAppInstalled(serial_, package);
  if (!status.IsOk())
    return status;
  status = adb_->ClearAppData(serial_, package);
  if (!status.IsOk())
    return status;
  status = adb_->SetChromeArgs(serial_, args);
  if (!status.IsOk())
    return status;
  status = adb_->Launch(serial_, package, GetActivityForPackage(package));
  if (!status.IsOk())
    return status;
  active_package_ = package;
  return adb_->ForwardPort(serial_, port, GetDevtoolsSocket(package));
}

Status Device::StopChrome() {
  if (active_package_.empty())
    return Status(kUnknownError, "No package has been launched");
  std::string response;
  Status status = adb_->ForceStop(serial_, active_package_);
  if (!status.IsOk())
    return status;
  active_package_ = "";
  return Status(kOk);
}

DeviceManager::DeviceManager(Adb* adb) : adb_(adb) {
  CHECK(adb_);
}

DeviceManager::~DeviceManager() {}

Status DeviceManager::AcquireDevice(scoped_ptr<Device>* device) {
  std::vector<std::string> devices;
  Status status = adb_->GetDevices(&devices);
  if (!status.IsOk())
    return status;

  base::AutoLock lock(devices_lock_);
  status = Status(kUnknownError, "No device avaliable");
  std::vector<std::string>::iterator iter;
  for (iter = devices.begin(); iter != devices.end(); iter++) {
    if (!IsDeviceLocked(*iter)) {
      device->reset(LockDevice(*iter));
      status = Status(kOk);
      break;
    }
  }
  return status;
}

Status DeviceManager::AcquireSpecificDevice(
    const std::string& device_serial, scoped_ptr<Device>* device) {
  std::vector<std::string> devices;
  Status status = adb_->GetDevices(&devices);
  if (!status.IsOk())
    return status;

  if (std::find(devices.begin(), devices.end(), device_serial) == devices.end())
    return Status(kUnknownError,
        "Device " + device_serial + " is not avaliable");

  base::AutoLock lock(devices_lock_);
  if (IsDeviceLocked(device_serial)) {
    status = Status(kUnknownError,
        "Device " + device_serial + " already has an active session");
  } else {
    device->reset(LockDevice(device_serial));
    status = Status(kOk);
  }
  return status;
}

void DeviceManager::ReleaseDevice(const std::string& device_serial) {
  base::AutoLock lock(devices_lock_);
  active_devices_.remove(device_serial);
}

Device* DeviceManager::LockDevice(const std::string& device_serial) {
  active_devices_.push_back(device_serial);
  return new Device(device_serial, adb_,
      base::Bind(&DeviceManager::ReleaseDevice, base::Unretained(this),
                 device_serial));
}

bool DeviceManager::IsDeviceLocked(const std::string& device_serial) {
  return std::find(active_devices_.begin(), active_devices_.end(),
                   device_serial) != active_devices_.end();
}

