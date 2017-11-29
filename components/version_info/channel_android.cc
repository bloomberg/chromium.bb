// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_info/channel_android.h"

#include <cstring>

namespace version_info {

Channel ChannelFromPackageName(const char* package_name) {
  if (!strcmp(package_name, "com.android.chrome"))
    return Channel::STABLE;
  if (!strcmp(package_name, "com.chrome.beta"))
    return Channel::BETA;
  if (!strcmp(package_name, "com.chrome.dev"))
    return Channel::DEV;
  if (!strcmp(package_name, "com.chrome.canary"))
    return Channel::CANARY;

  return Channel::UNKNOWN;
}

}  // namespace version_info
