// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_util.h"

#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "ui/base/device_form_factor.h"

namespace syncer {

// Declared here but defined in platform-specific files.
std::string GetSessionNameInternal();

sync_pb::SyncEnums::DeviceType GetLocalDeviceType() {
#if defined(OS_CHROMEOS)
  return sync_pb::SyncEnums_DeviceType_TYPE_CROS;
#elif defined(OS_LINUX)
  return sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
#elif defined(OS_ANDROID) || defined(OS_IOS)
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? sync_pb::SyncEnums_DeviceType_TYPE_TABLET
             : sync_pb::SyncEnums_DeviceType_TYPE_PHONE;
#elif defined(OS_MACOSX)
  return sync_pb::SyncEnums_DeviceType_TYPE_MAC;
#elif defined(OS_WIN)
  return sync_pb::SyncEnums_DeviceType_TYPE_WIN;
#else
  return sync_pb::SyncEnums_DeviceType_TYPE_OTHER;
#endif
}

std::string GetSessionNameBlocking() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::string session_name = GetSessionNameInternal();

  if (session_name == "Unknown" || session_name.empty()) {
    session_name = base::SysInfo::OperatingSystemName();
  }

  DCHECK(base::IsStringUTF8(session_name));
  return session_name;
}

}  // namespace syncer
