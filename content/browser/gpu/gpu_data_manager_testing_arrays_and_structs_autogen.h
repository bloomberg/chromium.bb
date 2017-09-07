// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_TESTING_ARRAYS_AND_STRUCTS_AUTOGEN_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_TESTING_ARRAYS_AND_STRUCTS_AUTOGEN_H_

#include "gpu/config/gpu_feature_type.h"

namespace gpu {
const int kFeatureListForEntry1[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

const int kFeatureListForEntry2[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
};

const GpuControlList::GLStrings kGLStringsForEntry2 = {
    nullptr, ".*GeForce.*", nullptr, nullptr,
};

const int kFeatureListForEntry3[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
};

const int kFeatureListForEntry4[2] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL2, GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

const GpuControlList::GLStrings kGLStringsForEntry4 = {
    nullptr, ".*GeForce.*", nullptr, nullptr,
};

const int kFeatureListForEntry5[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

const GpuControlList::GLStrings kGLStringsForEntry5Exception0 = {
    nullptr, ".*GeForce.*", nullptr, nullptr,
};

const int kFeatureListForEntry6[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

const uint32_t kDeviceIDsForEntry6Exception0[1] = {
    0x0042,
};

const GpuControlList::DriverInfo kDriverInfoForEntry6Exception0 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "8.0.2",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry7[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

const uint32_t kDeviceIDsForEntry7Exception0[1] = {
    0x0042,
};

const GpuControlList::DriverInfo kDriverInfoForEntry7Exception0 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "8.0.2",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry8[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

const uint32_t kDeviceIDsForEntry8[1] = {
    0x0042,
};

const GpuControlList::DriverInfo kDriverInfoForEntry8 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "8.0.0",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry9[11] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const int kFeatureListForEntry10[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

}  // namespace gpu

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_TESTING_ARRAYS_AND_STRUCTS_AUTOGEN_H_
