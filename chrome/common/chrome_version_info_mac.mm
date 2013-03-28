// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

#import <Foundation/Foundation.h>

#include "base/basictypes.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"

namespace chrome {

// static
std::string VersionInfo::GetVersionStringModifier() {
#if defined(GOOGLE_CHROME_BUILD)
  // Use the main Chrome application bundle and not the framework bundle.
  // Keystone keys don't live in the framework.
  NSBundle* bundle = base::mac::OuterBundle();
  NSString* channel = [bundle objectForInfoDictionaryKey:@"KSChannelID"];

  // Only ever return "", "unknown", "beta", "dev", or "canary" in a branded
  // build.
  if (![bundle objectForInfoDictionaryKey:@"KSProductID"]) {
    // This build is not Keystone-enabled, it can't have a channel.
    channel = @"unknown";
  } else if (!channel) {
    // For the stable channel, KSChannelID is not set.
    channel = @"";
  } else if ([channel isEqual:@"beta"] ||
             [channel isEqual:@"dev"] ||
             [channel isEqual:@"canary"]) {
    // do nothing.
  } else {
    channel = @"unknown";
  }

  return base::SysNSStringToUTF8(channel);
#else
  return std::string();
#endif
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
