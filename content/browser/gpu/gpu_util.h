// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_UTIL_H_
#define CONTENT_BROWSER_GPU_GPU_UTIL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/gpu_switching_option.h"

namespace content {
class GpuBlacklist;

// Maps string to GpuSwitchingOption; returns GPU_SWITCHING_UNKNOWN if an
// unknown name is input (case-sensitive).
CONTENT_EXPORT GpuSwitchingOption StringToGpuSwitchingOption(
    const std::string& switching_string);

// Gets a string version of a GpuSwitchingOption.
CONTENT_EXPORT std::string GpuSwitchingOptionToString(
    GpuSwitchingOption option);

// Send UMA histograms about the enabled features.
CONTENT_EXPORT void UpdateStats(
    const GpuBlacklist* blacklist, const std::set<int>& blacklisted_features);

// Merge features in src into dst.
CONTENT_EXPORT void MergeFeatureSets(
    std::set<int>* dst, const std::set<int>& src);

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_UTIL_H_

