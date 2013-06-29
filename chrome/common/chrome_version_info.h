// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_VERSION_INFO_H_
#define CHROME_COMMON_CHROME_VERSION_INFO_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class FileVersionInfo;

namespace chrome {

// An instance of chrome::VersionInfo has information about the
// current running build of Chrome.
class VersionInfo {
 public:
  // The possible channels for an installation, from most fun to most stable.
  enum Channel {
    CHANNEL_UNKNOWN = 0,  // Probably blue
    CHANNEL_CANARY,       // Yellow
    CHANNEL_DEV,          // Technicolor
    CHANNEL_BETA,         // Rainbow
    CHANNEL_STABLE        // Full-spectrum
  };

  VersionInfo();
  ~VersionInfo();

  // In the rare case where we fail to get the version info,
  // is_valid() will return false.  The other functions will return
  // the empty string in this case, so it's not harmful if you don't
  // check is_valid().
  bool is_valid() const;

  // E.g. "Chrome/a.b.c.d"
  std::string ProductNameAndVersionForUserAgent() const;

  // E.g. "Chromium" or "Google Chrome".
  std::string Name() const;

  // Version number, e.g. "6.0.490.1".
  std::string Version() const;

  // The SVN revision of this release.  E.g. "55800".
  std::string LastChange() const;

  // Whether this is an "official" release of the current Version():
  // whether knowing Version() is enough to completely determine what
  // LastChange() is.
  bool IsOfficialBuild() const;

  // OS type. E.g. "Windows", "Linux", "FreeBSD", ...
  std::string OSType() const;

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
  static Channel GetChannel();

#if defined(OS_CHROMEOS)
  // Sets channel before use.
  static void SetChannel(const std::string& channel);
#endif

  // Returns a version string to be displayed in "About Chromium" dialog.
  std::string CreateVersionString() const;

 private:
#if defined(OS_WIN) || defined(OS_MACOSX)
  scoped_ptr<FileVersionInfo> version_info_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VersionInfo);
};

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_VERSION_INFO_H_
