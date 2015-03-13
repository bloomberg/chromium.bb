// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version_info_values.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chrome {

std::string VersionInfo::ProductNameAndVersionForUserAgent() const {
  return "Chrome/" + Version();
}

VersionInfo::VersionInfo() {
}

VersionInfo::~VersionInfo() {
}

std::string VersionInfo::Name() const {
  return PRODUCT_NAME;
}

std::string VersionInfo::Version() const {
  return PRODUCT_VERSION;
}

std::string VersionInfo::LastChange() const {
  return LAST_CHANGE;
}

bool VersionInfo::IsOfficialBuild() const {
  return IS_OFFICIAL_BUILD;
}

std::string VersionInfo::CreateVersionString() const {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 VersionInfo::CreateVersionString"));

  std::string current_version;
  current_version += Version();
#if !defined(GOOGLE_CHROME_BUILD)
  current_version += " (";
  current_version += l10n_util::GetStringUTF8(IDS_ABOUT_VERSION_UNOFFICIAL);
  current_version += " ";
  current_version += LastChange();
  current_version += " ";
  current_version += OSType();
  current_version += ")";
#endif
  std::string modifier = GetVersionStringModifier();
  if (!modifier.empty())
    current_version += " " + modifier;
  return current_version;
}

std::string VersionInfo::OSType() const {
#if defined(OS_WIN)
  return "Windows";
#elif defined(OS_IOS)
  return "iOS";
#elif defined(OS_MACOSX)
  return "Mac OS X";
#elif defined(OS_CHROMEOS)
  #if defined(GOOGLE_CHROME_BUILD)
    return "Chrome OS";
  #else
    return "Chromium OS";
  #endif
#elif defined(OS_ANDROID)
  return "Android";
#elif defined(OS_LINUX)
  return "Linux";
#elif defined(OS_FREEBSD)
  return "FreeBSD";
#elif defined(OS_OPENBSD)
  return "OpenBSD";
#elif defined(OS_SOLARIS)
  return "Solaris";
#else
  return "Unknown";
#endif
}

}  // namespace chrome
