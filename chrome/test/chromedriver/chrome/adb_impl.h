// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_ADB_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_ADB_IMPL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/test/chromedriver/chrome/adb.h"

namespace base {
class MessageLoopProxy;
}

class Log;
class Status;

class AdbImpl : public Adb {
 public:
  explicit AdbImpl(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy,
      Log* log);
  virtual ~AdbImpl();

  // Overridden from Adb:
  virtual Status GetDevices(std::vector<std::string>* devices) OVERRIDE;
  virtual Status ForwardPort(const std::string& device_serial,
                             int local_port,
                             const std::string& remote_abstract) OVERRIDE;
  virtual Status SetChromeArgs(const std::string& device_serial,
                               const std::string& args) OVERRIDE;
  virtual Status CheckAppInstalled(const std::string& device_serial,
                                   const std::string& package) OVERRIDE;
  virtual Status ClearAppData(const std::string& device_serial,
                              const std::string& package) OVERRIDE;
  virtual Status Launch(const std::string& device_serial,
                        const std::string& package,
                        const std::string& activity) OVERRIDE;
  virtual Status ForceStop(const std::string& device_serial,
                           const std::string& package) OVERRIDE;

 private:
  Status ExecuteCommand(const std::string& command,
                        std::string* response);
  Status ExecuteHostCommand(const std::string& device_serial,
                            const std::string& host_command,
                            std::string* response);
  Status ExecuteHostShellCommand(const std::string& device_serial,
                                 const std::string& shell_command,
                                 std::string* response);

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  Log* log_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_ADB_IMPL_H_
