// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"

namespace chrome {

namespace {

// Helper function to return both the channel enum and modifier string.
// Implements both together to prevent their behavior from diverging, which has
// happened multiple times in the past.
VersionInfo::Channel GetChannelImpl(std::string* modifier_out) {
  VersionInfo::Channel channel = VersionInfo::CHANNEL_UNKNOWN;
  std::string modifier;

  char* env = getenv("CHROME_VERSION_EXTRA");
  if (env)
    modifier = env;

#if defined(GOOGLE_CHROME_BUILD)
  // Only ever return "", "unknown", "dev" or "beta" in a branded build.
  if (modifier == "unstable")  // linux version of "dev"
    modifier = "dev";
  if (modifier == "stable") {
    channel = VersionInfo::CHANNEL_STABLE;
    modifier = "";
  } else if (modifier == "dev") {
    channel = VersionInfo::CHANNEL_DEV;
  } else if (modifier == "beta") {
    channel = VersionInfo::CHANNEL_BETA;
  } else {
    modifier = "unknown";
  }
#endif

  if (modifier_out)
    modifier_out->swap(modifier);

  return channel;
}

}  // namespace

// static
std::string VersionInfo::GetVersionStringModifier() {
  std::string modifier;
  GetChannelImpl(&modifier);
  return modifier;
}

// static
VersionInfo::Channel VersionInfo::GetChannel() {
  return GetChannelImpl(NULL);
}

}  // namespace chrome
