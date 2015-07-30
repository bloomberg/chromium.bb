// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_VERSION_INFO_H_
#define CHROME_COMMON_CHROME_VERSION_INFO_H_

#include <string>

#include "base/macros.h"
#include "components/version_info/version_info.h"

namespace chrome {

// An instance of chrome::VersionInfo has information about the
// current running build of Chrome.
// TODO(sdefresne): this class should be removed in favor of calling directly
// version_info free functions and this file should just provide method to
// compute version string modifier (system dependent) and channel (embedder
// dependent). Tracked by http://crbug.com/514562.
class VersionInfo {
 public:
  VersionInfo();
  ~VersionInfo();

  // E.g. "Chrome/a.b.c.d"
  static std::string ProductNameAndVersionForUserAgent();

  // E.g. "Chromium" or "Google Chrome".
  static std::string Name();

  // Version number, e.g. "6.0.490.1".
  static std::string Version();

  // The SVN revision of this release.  E.g. "55800".
  static std::string LastChange();

  // Whether this is an "official" release of the current Version():
  // whether knowing Version() is enough to completely determine what
  // LastChange() is.
  static bool IsOfficialBuild();

  // OS type. E.g. "Windows", "Linux", "FreeBSD", ...
  static std::string OSType();

  // Returns a human-readable modifier for the version string. For a branded
  // build, this modifier is the channel ("canary", "dev", or "beta", but ""
  // for stable). On Windows, this may be modified with additional information
  // after a hyphen. For multi-user installations, it will return "canary-m",
  // "dev-m", "beta-m", and for a stable channel multi-user installation, "m".
  // In branded builds, when the channel cannot be determined, "unknown" will
  // be returned. In unbranded builds, the modifier is usually an empty string
  // (""), although on Linux, it may vary in certain distributions.
  // GetVersionStringModifier() is intended to be used for display purposes.
  // To simply test the channel, use GetChannel().
  static std::string GetVersionStringModifier();

  // Returns the channel for the installation. In branded builds, this will be
  // CHANNEL_STABLE, CHANNEL_BETA, CHANNEL_DEV, or CHANNEL_CANARY. In unbranded
  // builds, or in branded builds when the channel cannot be determined, this
  // will be CHANNEL_UNKNOWN.
  static version_info::Channel GetChannel();

  // Returns a string equivalent of the channel, independent of whether it is a
  // branded build or not and without any additional modifiers.
  static std::string GetChannelString();

#if defined(OS_CHROMEOS)
  // Sets channel before use.
  static void SetChannel(const std::string& channel);
#endif

  // Returns a version string to be displayed in "About Chromium" dialog.
  static std::string CreateVersionString();

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionInfo);
};

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_VERSION_INFO_H_
