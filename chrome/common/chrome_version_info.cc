// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

#include "base/profiler/scoped_tracker.h"
#include "build/build_config.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chrome {

VersionInfo::VersionInfo() {
}

VersionInfo::~VersionInfo() {
}

// static
std::string VersionInfo::ProductNameAndVersionForUserAgent() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

// static
std::string VersionInfo::Name() {
  return version_info::GetProductName();
}

// static
std::string VersionInfo::Version() {
  return version_info::GetVersionNumber();
}

// static
std::string VersionInfo::LastChange() {
  return version_info::GetLastChange();
}

// static
bool VersionInfo::IsOfficialBuild() {
  return version_info::IsOfficialBuild();
}

// static
std::string VersionInfo::OSType() {
  return version_info::GetOSType();
}

// static
std::string VersionInfo::GetChannelString() {
  return version_info::GetChannelString(GetChannel());
}

// static
std::string VersionInfo::CreateVersionString() {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 VersionInfo::CreateVersionString"));

  return version_info::GetVersionStringWithModifier(GetVersionStringModifier());
}

}  // namespace chrome
