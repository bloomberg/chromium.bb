// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

#include "base/android/build_info.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

namespace chrome {

// static
std::string VersionInfo::GetVersionStringModifier() {
  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN: return "unknown";
    case version_info::Channel::CANARY: return "canary";
    case version_info::Channel::DEV: return "dev";
    case version_info::Channel::BETA: return "beta";
    case version_info::Channel::STABLE: return std::string();
  }
  NOTREACHED() << "Unknown channel " << static_cast<int>(GetChannel());
  return std::string();
}

// static
version_info::Channel VersionInfo::GetChannel() {
  const base::android::BuildInfo* bi = base::android::BuildInfo::GetInstance();
  DCHECK(bi && bi->package_name());

  if (!strcmp(bi->package_name(), "com.android.chrome")
       || !strcmp(bi->package_name(), "com.chrome.work"))
    return version_info::Channel::STABLE;
  if (!strcmp(bi->package_name(), "com.chrome.beta"))
    return version_info::Channel::BETA;
  if (!strcmp(bi->package_name(), "com.chrome.dev"))
    return version_info::Channel::DEV;
  if (!strcmp(bi->package_name(), "com.chrome.canary"))
    return version_info::Channel::CANARY;

  return version_info::Channel::UNKNOWN;
}

}  // namespace chrome
