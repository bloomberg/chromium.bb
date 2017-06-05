// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_INFO_CHANNEL_ANDROID_H_
#define COMPONENTS_VERSION_INFO_CHANNEL_ANDROID_H_

#include "components/version_info/channel.h"

namespace version_info {

// Determine channel based on Chrome's package name.
Channel ChannelFromPackageName(const char* package_name);

}  // namespace version_info

#endif  // COMPONENTS_VERSION_INFO_CHANNEL_ANDROID_H_
