// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_DEVICE_INFO_QUERY_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_DEVICE_INFO_QUERY_H_

#include "base/threading/non_thread_safe.h"
#include "chrome/browser/devtools/device/android_device_manager.h"

class AdbDeviceInfoQuery : public base::NonThreadSafe {
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
  AdbDeviceInfoQuery(const RunCommandCallback& command_callback,
                     const DeviceInfoCallback& callback);

  virtual ~AdbDeviceInfoQuery();

  void ReceivedModel(int result, const std::string& response);

  void ReceivedDumpsys(int result, const std::string& response);

  void ParseDumpsysResponse(const std::string& response);

  void ParseScreenSize(const std::string& str);

  void ReceivedProcesses(int result,
                         const std::string& processes_response);

  void ReceivedSockets(const std::string& processes_response,
                       int result,
                       const std::string& sockets_response);

  void ParseBrowserInfo(const std::string& processes_response,
                        const std::string& sockets_response);

  void Respond();

  RunCommandCallback command_callback_;
  DeviceInfoCallback callback_;
  AndroidDeviceManager::DeviceInfo device_info_;

  DISALLOW_COPY_AND_ASSIGN(AdbDeviceInfoQuery);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_DEVICE_INFO_QUERY_H_
