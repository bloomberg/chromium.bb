// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_CONFIG_SOFTWARE_RENDERING_LIST_ARRAYS_AND_STRUCTS_AUTOGEN_H_
#define GPU_CONFIG_SOFTWARE_RENDERING_LIST_ARRAYS_AND_STRUCTS_AUTOGEN_H_

#include "gpu/config/gpu_feature_type.h"

namespace gpu {
const int kFeatureListForEntry1[4] = {
    GPU_FEATURE_TYPE_FLASH3D, GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL, GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kDeviceIDsForEntry1[1] = {
    0x7249,
};

const int kFeatureListForEntry3[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry3[2] = {
    59302, 315217,
};

const GpuControlList::GLStrings kGLStringsForEntry3 = {
    nullptr, "(?i).*software.*", nullptr, nullptr,
};

const int kFeatureListForEntry4[4] = {
    GPU_FEATURE_TYPE_FLASH3D, GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL, GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
};

const uint32_t kCrBugsForEntry4[1] = {
    232035,
};

const uint32_t kDeviceIDsForEntry4[2] = {
    0x27AE, 0x27A2,
};

const int kFeatureListForEntry5[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry5[6] = {
    71381, 76428, 73910, 101225, 136240, 357314,
};

const GpuControlList::DriverInfo kDriverInfoForEntry5Exception0 = {
    ".*AMD.*",  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleLexical, "8.98",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::DriverInfo kDriverInfoForEntry5Exception1 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "10.0.4",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::DriverInfo kDriverInfoForEntry5Exception2 = {
    ".*ANGLE.*",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry8[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry8[1] = {
    72938,
};

const uint32_t kDeviceIDsForEntry8[1] = {
    0x0324,
};

const int kFeatureListForEntry10[4] = {
    GPU_FEATURE_TYPE_FLASH3D, GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL, GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry10[1] = {
    73794,
};

const uint32_t kDeviceIDsForEntry10[1] = {
    0x0393,
};

const int kFeatureListForEntry12[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry12[3] = {
    72979, 89802, 315205,
};

const GpuControlList::DriverInfo kDriverInfoForEntry12 = {
    nullptr,  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "2009.1",
     nullptr},  // driver_date
};

const uint32_t kDeviceIDsForEntry12Exception0[1] = {
    0x29a2,
};

const GpuControlList::DriverInfo kDriverInfoForEntry12Exception0 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
     "7.15.10.1624", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::DriverInfo kDriverInfoForEntry12Exception1 = {
    "osmesa",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const uint32_t kDeviceIDsForEntry12Exception2[1] = {
    0x02c1,
};

const int kFeatureListForEntry17[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry17[4] = {
    76703, 164555, 225200, 340886,
};

const GpuControlList::DriverInfo kDriverInfoForEntry17 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "10.1",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const uint32_t kDeviceIDsForEntry17Exception0[12] = {
    0x0102, 0x0106, 0x0112, 0x0116, 0x0122, 0x0126,
    0x010a, 0x0152, 0x0156, 0x015a, 0x0162, 0x0166,
};

const GpuControlList::DriverInfo kDriverInfoForEntry17Exception0 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "8.0",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const uint32_t kDeviceIDsForEntry17Exception1[16] = {
    0xa001, 0xa002, 0xa011, 0xa012, 0x29a2, 0x2992, 0x2982, 0x2972,
    0x2a12, 0x2a42, 0x2e02, 0x2e12, 0x2e22, 0x2e32, 0x2e42, 0x2e92,
};

const GpuControlList::DriverInfo kDriverInfoForEntry17Exception1 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGT, GpuControlList::kVersionStyleNumerical, "8.0.2",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const uint32_t kDeviceIDsForEntry17Exception2[2] = {
    0x0042, 0x0046,
};

const GpuControlList::DriverInfo kDriverInfoForEntry17Exception2 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGT, GpuControlList::kVersionStyleNumerical, "8.0.4",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const uint32_t kDeviceIDsForEntry17Exception3[1] = {
    0x2a02,
};

const GpuControlList::DriverInfo kDriverInfoForEntry17Exception3 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "9.1",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const uint32_t kDeviceIDsForEntry17Exception4[2] = {
    0x0a16, 0x0a26,
};

const GpuControlList::DriverInfo kDriverInfoForEntry17Exception4 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "10.0.1",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry18[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry18[1] = {
    84701,
};

const uint32_t kDeviceIDsForEntry18[1] = {
    0x029e,
};

const int kFeatureListForEntry27[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry27[4] = {
    95934, 94973, 136240, 357314,
};

const GpuControlList::GLStrings kGLStringsForEntry27 = {
    "ATI.*", nullptr, nullptr, nullptr,
};

const GpuControlList::DriverInfo kDriverInfoForEntry27Exception0 = {
    ".*AMD.*",  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleLexical, "8.98",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::DriverInfo kDriverInfoForEntry27Exception1 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "10.0.4",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry28[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry28[3] = {
    95934, 94973, 357314,
};

const GpuControlList::GLStrings kGLStringsForEntry28 = {
    "X\\.Org.*", ".*AMD.*", nullptr, nullptr,
};

const GpuControlList::DriverInfo kDriverInfoForEntry28Exception0 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "10.0.4",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry29[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry29[3] = {
    95934, 94973, 357314,
};

const GpuControlList::GLStrings kGLStringsForEntry29 = {
    "X\\.Org.*", ".*ATI.*", nullptr, nullptr,
};

const GpuControlList::DriverInfo kDriverInfoForEntry29Exception0 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "10.0.4",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry30[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry30[1] = {
    94103,
};

const GpuControlList::DriverInfo kDriverInfoForEntry30 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "10.1",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry30 = {
    "(?i)nouveau.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry34[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry34[1] = {
    119948,
};

const uint32_t kDeviceIDsForEntry34[1] = {
    0x8811,
};

const int kFeatureListForEntry37[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry37[2] = {
    131308, 363418,
};

const GpuControlList::DriverInfo kDriverInfoForEntry37 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "10.1",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry37 = {
    "Intel.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry45[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry45[1] = {
    138105,
};

const GpuControlList::DriverInfo kDriverInfoForEntry45 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "7",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry46[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry46[1] = {
    124152,
};

const uint32_t kDeviceIDsForEntry46[1] = {
    0x3151,
};

const int kFeatureListForEntry47[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry47[1] = {
    78497,
};

const GpuControlList::DriverInfo kDriverInfoForEntry47 = {
    "NVIDIA",  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "295",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry48[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry48[1] = {
    137247,
};

const int kFeatureListForEntry50[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry50[4] = {
    145531, 332596, 571899, 629434,
};

const GpuControlList::GLStrings kGLStringsForEntry50 = {
    "VMware.*", nullptr, nullptr, nullptr,
};

const GpuControlList::DriverInfo kDriverInfoForEntry50Exception0 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "9.2.1",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry50Exception0 = {
    nullptr, ".*SVGA3D.*", nullptr, nullptr,
};

const GpuControlList::DriverInfo kDriverInfoForEntry50Exception1 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "10.1.3",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry50Exception1 = {
    nullptr, ".*Gallium.*llvmpipe.*", nullptr, nullptr,
};

const int kFeatureListForEntry53[1] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
};

const uint32_t kCrBugsForEntry53[1] = {
    152096,
};

const uint32_t kDeviceIDsForEntry53[2] = {
    0x8108, 0x8109,
};

const int kFeatureListForEntry56[3] = {
    GPU_FEATURE_TYPE_FLASH3D, GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry56[1] = {
    145600,
};

const GpuControlList::DriverInfo kDriverInfoForEntry56 = {
    "NVIDIA",  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "331.38",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry57[1] = {
    GPU_FEATURE_TYPE_PANEL_FITTING,
};

const uint32_t kDeviceIDsForEntry57Exception0[3] = {
    0x0106, 0x0116, 0x0166,
};

const int kFeatureListForEntry59[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry59[1] = {
    155749,
};

const GpuControlList::DriverInfo kDriverInfoForEntry59 = {
    nullptr,  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
     "8.15.11.8593", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry64[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry64[1] = {
    159458,
};

const int kFeatureListForEntry68[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry68[1] = {
    169470,
};

const GpuControlList::DriverInfo kDriverInfoForEntry68 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical, "7.14.1.1134",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry69[1] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
};

const uint32_t kCrBugsForEntry69[1] = {
    172771,
};

const GpuControlList::DriverInfo kDriverInfoForEntry69 = {
    nullptr,  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
     "8.17.11.9621", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry70[1] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
};

const uint32_t kCrBugsForEntry70[1] = {
    172771,
};

const GpuControlList::DriverInfo kDriverInfoForEntry70 = {
    nullptr,  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
     "8.17.11.8267", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry71[1] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
};

const uint32_t kCrBugsForEntry71[1] = {
    172771,
};

const GpuControlList::DriverInfo kDriverInfoForEntry71 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
     "8.15.10.2021", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry72[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

const uint32_t kCrBugsForEntry72[1] = {
    232529,
};

const uint32_t kDeviceIDsForEntry72[1] = {
    0x0163,
};

const int kFeatureListForEntry74[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry74[1] = {
    248178,
};

const GpuControlList::DriverInfo kDriverInfoForEntry74 = {
    "Microsoft",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const uint32_t kDeviceIDsForEntry74Exception0[1] = {
    0x02c1,
};

const int kFeatureListForEntry76[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

const GpuControlList::More kMoreForEntry76 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    true,       // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const GpuControlList::More kMoreForEntry76Exception0 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    33362,      // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const GpuControlList::GLStrings kGLStringsForEntry76Exception1 = {
    nullptr, "Mali-4.*", ".*EXT_robustness.*", nullptr,
};

const int kFeatureListForEntry78[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry78[3] = {
    180695, 298968, 436968,
};

const GpuControlList::DriverInfo kDriverInfoForEntry78 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
     "8.15.10.2702", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry79[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry79[1] = {
    315199,
};

const int kFeatureListForEntry82[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry82[1] = {
    615108,
};

const int kFeatureListForEntry86[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
};

const uint32_t kCrBugsForEntry86[1] = {
    305431,
};

const uint32_t kDeviceIDsForEntry86[1] = {
    0xa011,
};

const int kFeatureListForEntry87[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry87[1] = {
    298968,
};

const GpuControlList::DriverInfo kDriverInfoForEntry87 = {
    nullptr,  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
     "10.18.10.3308", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry88[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry88[1] = {
    298968,
};

const GpuControlList::DriverInfo kDriverInfoForEntry88 = {
    nullptr,  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
     "13.152.1.8000", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry89[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry89[1] = {
    298968,
};

const GpuControlList::DriverInfo kDriverInfoForEntry89 = {
    nullptr,  // driver_vendor
    {GpuControlList::kBetween, GpuControlList::kVersionStyleNumerical,
     "8.810.4.5000", "8.970.100.1100"},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry90[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry90[1] = {
    298968,
};

const GpuControlList::DriverInfo kDriverInfoForEntry90 = {
    nullptr,  // driver_vendor
    {GpuControlList::kBetween, GpuControlList::kVersionStyleNumerical,
     "8.17.12.5729", "8.17.12.8026"},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry91[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry91[1] = {
    298968,
};

const GpuControlList::DriverInfo kDriverInfoForEntry91 = {
    nullptr,  // driver_vendor
    {GpuControlList::kBetween, GpuControlList::kVersionStyleNumerical,
     "9.18.13.783", "9.18.13.1090"},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry92[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry92[1] = {
    298968,
};

const int kFeatureListForEntry93[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry93[1] = {
    72373,
};

const GpuControlList::More kMoreForEntry93 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    false,      // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const int kFeatureListForEntry94[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry94[1] = {
    350566,
};

const GpuControlList::DriverInfo kDriverInfoForEntry94 = {
    nullptr,  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
     "8.15.10.1749", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry95[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry95[1] = {
    363378,
};

const GpuControlList::DriverInfo kDriverInfoForEntry95 = {
    ".*AMD.*",  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "13.101",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry96[2] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry96[2] = {
    362779, 424970,
};

const GpuControlList::GLStrings kGLStringsForEntry96Exception0 = {
    nullptr, "Adreno \\(TM\\) 3.*", nullptr, nullptr,
};

const GpuControlList::GLStrings kGLStringsForEntry96Exception1 = {
    nullptr, "Mali-4.*", nullptr, nullptr,
};

const GpuControlList::GLStrings kGLStringsForEntry96Exception2 = {
    nullptr, "NVIDIA.*", nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry96Exception3 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "3.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const GpuControlList::GLStrings kGLStringsForEntry96Exception4 = {
    nullptr, ".*Google.*", nullptr, nullptr,
};

const int kFeatureListForEntry100[2] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry100[1] = {
    407144,
};

const GpuControlList::GLStrings kGLStringsForEntry100 = {
    nullptr, ".*Mali-T604.*", nullptr, nullptr,
};

const int kFeatureListForEntry102[2] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry102[1] = {
    416910,
};

const GpuControlList::GLStrings kGLStringsForEntry102 = {
    nullptr, "PowerVR SGX 540", nullptr, nullptr,
};

const int kFeatureListForEntry104[2] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry104[2] = {
    436331, 483574,
};

const GpuControlList::GLStrings kGLStringsForEntry104 = {
    nullptr, "PowerVR Rogue.*", nullptr, nullptr,
};

const int kFeatureListForEntry105[2] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry105[1] = {
    461456,
};

const GpuControlList::GLStrings kGLStringsForEntry105 = {
    nullptr, "PowerVR SGX.*", nullptr, nullptr,
};

const int kFeatureListForEntry106[2] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry106[1] = {
    480149,
};

const GpuControlList::GLStrings kGLStringsForEntry106 = {
    nullptr, "Adreno \\(TM\\) 3.*", nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry106 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical, "2.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const int kFeatureListForEntry107[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry107[1] = {
    463243,
};

const uint32_t kDeviceIDsForEntry107[15] = {
    0x0402, 0x0406, 0x040a, 0x040b, 0x040e, 0x0a02, 0x0a06, 0x0a0a,
    0x0a0b, 0x0a0e, 0x0d02, 0x0d06, 0x0d0a, 0x0d0b, 0x0d0e,
};

const int kFeatureListForEntry108[2] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry108[1] = {
    560587,
};

const GpuControlList::GLStrings kGLStringsForEntry108 = {
    nullptr, ".*Vivante.*", nullptr, nullptr,
};

const int kFeatureListForEntry109[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry109[1] = {
    585963,
};

const GpuControlList::DriverInfo kDriverInfoForEntry109 = {
    nullptr,  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "45.0",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry109 = {
    nullptr, "Adreno \\(TM\\) 330", nullptr, nullptr,
};

const int kFeatureListForEntry110[11] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry110[1] = {
    571899,
};

const GpuControlList::DriverInfo kDriverInfoForEntry110 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry110 = {
    "VMware.*", ".*Gallium.*llvmpipe.*", nullptr, nullptr,
};

const int kFeatureListForEntry111[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry111[1] = {
    607829,
};

const int kFeatureListForEntry112[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry112[1] = {
    592130,
};

const uint32_t kDeviceIDsForEntry112[2] = {
    0x0116, 0x0126,
};

const int kFeatureListForEntry113[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry113[2] = {
    613272, 614468,
};

const uint32_t kDeviceIDsForEntry113[8] = {
    0x0126, 0x0116, 0x191e, 0x0046, 0x1912, 0x2a02, 0x27a2, 0x2a42,
};

const int kFeatureListForEntry114[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry114[2] = {
    613272, 614468,
};

const uint32_t kDeviceIDsForEntry114[142] = {
    0x0863, 0x08a0, 0x0a29, 0x0869, 0x0867, 0x08a3, 0x11a3, 0x08a2, 0x0407,
    0x0861, 0x08a4, 0x0647, 0x0640, 0x0866, 0x0655, 0x062e, 0x0609, 0x1187,
    0x13c2, 0x0602, 0x1180, 0x1401, 0x0fc8, 0x0611, 0x1189, 0x11c0, 0x0870,
    0x0a65, 0x06dd, 0x0fc1, 0x1380, 0x11c6, 0x104a, 0x1184, 0x0fc6, 0x13c0,
    0x1381, 0x05e3, 0x1183, 0x05fe, 0x1004, 0x17c8, 0x11ba, 0x0a20, 0x0f00,
    0x0ca3, 0x06fd, 0x0f02, 0x0614, 0x0402, 0x13bb, 0x0401, 0x0f01, 0x1287,
    0x0615, 0x1402, 0x019d, 0x0400, 0x0622, 0x06e4, 0x06cd, 0x1201, 0x100a,
    0x10c3, 0x1086, 0x17c2, 0x1005, 0x0a23, 0x0de0, 0x1040, 0x0421, 0x1282,
    0x0e22, 0x0e23, 0x0610, 0x11c8, 0x11c2, 0x1188, 0x0de9, 0x1200, 0x1244,
    0x0dc4, 0x0df8, 0x0641, 0x0613, 0x11fa, 0x100c, 0x0de1, 0x0ca5, 0x0cb1,
    0x0a6c, 0x05ff, 0x05e2, 0x0a2d, 0x06c0, 0x1288, 0x1048, 0x1081, 0x0dd8,
    0x05e6, 0x11c4, 0x0605, 0x1080, 0x042f, 0x0ca2, 0x1245, 0x124d, 0x1284,
    0x0191, 0x1050, 0x0ffd, 0x0193, 0x061a, 0x0422, 0x1185, 0x103a, 0x0fc2,
    0x0194, 0x0df5, 0x040e, 0x065b, 0x0de2, 0x0a75, 0x0601, 0x1087, 0x019e,
    0x104b, 0x107d, 0x1382, 0x042b, 0x1049, 0x0df0, 0x11a1, 0x040f, 0x0de3,
    0x0fc0, 0x13d8, 0x0de4, 0x11e2, 0x0644, 0x0fd1, 0x0dfa,
};

const int kFeatureListForEntry115[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry115[2] = {
    613272, 614468,
};

const uint32_t kDeviceIDsForEntry115[41] = {
    0x6741, 0x6740, 0x9488, 0x9583, 0x6720, 0x6760, 0x68c0, 0x68a1, 0x944a,
    0x94c8, 0x6819, 0x68b8, 0x6920, 0x6938, 0x6640, 0x9588, 0x6898, 0x9440,
    0x6738, 0x6739, 0x6818, 0x6758, 0x6779, 0x9490, 0x68d9, 0x683f, 0x683d,
    0x6899, 0x6759, 0x68e0, 0x68d8, 0x68ba, 0x68f9, 0x9501, 0x68a0, 0x6841,
    0x6840, 0x9442, 0x6658, 0x68c8, 0x68c1,
};

const int kFeatureListForEntry116[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry116[2] = {
    613272, 614468,
};

const int kFeatureListForEntry117[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry117[1] = {
    626814,
};

const GpuControlList::GLStrings kGLStringsForEntry117 = {
    nullptr, ".*Vivante.*", nullptr, nullptr,
};

const int kFeatureListForEntry118[2] = {
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL, GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
};

const uint32_t kCrBugsForEntry118[1] = {
    628059,
};

const GpuControlList::GLStrings kGLStringsForEntry118 = {
    "Vivante.*", ".*PXA.*", nullptr, nullptr,
};

const int kFeatureListForEntry119[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry119[1] = {
    611310,
};

const int kFeatureListForEntry120[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
};

const uint32_t kCrBugsForEntry120[1] = {
    616318,
};

const int kFeatureListForEntry121[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
};

const uint32_t kCrBugsForEntry121[1] = {
    616318,
};

const uint32_t kDeviceIDsForEntry121[47] = {
    0x1602, 0x1606, 0x160a, 0x160b, 0x160d, 0x160e, 0x1612, 0x1616,
    0x161a, 0x161b, 0x161d, 0x161e, 0x1622, 0x1626, 0x162a, 0x162b,
    0x162d, 0x162e, 0x22b0, 0x22b1, 0x22b2, 0x22b3, 0x1902, 0x1906,
    0x190a, 0x190b, 0x190e, 0x1912, 0x1913, 0x1915, 0x1916, 0x1917,
    0x191a, 0x191b, 0x191d, 0x191e, 0x1921, 0x1923, 0x1926, 0x1927,
    0x192a, 0x192b, 0x192d, 0x1932, 0x193a, 0x193b, 0x193d,
};

const int kFeatureListForEntry122[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry122[1] = {
    643850,
};

const GpuControlList::More kMoreForEntry122Exception0 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "5.0",
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const GpuControlList::More kMoreForEntry122Exception1 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "5.0",
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const GpuControlList::DriverInfo kDriverInfoForEntry122Exception2 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "15.201",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::More kMoreForEntry122Exception2 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "5.0",
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const int kFeatureListForEntry123[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
};

const uint32_t kCrBugsForEntry123[1] = {
    654111,
};

const GpuControlList::DriverInfo kDriverInfoForEntry123 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
     "21.20.16.4542", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry124[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry124[1] = {
    653538,
};

const GpuControlList::DriverInfo kDriverInfoForEntry124 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGT, GpuControlList::kVersionStyleNumerical,
     "16.200.1035.1001", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::DriverInfo kDriverInfoForEntry124Exception0 = {
    nullptr,  // driver_vendor
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "21.19.384.0",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry125[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry125[2] = {
    656572, 658668,
};

const uint32_t kDeviceIDsForEntry125[1] = {
    0xbeef,
};

const int kFeatureListForEntry126[1] = {
    GPU_FEATURE_TYPE_WEBGL2,
};

const uint32_t kCrBugsForEntry126[1] = {
    295792,
};

const GpuControlList::More kMoreForEntry126 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.1",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const int kFeatureListForEntry129[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry129[1] = {
    662909,
};

const int kFeatureListForEntry130[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry130[2] = {
    676829, 676975,
};

const uint32_t kDeviceIDsForEntry130[3] = {
    0x0407, 0x0647, 0x0863,
};

const int kFeatureListForEntry131[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry131[1] = {
    462426,
};

const uint32_t kDeviceIDsForEntry131[1] = {
    0x2a02,
};

const GpuControlList::DriverInfo kDriverInfoForEntry131 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "10.4.3",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry132[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
};

const uint32_t kCrBugsForEntry132[1] = {
    691601,
};

const GpuControlList::GLStrings kGLStringsForEntry132 = {
    nullptr, "Mali-4.*", nullptr, nullptr,
};

const int kFeatureListForEntry133[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
};

const uint32_t kCrBugsForEntry133[1] = {
    654905,
};

const GpuControlList::GLStrings kGLStringsForEntry133 = {
    nullptr, ".*VideoCore IV.*", nullptr, nullptr,
};

const int kFeatureListForEntry134[12] = {
    GPU_FEATURE_TYPE_FLASH_STAGE3D,
    GPU_FEATURE_TYPE_GPU_COMPOSITING,
    GPU_FEATURE_TYPE_PANEL_FITTING,
    GPU_FEATURE_TYPE_FLASH3D,
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
    GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
    GPU_FEATURE_TYPE_WEBGL2,
    GPU_FEATURE_TYPE_ACCELERATED_VPX_DECODE,
    GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
    GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE,
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const uint32_t kCrBugsForEntry134[1] = {
    629434,
};

const GpuControlList::DriverInfo kDriverInfoForEntry134 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical, "10.1.3",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry134Exception0 = {
    nullptr, ".*SVGA3D.*", nullptr, nullptr,
};

const GpuControlList::GLStrings kGLStringsForEntry134Exception1 = {
    nullptr, ".*Gallium.*llvmpipe.*", nullptr, nullptr,
};

const int kFeatureListForEntry135[1] = {
    GPU_FEATURE_TYPE_WEBGL2,
};

const uint32_t kCrBugsForEntry135[1] = {
    471200,
};

const GpuControlList::GLStrings kGLStringsForEntry135 = {
    nullptr, "Adreno \\(TM\\) 4.*", nullptr, nullptr,
};

const int kFeatureListForEntry136[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry136[1] = {
    643850,
};

const uint32_t kDeviceIDsForEntry136[113] = {
    0x06c0, 0x06c4, 0x06ca, 0x06cb, 0x06cd, 0x06d1, 0x06d2, 0x06d8, 0x06d9,
    0x06da, 0x06dc, 0x06dd, 0x06de, 0x06df, 0x0e22, 0x0e23, 0x0e24, 0x0e30,
    0x0e31, 0x0e3a, 0x0e3b, 0x1200, 0x1201, 0x1202, 0x1203, 0x1205, 0x1206,
    0x1207, 0x1208, 0x1210, 0x1211, 0x1212, 0x1213, 0x0dc0, 0x0dc4, 0x0dc5,
    0x0dc6, 0x0dcd, 0x0dce, 0x0dd1, 0x0dd2, 0x0dd3, 0x0dd6, 0x0dd8, 0x0dda,
    0x1241, 0x1243, 0x1244, 0x1245, 0x1246, 0x1247, 0x1248, 0x1249, 0x124b,
    0x124d, 0x1251, 0x0de0, 0x0de1, 0x0de2, 0x0de3, 0x0de4, 0x0de5, 0x0de8,
    0x0de9, 0x0dea, 0x0deb, 0x0dec, 0x0ded, 0x0dee, 0x0def, 0x0df0, 0x0df1,
    0x0df2, 0x0df3, 0x0df4, 0x0df5, 0x0df6, 0x0df7, 0x0df8, 0x0df9, 0x0dfa,
    0x0dfc, 0x0f00, 0x0f01, 0x1080, 0x1081, 0x1082, 0x1084, 0x1086, 0x1087,
    0x1088, 0x1089, 0x108b, 0x1091, 0x109a, 0x109b, 0x1040, 0x1042, 0x1048,
    0x1049, 0x104a, 0x1050, 0x1051, 0x1052, 0x1054, 0x1055, 0x1056, 0x1057,
    0x1058, 0x1059, 0x105a, 0x107d, 0x1140,
};

const int kFeatureListForEntry137[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry137[1] = {
    684094,
};

const int kFeatureListForEntry138[1] = {
    GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
};

const int kFeatureListForEntry139[1] = {
    GPU_FEATURE_TYPE_GPU_RASTERIZATION,
};

const uint32_t kCrBugsForEntry139[1] = {
    643850,
};

const GpuControlList::DriverInfo kDriverInfoForEntry139 = {
    nullptr,  // driver_vendor
    {GpuControlList::kBetween, GpuControlList::kVersionStyleNumerical, "15.301",
     "15.302"},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry140[1] = {
    GPU_FEATURE_TYPE_WEBGL2,
};

const uint32_t kCrBugsForEntry140[2] = {
    449116, 698197,
};

const GpuControlList::GLStrings kGLStringsForEntry140 = {
    nullptr, "Adreno \\(TM\\) 3.*", nullptr, nullptr,
};

}  // namespace gpu

#endif  // GPU_CONFIG_SOFTWARE_RENDERING_LIST_ARRAYS_AND_STRUCTS_AUTOGEN_H_
