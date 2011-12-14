// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

namespace chrome {

// static
std::string VersionInfo::GetVersionStringModifier() {
  char* env = getenv("CHROME_VERSION_EXTRA");
  if (!env)
    return std::string();
  std::string modifier(env);

#if defined(GOOGLE_CHROME_BUILD)
  // Only ever return "", "unknown", "dev" or "beta" in a branded build.
  if (modifier == "unstable")  // linux version of "dev"
    modifier = "dev";
  if (modifier == "stable") {
    modifier = "";
  } else if ((modifier == "dev") || (modifier == "beta")) {
    // do nothing.
  } else {
    modifier = "unknown";
  }
#endif

  return modifier;
}

// static
VersionInfo::Channel VersionInfo::GetChannel() {
#if defined(GOOGLE_CHROME_BUILD)
  std::string channel = GetVersionStringModifier();
  if (channel.empty()) {
    return CHANNEL_STABLE;
  } else if (channel == "beta") {
    return CHANNEL_BETA;
  } else if (channel == "dev") {
    return CHANNEL_DEV;
  } else if (channel == "canary") {
    return CHANNEL_CANARY;
  }
#endif

  return CHANNEL_UNKNOWN;
}

}  // namespace chrome
