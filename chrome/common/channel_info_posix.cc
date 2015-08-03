// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"

namespace chrome {

namespace {

// Helper function to return both the channel enum and modifier string.
// Implements both together to prevent their behavior from diverging, which has
// happened multiple times in the past.
version_info::Channel GetChannelImpl(std::string* modifier_out) {
  version_info::Channel channel = version_info::Channel::UNKNOWN;
  std::string modifier;

  char* env = getenv("CHROME_VERSION_EXTRA");
  if (env)
    modifier = env;

#if defined(GOOGLE_CHROME_BUILD)
  // Only ever return "", "unknown", "dev" or "beta" in a branded build.
  if (modifier == "unstable")  // linux version of "dev"
    modifier = "dev";
  if (modifier == "stable") {
    channel = version_info::Channel::STABLE;
    modifier = "";
  } else if (modifier == "dev") {
    channel = version_info::Channel::DEV;
  } else if (modifier == "beta") {
    channel = version_info::Channel::BETA;
  } else {
    modifier = "unknown";
  }
#endif

  if (modifier_out)
    modifier_out->swap(modifier);

  return channel;
}

}  // namespace

std::string GetChannelString() {
  std::string modifier;
  GetChannelImpl(&modifier);
  return modifier;
}

version_info::Channel GetChannel() {
  return GetChannelImpl(nullptr);
}

}  // namespace chrome
