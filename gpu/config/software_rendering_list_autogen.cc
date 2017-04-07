// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/config/software_rendering_list_autogen.h"

#include "gpu/config/software_rendering_list_arrays_and_structs_autogen.h"
#include "gpu/config/software_rendering_list_exceptions_autogen.h"

namespace gpu {

const char kSoftwareRenderingListVersion[] = "13.2";

const size_t kSoftwareRenderingListEntryCount = 82;
const GpuControlList::Entry kSoftwareRenderingListEntries[82] = {
    {
        1,  // id
        "ATI Radeon X1900 is not compatible with WebGL on the Mac",
        arraysize(kFeatureListForEntry1),  // features size
        kFeatureListForEntry1,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x1002,                                // vendor_id
            arraysize(kDeviceIDsForEntry1),        // DeviceIDs size
            kDeviceIDsForEntry1,                   // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        3,  // id
        "GL driver is software rendered. GPU acceleration is disabled",
        arraysize(kFeatureListForEntry3),  // features size
        kFeatureListForEntry3,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        arraysize(kCrBugsForEntry3),       // CrBugs size
        kCrBugsForEntry3,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry3,                   // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        4,  // id
        "The Intel Mobile 945 Express family of chipsets is not compatible "
        "with WebGL",
        arraysize(kFeatureListForEntry4),  // features size
        kFeatureListForEntry4,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        arraysize(kCrBugsForEntry4),       // CrBugs size
        kCrBugsForEntry4,                  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry4),         // DeviceIDs size
            kDeviceIDsForEntry4,                    // DeviceIDs
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
        5,  // id
        "ATI/AMD cards with older drivers in Linux are crash-prone",
        arraysize(kFeatureListForEntry5),  // features size
        kFeatureListForEntry5,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        arraysize(kCrBugsForEntry5),       // CrBugs size
        kCrBugsForEntry5,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
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
        8,  // id
        "NVIDIA GeForce FX Go5200 is assumed to be buggy",
        arraysize(kFeatureListForEntry8),  // features size
        kFeatureListForEntry8,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        arraysize(kCrBugsForEntry8),       // CrBugs size
        kCrBugsForEntry8,                  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            arraysize(kDeviceIDsForEntry8),         // DeviceIDs size
            kDeviceIDsForEntry8,                    // DeviceIDs
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
        "NVIDIA GeForce 7300 GT on Mac does not support WebGL",
        arraysize(kFeatureListForEntry10),  // features size
        kFeatureListForEntry10,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry10),       // CrBugs size
        kCrBugsForEntry10,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x10de,                                // vendor_id
            arraysize(kDeviceIDsForEntry10),       // DeviceIDs size
            kDeviceIDsForEntry10,                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        12,  // id
        "Drivers older than 2009-01 on Windows are possibly unreliable",
        arraysize(kFeatureListForEntry12),  // features size
        kFeatureListForEntry12,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry12),       // CrBugs size
        kCrBugsForEntry12,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry12,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry12),  // exceptions count
        kExceptionsForEntry12,             // exceptions
    },
    {
        17,  // id
        "Older Intel mesa drivers are crash-prone",
        arraysize(kFeatureListForEntry17),  // features size
        kFeatureListForEntry17,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry17),       // CrBugs size
        kCrBugsForEntry17,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry17,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry17),  // exceptions count
        kExceptionsForEntry17,             // exceptions
    },
    {
        18,  // id
        "NVIDIA Quadro FX 1500 is buggy",
        arraysize(kFeatureListForEntry18),  // features size
        kFeatureListForEntry18,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry18),       // CrBugs size
        kCrBugsForEntry18,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            arraysize(kDeviceIDsForEntry18),        // DeviceIDs size
            kDeviceIDsForEntry18,                   // DeviceIDs
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
        27,  // id
        "ATI/AMD cards with older drivers in Linux are crash-prone",
        arraysize(kFeatureListForEntry27),  // features size
        kFeatureListForEntry27,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry27),       // CrBugs size
        kCrBugsForEntry27,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry27,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry27),  // exceptions count
        kExceptionsForEntry27,             // exceptions
    },
    {
        28,  // id
        "ATI/AMD cards with third-party drivers in Linux are crash-prone",
        arraysize(kFeatureListForEntry28),  // features size
        kFeatureListForEntry28,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry28),       // CrBugs size
        kCrBugsForEntry28,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry28,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry28),  // exceptions count
        kExceptionsForEntry28,             // exceptions
    },
    {
        29,  // id
        "ATI/AMD cards with third-party drivers in Linux are crash-prone",
        arraysize(kFeatureListForEntry29),  // features size
        kFeatureListForEntry29,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry29),       // CrBugs size
        kCrBugsForEntry29,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry29,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry29),  // exceptions count
        kExceptionsForEntry29,             // exceptions
    },
    {
        30,  // id
        "NVIDIA cards with nouveau drivers in Linux are crash-prone",
        arraysize(kFeatureListForEntry30),  // features size
        kFeatureListForEntry30,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry30),       // CrBugs size
        kCrBugsForEntry30,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry30,                 // driver info
            &kGLStringsForEntry30,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        34,  // id
        "S3 Trio (used in Virtual PC) is not compatible",
        arraysize(kFeatureListForEntry34),  // features size
        kFeatureListForEntry34,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry34),       // CrBugs size
        kCrBugsForEntry34,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x5333,                                 // vendor_id
            arraysize(kDeviceIDsForEntry34),        // DeviceIDs size
            kDeviceIDsForEntry34,                   // DeviceIDs
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
        37,  // id
        "Older drivers are unreliable for Optimus on Linux",
        arraysize(kFeatureListForEntry37),  // features size
        kFeatureListForEntry37,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry37),       // CrBugs size
        kCrBugsForEntry37,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleOptimus,  // multi_gpu_style
            &kDriverInfoForEntry37,                 // driver info
            &kGLStringsForEntry37,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        45,  // id
        "Parallels drivers older than 7 are buggy",
        arraysize(kFeatureListForEntry45),  // features size
        kFeatureListForEntry45,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry45),       // CrBugs size
        kCrBugsForEntry45,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1ab8,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry45,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        46,  // id
        "ATI FireMV 2400 cards on Windows are buggy",
        arraysize(kFeatureListForEntry46),  // features size
        kFeatureListForEntry46,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry46),       // CrBugs size
        kCrBugsForEntry46,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            arraysize(kDeviceIDsForEntry46),        // DeviceIDs size
            kDeviceIDsForEntry46,                   // DeviceIDs
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
        47,  // id
        "NVIDIA linux drivers older than 295.* are assumed to be buggy",
        arraysize(kFeatureListForEntry47),  // features size
        kFeatureListForEntry47,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry47),       // CrBugs size
        kCrBugsForEntry47,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry47,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        48,  // id
        "Accelerated video decode is unavailable on Linux",
        arraysize(kFeatureListForEntry48),  // features size
        kFeatureListForEntry48,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry48),       // CrBugs size
        kCrBugsForEntry48,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
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
        50,  // id
        "Disable VMware software renderer on older Mesa",
        arraysize(kFeatureListForEntry50),  // features size
        kFeatureListForEntry50,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry50),       // CrBugs size
        kCrBugsForEntry50,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry50,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry50),  // exceptions count
        kExceptionsForEntry50,             // exceptions
    },
    {
        53,  // id
        "The Intel GMA500 is too slow for Stage3D",
        arraysize(kFeatureListForEntry53),  // features size
        kFeatureListForEntry53,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry53),       // CrBugs size
        kCrBugsForEntry53,                  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry53),        // DeviceIDs size
            kDeviceIDsForEntry53,                   // DeviceIDs
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
        56,  // id
        "NVIDIA linux drivers are unstable when using multiple Open GL "
        "contexts and with low memory",
        arraysize(kFeatureListForEntry56),  // features size
        kFeatureListForEntry56,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry56),       // CrBugs size
        kCrBugsForEntry56,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry56,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        57,  // id
        "Chrome OS panel fitting is only supported for Intel IVB and SNB "
        "Graphics Controllers",
        arraysize(kFeatureListForEntry57),  // features size
        kFeatureListForEntry57,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
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
        arraysize(kExceptionsForEntry57),  // exceptions count
        kExceptionsForEntry57,             // exceptions
    },
    {
        59,  // id
        "NVidia driver 8.15.11.8593 is crashy on Windows",
        arraysize(kFeatureListForEntry59),  // features size
        kFeatureListForEntry59,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry59),       // CrBugs size
        kCrBugsForEntry59,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry59,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        64,  // id
        "Hardware video decode is only supported in win7+",
        arraysize(kFeatureListForEntry64),  // features size
        kFeatureListForEntry64,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry64),       // CrBugs size
        kCrBugsForEntry64,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "6.1",
             nullptr},                              // os_version
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
        68,  // id
        "VMware Fusion 4 has corrupt rendering with Win Vista+",
        arraysize(kFeatureListForEntry68),  // features size
        kFeatureListForEntry68,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry68),       // CrBugs size
        kCrBugsForEntry68,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "6.0",
             nullptr},                              // os_version
            0x15ad,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry68,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        69,  // id
        "NVIDIA driver 8.17.11.9621 is buggy with Stage3D baseline mode",
        arraysize(kFeatureListForEntry69),  // features size
        kFeatureListForEntry69,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry69),       // CrBugs size
        kCrBugsForEntry69,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry69,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        70,  // id
        "NVIDIA driver 8.17.11.8267 is buggy with Stage3D baseline mode",
        arraysize(kFeatureListForEntry70),  // features size
        kFeatureListForEntry70,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry70),       // CrBugs size
        kCrBugsForEntry70,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry70,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        71,  // id
        "All Intel drivers before 8.15.10.2021 are buggy with Stage3D baseline "
        "mode",
        arraysize(kFeatureListForEntry71),  // features size
        kFeatureListForEntry71,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry71),       // CrBugs size
        kCrBugsForEntry71,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry71,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        72,  // id
        "NVIDIA GeForce 6200 LE is buggy with WebGL",
        arraysize(kFeatureListForEntry72),  // features size
        kFeatureListForEntry72,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry72),       // CrBugs size
        kCrBugsForEntry72,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            arraysize(kDeviceIDsForEntry72),        // DeviceIDs size
            kDeviceIDsForEntry72,                   // DeviceIDs
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
        74,  // id
        "GPU access is blocked if users don't have proper graphics driver "
        "installed after Windows installation",
        arraysize(kFeatureListForEntry74),  // features size
        kFeatureListForEntry74,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry74),       // CrBugs size
        kCrBugsForEntry74,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry74,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry74),  // exceptions count
        kExceptionsForEntry74,             // exceptions
    },
    {
        76,  // id
        "WebGL is disabled on Android unless the GPU runs in a separate "
        "process or reset notification is supported",
        arraysize(kFeatureListForEntry76),  // features size
        kFeatureListForEntry76,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
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
            &kMoreForEntry76,                       // more data
        },
        arraysize(kExceptionsForEntry76),  // exceptions count
        kExceptionsForEntry76,             // exceptions
    },
    {
        78,  // id
        "Accelerated video decode interferes with GPU sandbox on older Intel "
        "drivers",
        arraysize(kFeatureListForEntry78),  // features size
        kFeatureListForEntry78,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry78),       // CrBugs size
        kCrBugsForEntry78,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry78,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        79,  // id
        "Disable GPU on all Windows versions prior to and including Vista",
        arraysize(kFeatureListForEntry79),  // features size
        kFeatureListForEntry79,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry79),       // CrBugs size
        kCrBugsForEntry79,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical, "6.0",
             nullptr},                              // os_version
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
        82,  // id
        "MediaCodec is still too buggy to use for encoding (b/11536167)",
        arraysize(kFeatureListForEntry82),  // features size
        kFeatureListForEntry82,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry82),       // CrBugs size
        kCrBugsForEntry82,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
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
        86,  // id
        "Intel Graphics Media Accelerator 3150 causes the GPU process to hang "
        "running WebGL",
        arraysize(kFeatureListForEntry86),  // features size
        kFeatureListForEntry86,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry86),       // CrBugs size
        kCrBugsForEntry86,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry86),        // DeviceIDs size
            kDeviceIDsForEntry86,                   // DeviceIDs
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
        87,  // id
        "Accelerated video decode on Intel driver 10.18.10.3308 is "
        "incompatible with the GPU sandbox",
        arraysize(kFeatureListForEntry87),  // features size
        kFeatureListForEntry87,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry87),       // CrBugs size
        kCrBugsForEntry87,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry87,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        88,  // id
        "Accelerated video decode on AMD driver 13.152.1.8000 is incompatible "
        "with the GPU sandbox",
        arraysize(kFeatureListForEntry88),  // features size
        kFeatureListForEntry88,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry88),       // CrBugs size
        kCrBugsForEntry88,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry88,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        89,  // id
        "Accelerated video decode interferes with GPU sandbox on certain AMD "
        "drivers",
        arraysize(kFeatureListForEntry89),  // features size
        kFeatureListForEntry89,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry89),       // CrBugs size
        kCrBugsForEntry89,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry89,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        90,  // id
        "Accelerated video decode interferes with GPU sandbox on certain "
        "NVIDIA drivers",
        arraysize(kFeatureListForEntry90),  // features size
        kFeatureListForEntry90,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry90),       // CrBugs size
        kCrBugsForEntry90,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry90,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        91,  // id
        "Accelerated video decode interferes with GPU sandbox on certain "
        "NVIDIA drivers",
        arraysize(kFeatureListForEntry91),  // features size
        kFeatureListForEntry91,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry91),       // CrBugs size
        kCrBugsForEntry91,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry91,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        92,  // id
        "Accelerated video decode does not work with the discrete GPU on AMD "
        "switchables",
        arraysize(kFeatureListForEntry92),  // features size
        kFeatureListForEntry92,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry92),       // CrBugs size
        kCrBugsForEntry92,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::
                kMultiGpuStyleAMDSwitchableDiscrete,  // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        93,  // id
        "GLX indirect rendering (X remoting) is not supported",
        arraysize(kFeatureListForEntry93),  // features size
        kFeatureListForEntry93,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry93),       // CrBugs size
        kCrBugsForEntry93,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
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
            &kMoreForEntry93,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        94,  // id
        "Intel driver version 8.15.10.1749 causes GPU process hangs.",
        arraysize(kFeatureListForEntry94),  // features size
        kFeatureListForEntry94,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry94),       // CrBugs size
        kCrBugsForEntry94,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry94,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        95,  // id
        "AMD driver version 13.101 is unstable on linux.",
        arraysize(kFeatureListForEntry95),  // features size
        kFeatureListForEntry95,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry95),       // CrBugs size
        kCrBugsForEntry95,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry95,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        96,  // id
        "Blacklist GPU raster/canvas on all except known good GPUs and newer "
        "Android releases",
        arraysize(kFeatureListForEntry96),  // features size
        kFeatureListForEntry96,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry96),       // CrBugs size
        kCrBugsForEntry96,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
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
        arraysize(kExceptionsForEntry96),  // exceptions count
        kExceptionsForEntry96,             // exceptions
    },
    {
        100,  // id
        "GPU rasterization and canvas is blacklisted on Nexus 10",
        arraysize(kFeatureListForEntry100),  // features size
        kFeatureListForEntry100,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry100),       // CrBugs size
        kCrBugsForEntry100,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry100,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        102,  // id
        "Accelerated 2D canvas and Ganesh broken on Galaxy Tab 2",
        arraysize(kFeatureListForEntry102),  // features size
        kFeatureListForEntry102,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry102),       // CrBugs size
        kCrBugsForEntry102,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry102,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        104,  // id
        "GPU raster broken on PowerVR Rogue",
        arraysize(kFeatureListForEntry104),  // features size
        kFeatureListForEntry104,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry104),       // CrBugs size
        kCrBugsForEntry104,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry104,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        105,  // id
        "GPU raster broken on PowerVR SGX even on Lollipop",
        arraysize(kFeatureListForEntry105),  // features size
        kFeatureListForEntry105,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry105),       // CrBugs size
        kCrBugsForEntry105,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry105,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        106,  // id
        "GPU raster broken on ES2-only Adreno 3xx drivers",
        arraysize(kFeatureListForEntry106),  // features size
        kFeatureListForEntry106,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry106),       // CrBugs size
        kCrBugsForEntry106,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry106,                 // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry106,                      // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        107,  // id
        "Haswell GT1 Intel drivers are buggy on kernels < 3.19.1",
        arraysize(kFeatureListForEntry107),  // features size
        kFeatureListForEntry107,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry107),       // CrBugs size
        kCrBugsForEntry107,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "3.19.1", nullptr},                    // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry107),       // DeviceIDs size
            kDeviceIDsForEntry107,                  // DeviceIDs
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
        108,  // id
        "GPU rasterization image color broken on Vivante",
        arraysize(kFeatureListForEntry108),  // features size
        kFeatureListForEntry108,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry108),       // CrBugs size
        kCrBugsForEntry108,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry108,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        109,  // id
        "MediaCodec on Adreno 330 / 4.2.2 doesn't always send FORMAT_CHANGED",
        arraysize(kFeatureListForEntry109),  // features size
        kFeatureListForEntry109,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry109),       // CrBugs size
        kCrBugsForEntry109,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
             "4.2.2", nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry109,                // driver info
            &kGLStringsForEntry109,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        110,  // id
        "Only enable WebGL for the Mesa Gallium llvmpipe driver",
        arraysize(kFeatureListForEntry110),  // features size
        kFeatureListForEntry110,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry110),       // CrBugs size
        kCrBugsForEntry110,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry110,                // driver info
            &kGLStringsForEntry110,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        111,  // id
        "Apple Software Renderer used under VMWare experiences synchronization "
        "issues with GPU Raster",
        arraysize(kFeatureListForEntry111),  // features size
        kFeatureListForEntry111,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry111),       // CrBugs size
        kCrBugsForEntry111,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x15ad,                                // vendor_id
            0,                                     // DeviceIDs size
            nullptr,                               // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        112,  // id
        "Intel HD 3000 driver crashes frequently on Mac",
        arraysize(kFeatureListForEntry112),  // features size
        kFeatureListForEntry112,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry112),       // CrBugs size
        kCrBugsForEntry112,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x8086,                                // vendor_id
            arraysize(kDeviceIDsForEntry112),      // DeviceIDs size
            kDeviceIDsForEntry112,                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        113,  // id
        "Some GPUs on Mac can perform poorly with GPU rasterization. Disable "
        "all known Intel GPUs other than Intel 6th and 7th Generation cards, "
        "which have been tested.",
        arraysize(kFeatureListForEntry113),  // features size
        kFeatureListForEntry113,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry113),       // CrBugs size
        kCrBugsForEntry113,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x8086,                                // vendor_id
            arraysize(kDeviceIDsForEntry113),      // DeviceIDs size
            kDeviceIDsForEntry113,                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        114,  // id
        "Some GPUs on Mac can perform poorly with GPU rasterization. Disable "
        "all known NVidia GPUs other than the Geforce 6xx and 7xx series, "
        "which have been tested.",
        arraysize(kFeatureListForEntry114),  // features size
        kFeatureListForEntry114,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry114),       // CrBugs size
        kCrBugsForEntry114,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x10de,                                // vendor_id
            arraysize(kDeviceIDsForEntry114),      // DeviceIDs size
            kDeviceIDsForEntry114,                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        115,  // id
        "Some GPUs on Mac can perform poorly with GPU rasterization. Disable "
        "all known AMD GPUs other than the R200, R300, and D series, which "
        "have been tested.",
        arraysize(kFeatureListForEntry115),  // features size
        kFeatureListForEntry115,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry115),       // CrBugs size
        kCrBugsForEntry115,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x1002,                                // vendor_id
            arraysize(kDeviceIDsForEntry115),      // DeviceIDs size
            kDeviceIDsForEntry115,                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        116,  // id
        "Some GPUs on Mac can perform poorly with GPU rasterization. Disable "
        "untested Virtualbox GPU.",
        arraysize(kFeatureListForEntry116),  // features size
        kFeatureListForEntry116,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry116),       // CrBugs size
        kCrBugsForEntry116,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x80ee,                                // vendor_id
            0,                                     // DeviceIDs size
            nullptr,                               // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        117,  // id
        "MediaCodec on Vivante hangs in MediaCodec often",
        arraysize(kFeatureListForEntry117),  // features size
        kFeatureListForEntry117,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry117),       // CrBugs size
        kCrBugsForEntry117,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
             "4.4.4", nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry117,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        118,  // id
        "webgl/canvas crashy on imporperly parsed vivante driver",
        arraysize(kFeatureListForEntry118),  // features size
        kFeatureListForEntry118,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry118),       // CrBugs size
        kCrBugsForEntry118,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
             "4.4.4", nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry118,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        119,  // id
        "There are display issues with GPU Raster on OSX 10.9",
        arraysize(kFeatureListForEntry119),  // features size
        kFeatureListForEntry119,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry119),       // CrBugs size
        kCrBugsForEntry119,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
             "10.9", nullptr},                      // os_version
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
        120,  // id
        "VPx decoding isn't supported before Windows 10 anniversary update.",
        arraysize(kFeatureListForEntry120),  // features size
        kFeatureListForEntry120,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry120),       // CrBugs size
        kCrBugsForEntry120,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "10.0.14393", nullptr},                // os_version
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
        121,  // id
        "VPx decoding is too slow on Intel Broadwell, Skylake, and CherryView",
        arraysize(kFeatureListForEntry121),  // features size
        kFeatureListForEntry121,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry121),       // CrBugs size
        kCrBugsForEntry121,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry121),       // DeviceIDs size
            kDeviceIDsForEntry121,                  // DeviceIDs
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
        122,  // id
        "GPU rasterization should only be enabled on NVIDIA and Intel DX11+, "
        "and AMD RX-R2 GPUs for now.",
        arraysize(kFeatureListForEntry122),  // features size
        kFeatureListForEntry122,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry122),       // CrBugs size
        kCrBugsForEntry122,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
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
        arraysize(kExceptionsForEntry122),  // exceptions count
        kExceptionsForEntry122,             // exceptions
    },
    {
        123,  // id
        "Accelerated VPx decoding is hanging on some videos.",
        arraysize(kFeatureListForEntry123),  // features size
        kFeatureListForEntry123,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry123),       // CrBugs size
        kCrBugsForEntry123,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry123,                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        124,  // id
        "Some AMD drivers have rendering glitches with GPU Rasterization",
        arraysize(kFeatureListForEntry124),  // features size
        kFeatureListForEntry124,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry124),       // CrBugs size
        kCrBugsForEntry124,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry124,                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry124),  // exceptions count
        kExceptionsForEntry124,             // exceptions
    },
    {
        125,  // id
        "VirtualBox driver is unstable on linux.",
        arraysize(kFeatureListForEntry125),  // features size
        kFeatureListForEntry125,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry125),       // CrBugs size
        kCrBugsForEntry125,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x80ee,                                 // vendor_id
            arraysize(kDeviceIDsForEntry125),       // DeviceIDs size
            kDeviceIDsForEntry125,                  // DeviceIDs
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
        126,  // id
        "Don't allow ES3 on Mac core profile < 4.1",
        arraysize(kFeatureListForEntry126),  // features size
        kFeatureListForEntry126,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry126),       // CrBugs size
        kCrBugsForEntry126,                  // CrBugs
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
            &kMoreForEntry126,                      // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        129,  // id
        "Intel drivers are buggy on Linux 2.x",
        arraysize(kFeatureListForEntry129),  // features size
        kFeatureListForEntry129,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry129),       // CrBugs size
        kCrBugsForEntry129,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "3.0",
             nullptr},                              // os_version
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        130,  // id
        "Older NVIDIA GPUs on macOS render incorrectly",
        arraysize(kFeatureListForEntry130),  // features size
        kFeatureListForEntry130,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry130),       // CrBugs size
        kCrBugsForEntry130,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x10de,                                // vendor_id
            arraysize(kDeviceIDsForEntry130),      // DeviceIDs size
            kDeviceIDsForEntry130,                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        131,  // id
        "Mesa drivers older than 10.4.3 is crash prone on Linux Intel i965gm",
        arraysize(kFeatureListForEntry131),  // features size
        kFeatureListForEntry131,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry131),       // CrBugs size
        kCrBugsForEntry131,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1f96,                                 // vendor_id
            arraysize(kDeviceIDsForEntry131),       // DeviceIDs size
            kDeviceIDsForEntry131,                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry131,                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        132,  // id
        "Mali accelerated 2d canvas is slow on Linux",
        arraysize(kFeatureListForEntry132),  // features size
        kFeatureListForEntry132,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry132),       // CrBugs size
        kCrBugsForEntry132,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry132,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        133,  // id
        "MediaCodec on VideoCore IV HW crashes on JB",
        arraysize(kFeatureListForEntry133),  // features size
        kFeatureListForEntry133,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry133),       // CrBugs size
        kCrBugsForEntry133,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.4",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry133,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        134,  // id
        "Mesa driver 10.1.3 renders incorrectly and crashes on multiple "
        "vendors",
        arraysize(kFeatureListForEntry134),  // features size
        kFeatureListForEntry134,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry134),       // CrBugs size
        kCrBugsForEntry134,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry134,                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry134),  // exceptions count
        kExceptionsForEntry134,             // exceptions
    },
    {
        136,  // id
        "GPU rasterization is blacklisted on NVidia Fermi architecture for "
        "now.",
        arraysize(kFeatureListForEntry136),  // features size
        kFeatureListForEntry136,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry136),       // CrBugs size
        kCrBugsForEntry136,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            arraysize(kDeviceIDsForEntry136),       // DeviceIDs size
            kDeviceIDsForEntry136,                  // DeviceIDs
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
        137,  // id
        "GPU rasterization on CrOS is blacklisted on non-Intel GPUs for now.",
        arraysize(kFeatureListForEntry137),  // features size
        kFeatureListForEntry137,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry137),       // CrBugs size
        kCrBugsForEntry137,                  // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
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
        arraysize(kExceptionsForEntry137),  // exceptions count
        kExceptionsForEntry137,             // exceptions
    },
    {
        138,  // id
        "Accelerated video encode is unavailable on Linux",
        arraysize(kFeatureListForEntry138),  // features size
        kFeatureListForEntry138,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        0,                                   // CrBugs size
        nullptr,                             // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
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
        139,  // id
        "GPU Rasterization is disabled on pre-GCN AMD cards",
        arraysize(kFeatureListForEntry139),  // features size
        kFeatureListForEntry139,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry139),       // CrBugs size
        kCrBugsForEntry139,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry139,                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        140,  // id
        "MSAA and depth texture buggy on Adreno 3xx, also disable WebGL2",
        arraysize(kFeatureListForEntry140),  // features size
        kFeatureListForEntry140,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry140),       // CrBugs size
        kCrBugsForEntry140,                  // CrBugs
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
            &kGLStringsForEntry140,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
};
}  // namespace gpu
