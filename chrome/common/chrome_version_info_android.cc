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
    case CHANNEL_UNKNOWN: return "unknown";
    case CHANNEL_CANARY: return "canary";
    case CHANNEL_DEV: return "dev";
    case CHANNEL_BETA: return "beta";
    case CHANNEL_STABLE: return std::string();
  }
  NOTREACHED() << "Unknown channel " << GetChannel();
  return std::string();
}

// static
VersionInfo::Channel VersionInfo::GetChannel() {
  const base::android::BuildInfo* bi = base::android::BuildInfo::GetInstance();
  DCHECK(bi && bi->package_name());

  if (!strcmp(bi->package_name(), "com.android.chrome"))
    return CHANNEL_STABLE;
  if (!strcmp(bi->package_name(), "com.chrome.beta"))
    return CHANNEL_BETA;
  if (!strcmp(bi->package_name(), "com.google.android.apps.chrome_dev"))
    return CHANNEL_DEV;
  if (!strcmp(bi->package_name(), "com.chrome.canary"))
    return CHANNEL_CANARY;

  return CHANNEL_UNKNOWN;
}

}  // namespace chrome
