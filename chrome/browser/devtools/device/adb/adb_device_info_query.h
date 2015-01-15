// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_DEVICE_INFO_QUERY_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_DEVICE_INFO_QUERY_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/devtools/device/android_device_manager.h"

class AdbDeviceInfoQuery : public base::RefCounted<AdbDeviceInfoQuery> {
 public:
  static AndroidDeviceManager::BrowserInfo::Type GetBrowserType(
      const std::string& socket);

  static std::string GetDisplayName(const std::string& socket,
                                    const std::string& package);

  typedef AndroidDeviceManager::CommandCallback CommandCallback;
  typedef AndroidDeviceManager::DeviceInfoCallback DeviceInfoCallback;

  typedef base::Callback<
      void(const std::string&, const CommandCallback&)> RunCommandCallback;

  static void Start(const RunCommandCallback& command_callback,
                    const DeviceInfoCallback& callback);

 private:
  friend class base::RefCounted<AdbDeviceInfoQuery>;

  AdbDeviceInfoQuery(const RunCommandCallback& command_callback,
                     const DeviceInfoCallback& callback);

  virtual ~AdbDeviceInfoQuery();

  void ReceivedModel(int result, const std::string& response);
  void ReceivedWindowPolicy(int result, const std::string& response);
  void ReceivedProcesses(int result, const std::string& response);
  void ReceivedSockets(int result, const std::string& response);
  void ReceivedUsers(int result, const std::string& response);

  void ParseBrowserInfo();

  DeviceInfoCallback callback_;
  std::string processes_response_;
  std::string sockets_response_;
  std::string users_response_;
  AndroidDeviceManager::DeviceInfo device_info_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AdbDeviceInfoQuery);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_DEVICE_INFO_QUERY_H_
