// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_H_
#define CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_H_

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "sync/protocol/sync.pb.h"

namespace chrome {
class VersionInfo;
}

namespace browser_sync {

// A class that holds information regarding the properties of a device.
//
// These objects do not contain enough information to uniquely identify devices.
// Two different devices may end up generating identical DeviceInfos.
class DeviceInfo {
 public:
  DeviceInfo(const std::string& client_name,
             const std::string& chrome_version,
             const std::string& sync_user_agent,
             const sync_pb::SyncEnums::DeviceType device_type);
  ~DeviceInfo();

  const std::string& client_name() const;
  const std::string& chrome_version() const;
  const std::string& sync_user_agent() const;
  sync_pb::SyncEnums::DeviceType device_type() const;

  // Compares this object's fields with another's.
  bool Equals(const DeviceInfo& other) const;

  static sync_pb::SyncEnums::DeviceType GetLocalDeviceType();
  static void CreateLocalDeviceInfo(
      base::Callback<void(const DeviceInfo& local_info)> callback);

  // Helper to construct a user agent string (ASCII) suitable for use by
  // the syncapi for any HTTP communication. This string is used by the sync
  // backend for classifying client types when calculating statistics.
  static std::string MakeUserAgentForSyncApi(
      const chrome::VersionInfo& version_info);

 private:
  static void CreateLocalDeviceInfoContinuation(
      base::Callback<void(const DeviceInfo& local_info)> callback,
      const std::string& session_name);

  const std::string client_name_;
  const std::string chrome_version_;
  const std::string sync_user_agent_;
  const sync_pb::SyncEnums::DeviceType device_type_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfo);
};

}

#endif  // CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_H_
