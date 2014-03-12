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
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/chromedriver/chrome/adb.h"
#include "chrome/test/chromedriver/chrome/status.h"

// TODO(craigdh): Remove once Chromedriver no longer supports pre-m33 Chrome.
const char* kChromeCmdLineFileBeforeM33 = "/data/local/chrome-command-line";
const char* kChromeCmdLineFile = "/data/local/tmp/chrome-command-line";

Device::Device(
    const std::string& device_serial, Adb* adb,
    base::Callback<void()> release_callback)
    : serial_(device_serial),
      adb_(adb),
      release_callback_(release_callback) {}

Device::~Device() {
  release_callback_.Run();
}

Status Device::SetUp(const std::string& package,
                     const std::string& activity,
                     const std::string& process,
                     const std::string& args,
                     bool use_running_app,
                     int port) {
  if (!active_package_.empty())
    return Status(kUnknownError,
        active_package_ + " was launched and has not been quit");

  Status status = adb_->CheckAppInstalled(serial_, package);
  if (status.IsError())
    return status;

  std::string known_activity;
  std::string command_line_file;
  std::string device_socket;
  std::string exec_name;
  if (package.compare("org.chromium.content_shell_apk") == 0) {
    // Chromium content shell.
    known_activity = ".ContentShellActivity";
    device_socket = "content_shell_devtools_remote";
    command_line_file = "/data/local/tmp/content-shell-command-line";
    exec_name = "content_shell";
  } else if (package.compare("org.chromium.chrome.shell") == 0) {
    // ChromeShell
    known_activity = ".ChromeShellActivity";
    device_socket = "chrome_shell_devtools_remote";
    command_line_file = "/data/local/tmp/chrome-shell-command-line";
    exec_name = "chrome_shell";
  } else if (package.find("chrome") != std::string::npos &&
             package.find("webview") == std::string::npos) {
    // Chrome.
    known_activity = "com.google.android.apps.chrome.Main";
    device_socket = "chrome_devtools_remote";
    command_line_file = kChromeCmdLineFileBeforeM33;
    exec_name = "chrome";
    status = adb_->SetDebugApp(serial_, package);
    if (status.IsError())
      return status;
  }

  if (!use_running_app) {
    status = adb_->ClearAppData(serial_, package);
    if (status.IsError())
      return status;

    if (!known_activity.empty()) {
      if (!activity.empty() ||
          !process.empty())
        return Status(kUnknownError, "known package " + package +
                      " does not accept activity/process");
    } else if (activity.empty()) {
      return Status(kUnknownError, "WebView apps require activity name");
    }

    if (!command_line_file.empty()) {
      // If Chrome is set as the debug app it looks in /data/local/tmp/.
      // There's no way to know if this is set, so write to both locations.
      // This can be removed once support for pre-M33 is no longer needed.
      if (command_line_file == kChromeCmdLineFileBeforeM33) {
        status = adb_->SetCommandLineFile(
            serial_, kChromeCmdLineFileBeforeM33, exec_name, args);
        Status status2 = adb_->SetCommandLineFile(
            serial_, kChromeCmdLineFile, exec_name, args);
        if (status.IsError() && status2.IsError())
          return Status(kUnknownError,
              "Failed to set Chrome's command line file on device " + serial_);
      } else {
        status = adb_->SetCommandLineFile(
            serial_, command_line_file, exec_name, args);
        if (status.IsError())
          return status;
      }
    }

    status = adb_->Launch(serial_, package,
                          known_activity.empty() ? activity : known_activity);
    if (status.IsError())
      return status;

    active_package_ = package;
  }
  this->ForwardDevtoolsPort(package, process, device_socket, port);

  return status;
}

Status Device::ForwardDevtoolsPort(const std::string& package,
                                   const std::string& process,
                                   std::string& device_socket,
                                   int port) {
  if (device_socket.empty()) {
    // Assume this is a WebView app.
    int pid;
    Status status = adb_->GetPidByName(serial_,
                                       process.empty() ? package : process,
                                       &pid);
    if (status.IsError()) {
      if (process.empty())
        status.AddDetails(
            "process name must be specified if not equal to package name");
      return status;
    }
    device_socket = base::StringPrintf("webview_devtools_remote_%d", pid);
  }

  return adb_->ForwardPort(serial_, port, device_socket);
}

Status Device::TearDown() {
  if (!active_package_.empty()) {
    std::string response;
    Status status = adb_->ForceStop(serial_, active_package_);
    if (status.IsError())
      return status;
    active_package_ = "";
  }
  return Status(kOk);
}

DeviceManager::DeviceManager(Adb* adb) : adb_(adb) {
  CHECK(adb_);
}

DeviceManager::~DeviceManager() {}

Status DeviceManager::AcquireDevice(scoped_ptr<Device>* device) {
  std::vector<std::string> devices;
  Status status = adb_->GetDevices(&devices);
  if (status.IsError())
    return status;

  if (devices.empty())
    return Status(kUnknownError, "There are no devices online");

  base::AutoLock lock(devices_lock_);
  status = Status(kUnknownError, "All devices are in use (" +
                  base::IntToString(devices.size()) + " online)");
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
  if (status.IsError())
    return status;

  if (std::find(devices.begin(), devices.end(), device_serial) == devices.end())
    return Status(kUnknownError,
        "Device " + device_serial + " is not online");

  base::AutoLock lock(devices_lock_);
  if (IsDeviceLocked(device_serial)) {
    status = Status(kUnknownError,
        "Device " + device_serial + " is already in use");
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

