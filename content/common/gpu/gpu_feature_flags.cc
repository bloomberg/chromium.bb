// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_feature_flags.h"

#include "base/logging.h"
#include "base/string_util.h"
#include <vector>

const char GpuFeatureFlags::kGpuFeatureNameAccelerated2dCanvas[] =
    "accelerated_2d_canvas";
const char GpuFeatureFlags::kGpuFeatureNameAcceleratedCompositing[] =
    "accelerated_compositing";
const char GpuFeatureFlags::kGpuFeatureNameWebgl[] = "webgl";
const char GpuFeatureFlags::kGpuFeatureNameMultisampling[] = "multisampling";
const char GpuFeatureFlags::kGpuFeatureNameAll[] = "all";
const char GpuFeatureFlags::kGpuFeatureNameUnknown[] = "unknown";

GpuFeatureFlags::GpuFeatureFlags()
    : flags_(0) {
}

void GpuFeatureFlags::set_flags(uint32 flags) {
  DCHECK_EQ(flags & (~kGpuFeatureAll), 0u);
  flags_ = flags;
}

uint32 GpuFeatureFlags::flags() const {
  return flags_;
}

void GpuFeatureFlags::Combine(const GpuFeatureFlags& other) {
  flags_ |= other.flags_;
}

// static
GpuFeatureFlags::GpuFeatureType GpuFeatureFlags::StringToGpuFeatureType(
    const std::string& feature_string) {
  if (feature_string == kGpuFeatureNameAccelerated2dCanvas)
    return kGpuFeatureAccelerated2dCanvas;
  else if (feature_string == kGpuFeatureNameAcceleratedCompositing)
    return kGpuFeatureAcceleratedCompositing;
  else if (feature_string == kGpuFeatureNameWebgl)
    return kGpuFeatureWebgl;
  else if (feature_string == kGpuFeatureNameMultisampling)
    return kGpuFeatureMultisampling;
  else if (feature_string == kGpuFeatureNameAll)
    return kGpuFeatureAll;
  return kGpuFeatureUnknown;
}

// static
std::string GpuFeatureFlags::GpuFeatureTypeToString(
    GpuFeatureFlags::GpuFeatureType type) {
  std::vector<std::string> matches;
  if (type == kGpuFeatureAll) {
    matches.push_back(kGpuFeatureNameAll);
  } else {
    if (kGpuFeatureAccelerated2dCanvas & type)
      matches.push_back(kGpuFeatureNameAccelerated2dCanvas);
    if (kGpuFeatureAcceleratedCompositing & type)
      matches.push_back(kGpuFeatureNameAcceleratedCompositing);
    if (kGpuFeatureWebgl & type)
      matches.push_back(kGpuFeatureNameWebgl);
    if (kGpuFeatureMultisampling & type)
      matches.push_back(kGpuFeatureNameMultisampling);
    if (!matches.size())
      matches.push_back(kGpuFeatureNameUnknown);
  }
  return JoinString(matches, ',');
}
