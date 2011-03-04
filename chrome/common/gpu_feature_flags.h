// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_FEATURE_FLAGS_H__
#define CHROME_COMMON_GPU_FEATURE_FLAGS_H__
#pragma once

// Provides flags indicating which gpu features are blacklisted for the system
// on which chrome is currently running.

#include <string>

#include "base/basictypes.h"

class GpuFeatureFlags {
 public:
  enum GpuFeatureType {
    kGpuFeatureAccelerated2dCanvas = 1 << 0,
    kGpuFeatureAcceleratedCompositing = 1 << 1,
    kGpuFeatureWebgl = 1 << 2,
    kGpuFeatureMultisampling = 1 << 3,
    kGpuFeatureAll = kGpuFeatureAccelerated2dCanvas |
                     kGpuFeatureAcceleratedCompositing |
                     kGpuFeatureWebgl |
                     kGpuFeatureMultisampling,
    kGpuFeatureUnknown = 0
  };

  // All flags initialized to false, i.e., no feature is blacklisted.
  GpuFeatureFlags();

  // flags are OR combination of GpuFeatureType.
  void set_flags(uint32 flags);

  uint32 flags() const;

  // Resets each flag by OR with the corresponding flag in "other".
  void Combine(const GpuFeatureFlags& other);

  // Maps string to GpuFeatureType; returns kGpuFeatureUnknown if none of the
  // following is input (case-sensitive):
  //   "accelerated_2d_canvas"
  //   "accelerated_compositing"
  //   "webgl"
  //   "multisampling"
  static GpuFeatureType StringToGpuFeatureType(
      const std::string& feature_string);

 private:
  static const char kGpuFeatureNameAccelerated2dCanvas[];
  static const char kGpuFeatureNameAcceleratedCompositing[];
  static const char kGpuFeatureNameWebgl[];
  static const char kGpuFeatureNameMultisampling[];
  static const char kGpuFeatureNameAll[];

  // If a bit is set to 1, corresponding feature is blacklisted.
  uint32 flags_;
};

#endif  // CHROME_COMMON_GPU_FEATURE_FLAGS_H__

