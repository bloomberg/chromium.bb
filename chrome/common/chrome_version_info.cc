// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/string_util.h"
#include "base/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"

namespace chrome {

#if defined(OS_WIN) || defined(OS_MACOSX)
// On Windows and Mac we get the Chrome version info by querying FileVersionInfo
// for the current module.

VersionInfo::VersionInfo() {
  // The current module is already loaded in memory, so this will be cheap.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  version_info_.reset(FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  DCHECK(version_info_.get());
}

VersionInfo::~VersionInfo() {
}

std::string VersionInfo::Name() const {
  std::wstring name = version_info_->product_name();
  return WideToASCII(name);
}

std::string VersionInfo::Version() const {
  return WideToASCII(version_info_->product_version());
}

std::string VersionInfo::LastChange() const {
  return WideToASCII(version_info_->last_change());
}

bool VersionInfo::IsOfficialBuild() const {
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
  return OFFICIAL_BUILD;
}

#endif

}  // namespace chrome
