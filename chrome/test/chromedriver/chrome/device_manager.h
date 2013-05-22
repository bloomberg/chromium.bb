// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVICE_MANAGER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVICE_MANAGER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/threading/thread.h"

namespace base {
class WaitableEvent;
}  // namespace base

class Status;

class DeviceManager {
 public:
  DeviceManager();
  ~DeviceManager();

  Status StartChrome(const std::string& device_serial,
                     const std::string& package,
                     int port);
  Status GetDevices(std::vector<std::string>* devices);

 private:
  Status ForwardPort(const std::string& device_serial,
                     int local_port,
                     const std::string& remote_abstract);
  Status SetChromeFlags(const std::string& device_serial);
  Status ClearAppData(const std::string& device_serial,
                      const std::string& package);
  Status Launch(const std::string& device_serial, const std::string& package);

  Status Init();
  Status ExecuteCommand(const std::string& command, std::string* response);
  Status ExecuteHostCommand(const std::string& device_serial,
                            const std::string& host_command,
                            std::string* response);
  Status ExecuteHostShellCommand(const std::string& device_serial,
                                 const std::string& shell_command,
                                 std::string* response);
  void ExecuteCommandOnIOThread(const std::string& command,
                                std::string* response,
                                bool* success,
                                base::WaitableEvent* event);

  base::Thread thread_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVICE_MANAGER_H_
