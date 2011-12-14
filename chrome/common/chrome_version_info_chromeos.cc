// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

namespace chrome {

static VersionInfo::Channel chromeos_channel = VersionInfo::CHANNEL_UNKNOWN;

// static
std::string VersionInfo::GetVersionStringModifier() {
#if defined(GOOGLE_CHROME_BUILD)
  switch (chromeos_channel) {
    case CHANNEL_STABLE:
      return "";
    case CHANNEL_BETA:
      return "beta";
    case CHANNEL_DEV:
      return "dev";
    case CHANNEL_CANARY:
      return "canary";
    default:
      return "unknown";
  }
#endif
  return std::string();
}

// static
VersionInfo::Channel VersionInfo::GetChannel() {
  return chromeos_channel;
}

// static
void VersionInfo::SetChannel(const std::string& channel) {
#if defined(GOOGLE_CHROME_BUILD)
  if (channel == "stable-channel") {
    chromeos_channel = CHANNEL_STABLE;
  } else if (channel == "beta-channel") {
    chromeos_channel = CHANNEL_BETA;
  } else if (channel == "dev-channel") {
    chromeos_channel = CHANNEL_DEV;
  } else if (channel == "canary-channel") {
    chromeos_channel = CHANNEL_CANARY;
  }
#endif
}

}  // namespace chrome
