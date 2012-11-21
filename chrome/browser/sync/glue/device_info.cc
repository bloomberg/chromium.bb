// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/device_info.h"

#include "base/command_line.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "sync/util/get_session_name.h"

namespace browser_sync {

namespace {

#if defined(OS_ANDROID)
bool IsTabletUI() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kTabletUI);
}
#endif

}  // namespace

DeviceInfo::DeviceInfo(const std::string& client_name,
                       const std::string& chrome_version,
                       const std::string& sync_user_agent,
                       const sync_pb::SyncEnums::DeviceType device_type)
    : client_name_(client_name),
      chrome_version_(chrome_version),
      sync_user_agent_(sync_user_agent),
      device_type_(device_type) {
}

DeviceInfo::~DeviceInfo() { }

const std::string& DeviceInfo::client_name() const {
  return client_name_;
}

const std::string& DeviceInfo::chrome_version() const {
  return chrome_version_;
}

const std::string& DeviceInfo::sync_user_agent() const {
  return sync_user_agent_;
}

sync_pb::SyncEnums::DeviceType DeviceInfo::device_type() const {
  return device_type_;
}

bool DeviceInfo::Equals(const DeviceInfo& other) const {
  return this->client_name() == other.client_name()
      && this->chrome_version() == other.chrome_version()
      && this->sync_user_agent() == other.sync_user_agent()
      && this->device_type() == other.device_type();
}

// static.
sync_pb::SyncEnums::DeviceType DeviceInfo::GetLocalDeviceType() {
#if defined(OS_CHROMEOS)
  return sync_pb::SyncEnums_DeviceType_TYPE_CROS;
#elif defined(OS_LINUX)
  return sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
#elif defined(OS_MACOSX)
  return sync_pb::SyncEnums_DeviceType_TYPE_MAC;
#elif defined(OS_WIN)
  return sync_pb::SyncEnums_DeviceType_TYPE_WIN;
#elif defined(OS_ANDROID)
  return IsTabletUI() ?
      sync_pb::SyncEnums_DeviceType_TYPE_TABLET :
      sync_pb::SyncEnums_DeviceType_TYPE_PHONE;
#else
  return sync_pb::SyncEnums_DeviceType_TYPE_OTHER;
#endif
}

// static
std::string DeviceInfo::MakeUserAgentForSyncApi(
    const chrome::VersionInfo& version_info) {
  std::string user_agent;
  user_agent = "Chrome ";
#if defined(OS_WIN)
  user_agent += "WIN ";
#elif defined(OS_CHROMEOS)
  user_agent += "CROS ";
#elif defined(OS_ANDROID)
  user_agent += "ANDROID ";
#elif defined(OS_LINUX)
  user_agent += "LINUX ";
#elif defined(OS_FREEBSD)
  user_agent += "FREEBSD ";
#elif defined(OS_OPENBSD)
  user_agent += "OPENBSD ";
#elif defined(OS_MACOSX)
  user_agent += "MAC ";
#endif
  if (!version_info.is_valid()) {
    DLOG(ERROR) << "Unable to create chrome::VersionInfo object";
    return user_agent;
  }

  user_agent += version_info.Version();
  user_agent += " (" + version_info.LastChange() + ")";
  if (!version_info.IsOfficialBuild())
    user_agent += "-devel";
  return user_agent;
}

// static.
void DeviceInfo::CreateLocalDeviceInfo(
    base::Callback<void(const DeviceInfo& local_info)> callback) {
  syncer::GetSessionName(
      content::BrowserThread::GetBlockingPool(),
      base::Bind(&DeviceInfo::CreateLocalDeviceInfoContinuation, callback));
}

// static.
void DeviceInfo::CreateLocalDeviceInfoContinuation(
    base::Callback<void(const DeviceInfo& local_info)> callback,
    const std::string& session_name) {
  chrome::VersionInfo version_info;

  DeviceInfo local_info(
      session_name,
      version_info.CreateVersionString(),
      MakeUserAgentForSyncApi(version_info),
      GetLocalDeviceType());

  callback.Run(local_info);
}

}  // namespace browser_sync
