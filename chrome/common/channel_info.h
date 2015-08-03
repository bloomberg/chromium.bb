// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHANNEL_INFO_H_
#define CHROME_COMMON_CHANNEL_INFO_H_

#include <string>

namespace version_info {
enum class Channel;
}

namespace chrome {

// Returns a version string to be displayed in "About Chromium" dialog.
std::string GetVersionString();

// Returns a human-readable modifier for the version string. For a branded
// build, this modifier is the channel ("canary", "dev", or "beta", but ""
// for stable). On Windows, this may be modified with additional information
// after a hyphen. For multi-user installations, it will return "canary-m",
// "dev-m", "beta-m", and for a stable channel multi-user installation, "m".
// In branded builds, when the channel cannot be determined, "unknown" will
// be returned. In unbranded builds, the modifier is usually an empty string
// (""), although on Linux, it may vary in certain distributions.
// GetChannelString() is intended to be used for display purposes.
// To simply test the channel, use GetChannel().
std::string GetChannelString();

// Returns the channel for the installation. In branded builds, this will be
// version_info::Channel::{STABLE,BETA,DEV,CANARY}. In unbranded builds, or
// in branded builds when the channel cannot be determined, this will be
// version_info::Channel::UNKNOWN.
version_info::Channel GetChannel();

#if defined(OS_CHROMEOS)
// Sets channel before use.
void SetChannel(const std::string& channel);
#endif

}  // namespace chrome

#endif  // CHROME_COMMON_CHANNEL_INFO_H_
