// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/version_utils.h"

#include "base/logging.h"
#include "chrome/common/chrome_version_info_values.h"

namespace metrics {

std::string GetVersionString() {
  std::string version = PRODUCT_VERSION;
#if defined(ARCH_CPU_64_BITS)
  version += "-64";
#endif  // defined(ARCH_CPU_64_BITS)
  if (!IS_OFFICIAL_BUILD)
    version.append("-devel");
  return version;
}

SystemProfileProto::Channel AsProtobufChannel(
    chrome::VersionInfo::Channel channel) {
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return SystemProfileProto::CHANNEL_UNKNOWN;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return SystemProfileProto::CHANNEL_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return SystemProfileProto::CHANNEL_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return SystemProfileProto::CHANNEL_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return SystemProfileProto::CHANNEL_STABLE;
  }
  NOTREACHED();
  return SystemProfileProto::CHANNEL_UNKNOWN;
}

}  // namespace metrics
