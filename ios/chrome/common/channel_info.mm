// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/channel_info.h"

#import <Foundation/Foundation.h>

#import "base/mac/bundle_locations.h"
#include "base/profiler/scoped_tracker.h"
#import "base/strings/sys_string_conversions.h"
#include "components/version_info/version_info.h"

namespace {

// Returns the channel and if |channel_string| is not null, it is set
// to the string representation of the channel (only ever return one
// of "", "unknown", "beta", "dev" or "canary" in branded build).
version_info::Channel GetChannelImpl(std::string* channel_string) {
#if defined(GOOGLE_CHROME_BUILD)
  NSBundle* bundle = base::mac::OuterBundle();
  NSString* channel = [bundle objectForInfoDictionaryKey:@"KSChannelID"];

  // Only Keystone-enabled build can have a channel.
  if ([bundle objectForInfoDictionaryKey:@"KSProductID"]) {
    // KSChannelID is unset for the stable channel.
    if (!channel) {
      if (channel_string)
        channel_string->clear();
      return version_info::Channel::STABLE;
    }

    if ([channel isEqualToString:@"beta"]) {
      if (channel_string)
        channel_string->assign(base::SysNSStringToUTF8(channel));
      return version_info::Channel::BETA;
    }

    if ([channel isEqualToString:@"dev"]) {
      if (channel_string)
        channel_string->assign(base::SysNSStringToUTF8(channel));
      return version_info::Channel::DEV;
    }

    if ([channel isEqualToString:@"canary"]) {
      if (channel_string)
        channel_string->assign(base::SysNSStringToUTF8(channel));
      return version_info::Channel::CANARY;
    }
  }

  if (channel_string)
    channel_string->assign("unknown");
  return version_info::Channel::UNKNOWN;
#else
  // Always return empty string for non-branded builds.
  return version_info::Channel::UNKNOWN;
#endif
}

}  // namespace

std::string GetVersionString() {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 VersionInfo::CreateVersionString"));

  return version_info::GetVersionStringWithModifier(GetChannelString());
}

std::string GetChannelString() {
  std::string channel;
  GetChannelImpl(&channel);
  return channel;
}

version_info::Channel GetChannel() {
  return GetChannelImpl(nullptr);
}
