// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chrome {

std::string VersionInfo::ProductNameAndVersionForUserAgent() const {
  if (!is_valid())
    return std::string();
  return "Chrome/" + Version();
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// On Windows and Mac, we get the Chrome version info by querying
// FileVersionInfo for the current module.

VersionInfo::VersionInfo() {
  // The current module is already loaded in memory, so this will be cheap.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  version_info_.reset(FileVersionInfo::CreateFileVersionInfoForCurrentModule());
}

VersionInfo::~VersionInfo() {
}

bool VersionInfo::is_valid() const {
  return version_info_.get() != NULL;
}

std::string VersionInfo::Name() const {
  if (!is_valid())
    return std::string();
  return base::UTF16ToUTF8(version_info_->product_name());
}

std::string VersionInfo::Version() const {
  if (!is_valid())
    return std::string();
  return base::UTF16ToUTF8(version_info_->product_version());
}

std::string VersionInfo::LastChange() const {
  if (!is_valid())
    return std::string();
  return base::UTF16ToUTF8(version_info_->last_change());
}

bool VersionInfo::IsOfficialBuild() const {
  if (!is_valid())
    return false;
  return version_info_->is_official_build();
}

#elif defined(OS_POSIX)
// We get chrome version information from chrome_version_info_posix.h,
// a generated header.

#include "chrome/common/chrome_version_info_posix.h"

VersionInfo::VersionInfo() {
}

VersionInfo::~VersionInfo() {
}

bool VersionInfo::is_valid() const {
  return true;
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

#endif

std::string VersionInfo::CreateVersionString() const {
  std::string current_version;
  if (is_valid()) {
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
  }
  return current_version;
}

std::string VersionInfo::OSType() const {
#if defined(OS_WIN)
  return "Windows";
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
