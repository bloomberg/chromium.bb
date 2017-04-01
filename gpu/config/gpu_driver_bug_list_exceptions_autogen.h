// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_CONFIG_GPU_DRIVER_BUG_LIST_EXCEPTIONS_AUTOGEN_H_
#define GPU_CONFIG_GPU_DRIVER_BUG_LIST_EXCEPTIONS_AUTOGEN_H_

namespace gpu {
const GpuControlList::Conditions kExceptionsForEntry54[1] = {
    {
        GpuControlList::kOsMacosx,  // os_type
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         nullptr, nullptr},                     // os_version
        0x00,                                   // vendor_id
        0,                                      // DeviceIDs size
        nullptr,                                // DeviceIDs
        GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
        GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
        nullptr,                                // driver info
        nullptr,                                // GL strings
        nullptr,                                // machine model info
        nullptr,                                // more conditions
    },
};

const GpuControlList::Conditions kExceptionsForEntry172[1] = {
    {
        GpuControlList::kOsChromeOS,  // os_type
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         nullptr, nullptr},                     // os_version
        0x8086,                                 // vendor_id
        0,                                      // DeviceIDs size
        nullptr,                                // DeviceIDs
        GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
        GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
        &kDriverInfoForEntry172Exception0,      // driver info
        &kGLStringsForEntry172Exception0,       // GL strings
        nullptr,                                // machine model info
        &kMoreForEntry172Exception0,            // more data
    },
};

const GpuControlList::Conditions kExceptionsForEntry192[1] = {
    {
        GpuControlList::kOsMacosx,  // os_type
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         nullptr, nullptr},                     // os_version
        0x00,                                   // vendor_id
        0,                                      // DeviceIDs size
        nullptr,                                // DeviceIDs
        GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
        GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
        nullptr,                                // driver info
        nullptr,                                // GL strings
        nullptr,                                // machine model info
        nullptr,                                // more conditions
    },
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_DRIVER_BUG_LIST_EXCEPTIONS_AUTOGEN_H_
