// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_VERSION_UTILS_H_
#define CHROME_COMMON_METRICS_VERSION_UTILS_H_

#include <string>

#include "chrome/common/chrome_version_info.h"
#include "components/metrics/proto/system_profile.pb.h"

namespace metrics {

// Build a string including the Chrome app version, suffixed by "-64" on 64-bit
// platforms, and "-devel" on developer builds.
std::string GetVersionString();

// Translates chrome::VersionInfo::Channel to the equivalent
// SystemProfileProto::Channel.
SystemProfileProto::Channel AsProtobufChannel(
    chrome::VersionInfo::Channel channel);

}  // namespace metrics

#endif  // CHROME_COMMON_METRICS_VERSION_UTILS_H_
