// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/system/sys_info.h"
#include "components/version_info/version_info.h"

namespace chrome {

namespace {

#if defined(GOOGLE_CHROME_BUILD)
version_info::Channel ChannelStringToChannel(const std::string& channel) {
  if (channel == "stable-channel")
    return version_info::Channel::STABLE;
  if (channel == "beta-channel")
    return version_info::Channel::BETA;
  if (channel == "dev-channel")
    return version_info::Channel::DEV;
  if (channel == "canary-channel")
    return version_info::Channel::CANARY;
  return version_info::Channel::UNKNOWN;
}
#endif

}  // namespace

std::string GetChannelName() {
#if defined(GOOGLE_CHROME_BUILD)
  switch (GetChannel()) {
    case version_info::Channel::STABLE:
      return std::string();
    case version_info::Channel::BETA:
      return "beta";
    case version_info::Channel::DEV:
      return "dev";
    case version_info::Channel::CANARY:
      return "canary";
    default:
      return "unknown";
  }
#endif
  return std::string();
}

#if defined(GOOGLE_CHROME_BUILD)
std::string GetChannelSuffixForDataDir() {
  // ChromeOS doesn't support side-by-side installations.
  return std::string();
}
#endif  // defined(GOOGLE_CHROME_BUILD)

namespace channel_info_internal {

version_info::Channel InitChannel() {
#if defined(GOOGLE_CHROME_BUILD)
  static const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
  std::string channel;
  if (base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &channel))
    return ChannelStringToChannel(channel);
#endif
  return version_info::Channel::UNKNOWN;
}

}  // namespace channel_info_internal

}  // namespace chrome
