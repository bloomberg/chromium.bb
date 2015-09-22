// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/version_utils.h"

#include "base/logging.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"

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
    version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
      return SystemProfileProto::CHANNEL_UNKNOWN;
    case version_info::Channel::CANARY:
      return SystemProfileProto::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return SystemProfileProto::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return SystemProfileProto::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return SystemProfileProto::CHANNEL_STABLE;
  }
  NOTREACHED();
  return SystemProfileProto::CHANNEL_UNKNOWN;
}

}  // namespace metrics
