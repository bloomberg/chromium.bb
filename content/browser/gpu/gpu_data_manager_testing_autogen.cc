// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "content/browser/gpu/gpu_data_manager_testing_autogen.h"

#include "content/browser/gpu/gpu_data_manager_testing_arrays_and_structs_autogen.h"
#include "content/browser/gpu/gpu_data_manager_testing_exceptions_autogen.h"

namespace gpu {

const GpuControlList::Entry kGpuDataManagerTestingEntries[] = {
    {
        1,  // id
        "GpuDataManagerImplPrivateTest.GpuSideBlacklisting.0",
        arraysize(kFeatureListForEntry1),  // features size
        kFeatureListForEntry1,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        2,  // id
        "GpuDataManagerImplPrivateTest.GpuSideBlacklisting.1",
        arraysize(kFeatureListForEntry2),  // features size
        kFeatureListForEntry2,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry2,                   // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        3,  // id
        "GpuDataManagerImplPrivateTest.GpuSideBlacklistingWebGL.0",
        arraysize(kFeatureListForEntry3),  // features size
        kFeatureListForEntry3,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        4,  // id
        "GpuDataManagerImplPrivateTest.GpuSideBlacklistingWebGL.1",
        arraysize(kFeatureListForEntry4),  // features size
        kFeatureListForEntry4,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry4,                   // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        5,  // id
        "GpuDataManagerImplPrivateTest.GpuSideException",
        arraysize(kFeatureListForEntry5),  // features size
        kFeatureListForEntry5,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
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
        arraysize(kExceptionsForEntry5),  // exceptions count
        kExceptionsForEntry5,             // exceptions
    },
    {
        6,  // id
        "GpuDataManagerImplPrivateTest.SetGLStrings",
        arraysize(kFeatureListForEntry6),  // features size
        kFeatureListForEntry6,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry6),  // exceptions count
        kExceptionsForEntry6,             // exceptions
    },
    {
        7,  // id
        "GpuDataManagerImplPrivateTest.SetGLStringsNoEffects",
        arraysize(kFeatureListForEntry7),  // features size
        kFeatureListForEntry7,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry7),  // exceptions count
        kExceptionsForEntry7,             // exceptions
    },
    {
        8,  // id
        "GpuDataManagerImplPrivateTest.SetGLStringsDefered",
        arraysize(kFeatureListForEntry8),  // features size
        kFeatureListForEntry8,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry8),         // DeviceIDs size
            kDeviceIDsForEntry8,                    // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry8,                  // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        9,  // id
        "GpuDataManagerImplPrivateTest.BlacklistAllFeatures",
        arraysize(kFeatureListForEntry9),  // features size
        kFeatureListForEntry9,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        10,  // id
        "GpuDataManagerImplPrivateTest.UpdateActiveGpu",
        arraysize(kFeatureListForEntry10),  // features size
        kFeatureListForEntry10,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x8086,                                   // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
};
const size_t kGpuDataManagerTestingEntryCount = 10;
}  // namespace gpu
