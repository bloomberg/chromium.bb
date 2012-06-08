// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_UTIL_H_
#define CHROME_BROWSER_GPU_UTIL_H_
#pragma once

#include <string>

#include "content/public/common/gpu_feature_type.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace gpu_util {

// Maps string to GpuFeatureType; returns GPU_FEATURE_TYPE_UNKNOWN if none of
// the following is input (case-sensitive):
//   "accelerated_2d_canvas"
//   "accelerated_compositing"
//   "webgl"
//   "multisampling"
content::GpuFeatureType StringToGpuFeatureType(
    const std::string& feature_string);

// Gets a string version of a feature type for use in about:gpu. Will yield
// strings from StringToGpuFeatureType, e.g.
// GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS becomes "accelerated_2d_canvas"
std::string GpuFeatureTypeToString(content::GpuFeatureType feature);

// Returns status of various GPU features. This is two parted:
// {
//    featureStatus: []
//    problems: []
// }
//
// Each entry in feature_status has:
// {
//    name:  "name of feature"
//    status: "enabled" | "unavailable_software" | "unavailable_off" |
//            "software" | "disabled_off" | "disabled_softare";
// }
//
// The features reported are not 1:1 with GpuFeatureType. Rather, they are:
//    '2d_canvas'
//    '3d_css'
//    'composting',
//    'webgl',
//    'multisampling',
//    'flash_3d',
//    'flash_stage3d'
//
// Each problems has:
// {
//    "description": "Your GPU is too old",
//    "crBugs": [1234],
//    "webkitBugs": []
// }
//
// Caller is responsible for deleting the returned value.
base::Value* GetFeatureStatus();

// Returns the GpuInfo as a DictionaryValue.
base::DictionaryValue* GpuInfoAsDictionaryValue();

// Send UMA histograms about the enabled features.
void UpdateStats();

// Returs whether this client has been selected for the force-compositing-mode
// trial.
bool InForceCompositingModeTrial();

// Sets up the force-compositing-mode field trial.
void InitializeForceCompositingModeFieldTrial();

}  // namespace gpu_util

#endif  // CHROME_BROWSER_GPU_UTIL_H_
