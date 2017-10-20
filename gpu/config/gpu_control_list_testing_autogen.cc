// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/config/gpu_control_list_testing_autogen.h"

#include "gpu/config/gpu_control_list_testing_arrays_and_structs_autogen.h"
#include "gpu/config/gpu_control_list_testing_exceptions_autogen.h"

namespace gpu {

const GpuControlList::Entry kGpuControlListTestingEntries[] = {
    {
        1,  // id
        "GpuControlListEntryTest.DetailedEntry",
        arraysize(kFeatureListForEntry1),         // features size
        kFeatureListForEntry1,                    // features
        arraysize(kDisabledExtensionsForEntry1),  // DisabledExtensions size
        kDisabledExtensionsForEntry1,             // DisabledExtensions
        arraysize(kCrBugsForEntry1),              // CrBugs size
        kCrBugsForEntry1,                         // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
             "10.6.4", nullptr},                    // os_version
            0x10de,                                 // vendor_id
            arraysize(kDeviceIDsForEntry1),         // DeviceIDs size
            kDeviceIDsForEntry1,                    // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry1,                  // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        2,  // id
        "GpuControlListEntryTest.VendorOnAllOsEntry",
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
            0x10de,                                 // vendor_id
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
        3,  // id
        "GpuControlListEntryTest.VendorOnLinuxEntry",
        arraysize(kFeatureListForEntry3),  // features size
        kFeatureListForEntry3,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
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
        "GpuControlListEntryTest.AllExceptNVidiaOnLinuxEntry",
        arraysize(kFeatureListForEntry4),  // features size
        kFeatureListForEntry4,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
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
        arraysize(kExceptionsForEntry4),  // exceptions count
        kExceptionsForEntry4,             // exceptions
    },
    {
        5,  // id
        "GpuControlListEntryTest.AllExceptIntelOnLinuxEntry",
        arraysize(kFeatureListForEntry5),  // features size
        kFeatureListForEntry5,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
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
        arraysize(kExceptionsForEntry5),  // exceptions count
        kExceptionsForEntry5,             // exceptions
    },
    {
        6,  // id
        "GpuControlListEntryTest.DateOnWindowsEntry",
        arraysize(kFeatureListForEntry6),  // features size
        kFeatureListForEntry6,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry6,                  // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        7,  // id
        "GpuControlListEntryTest.MultipleDevicesEntry",
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
            0x10de,                                 // vendor_id
            arraysize(kDeviceIDsForEntry7),         // DeviceIDs size
            kDeviceIDsForEntry7,                    // DeviceIDs
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
        8,  // id
        "GpuControlListEntryTest.ChromeOSEntry",
        arraysize(kFeatureListForEntry8),  // features size
        kFeatureListForEntry8,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        9,  // id
        "GpuControlListEntryTest.GlVersionGLESEntry",
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
            &kMoreForEntry9,                        // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        10,  // id
        "GpuControlListEntryTest.GlVersionANGLEEntry",
        arraysize(kFeatureListForEntry10),  // features size
        kFeatureListForEntry10,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kMoreForEntry10,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        11,  // id
        "GpuControlListEntryTest.GlVersionGLEntry",
        arraysize(kFeatureListForEntry11),  // features size
        kFeatureListForEntry11,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kMoreForEntry11,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        12,  // id
        "GpuControlListEntryTest.GlVendorEqual",
        arraysize(kFeatureListForEntry12),  // features size
        kFeatureListForEntry12,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kGLStringsForEntry12,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        13,  // id
        "GpuControlListEntryTest.GlVendorWithDot",
        arraysize(kFeatureListForEntry13),  // features size
        kFeatureListForEntry13,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kGLStringsForEntry13,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        14,  // id
        "GpuControlListEntryTest.GlRendererContains",
        arraysize(kFeatureListForEntry14),  // features size
        kFeatureListForEntry14,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kGLStringsForEntry14,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        15,  // id
        "GpuControlListEntryTest.GlRendererCaseInsensitive",
        arraysize(kFeatureListForEntry15),  // features size
        kFeatureListForEntry15,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kGLStringsForEntry15,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        16,  // id
        "GpuControlListEntryTest.GlExtensionsEndWith",
        arraysize(kFeatureListForEntry16),  // features size
        kFeatureListForEntry16,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kGLStringsForEntry16,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        17,  // id
        "GpuControlListEntryTest.OptimusEntry",
        arraysize(kFeatureListForEntry17),  // features size
        kFeatureListForEntry17,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleOptimus,  // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        18,  // id
        "GpuControlListEntryTest.AMDSwitchableEntry",
        arraysize(kFeatureListForEntry18),  // features size
        kFeatureListForEntry18,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                           // os_version
            0x00,                                         // vendor_id
            0,                                            // DeviceIDs size
            nullptr,                                      // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,        // multi_gpu_category
            GpuControlList::kMultiGpuStyleAMDSwitchable,  // multi_gpu_style
            nullptr,                                      // driver info
            nullptr,                                      // GL strings
            nullptr,                                      // machine model info
            nullptr,                                      // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        19,  // id
        "GpuControlListEntryTest.DriverVendorBeginWith",
        arraysize(kFeatureListForEntry19),  // features size
        kFeatureListForEntry19,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry19,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        20,  // id
        "GpuControlListEntryTest.LexicalDriverVersionEntry",
        arraysize(kFeatureListForEntry20),  // features size
        kFeatureListForEntry20,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry20,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        21,  // id
        "GpuControlListEntryTest.NeedsMoreInfoEntry",
        arraysize(kFeatureListForEntry21),  // features size
        kFeatureListForEntry21,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry21,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        22,  // id
        "GpuControlListEntryTest.NeedsMoreInfoForExceptionsEntry",
        arraysize(kFeatureListForEntry22),  // features size
        kFeatureListForEntry22,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
        arraysize(kExceptionsForEntry22),  // exceptions count
        kExceptionsForEntry22,             // exceptions
    },
    {
        23,  // id
        "GpuControlListEntryTest.NeedsMoreInfoForGlVersionEntry",
        arraysize(kFeatureListForEntry23),  // features size
        kFeatureListForEntry23,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kMoreForEntry23,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        24,  // id
        "GpuControlListEntryTest.FeatureTypeAllEntry",
        arraysize(kFeatureListForEntry24),  // features size
        kFeatureListForEntry24,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
        25,  // id
        "GpuControlListEntryTest.FeatureTypeAllEntryWithExceptions",
        arraysize(kFeatureListForEntry25),  // features size
        kFeatureListForEntry25,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
        26,  // id
        "GpuControlListEntryTest.SingleActiveGPU",
        arraysize(kFeatureListForEntry26),  // features size
        kFeatureListForEntry26,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x10de,                                   // vendor_id
            arraysize(kDeviceIDsForEntry26),          // DeviceIDs size
            kDeviceIDsForEntry26,                     // DeviceIDs
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
    {
        27,  // id
        "GpuControlListEntryTest.MachineModelName",
        arraysize(kFeatureListForEntry27),  // features size
        kFeatureListForEntry27,             // features
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
            &kMachineModelInfoForEntry27,           // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        28,  // id
        "GpuControlListEntryTest.MachineModelNameException",
        arraysize(kFeatureListForEntry28),  // features size
        kFeatureListForEntry28,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
        arraysize(kExceptionsForEntry28),  // exceptions count
        kExceptionsForEntry28,             // exceptions
    },
    {
        29,  // id
        "GpuControlListEntryTest.MachineModelVersion",
        arraysize(kFeatureListForEntry29),  // features size
        kFeatureListForEntry29,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kMachineModelInfoForEntry29,           // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        30,  // id
        "GpuControlListEntryTest.MachineModelVersionException",
        arraysize(kFeatureListForEntry30),  // features size
        kFeatureListForEntry30,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kMachineModelInfoForEntry30,           // machine model info
            nullptr,                                // more conditions
        },
        arraysize(kExceptionsForEntry30),  // exceptions count
        kExceptionsForEntry30,             // exceptions
    },
    {
        31,  // id
        "GpuControlListEntryDualGPUTest.CategoryAny.Intel",
        arraysize(kFeatureListForEntry31),  // features size
        kFeatureListForEntry31,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x8086,                                // vendor_id
            arraysize(kDeviceIDsForEntry31),       // DeviceIDs size
            kDeviceIDsForEntry31,                  // DeviceIDs
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
        32,  // id
        "GpuControlListEntryDualGPUTest.CategoryAny.NVidia",
        arraysize(kFeatureListForEntry32),  // features size
        kFeatureListForEntry32,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x10de,                                // vendor_id
            arraysize(kDeviceIDsForEntry32),       // DeviceIDs size
            kDeviceIDsForEntry32,                  // DeviceIDs
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
        33,  // id
        "GpuControlListEntryDualGPUTest.CategorySecondary",
        arraysize(kFeatureListForEntry33),  // features size
        kFeatureListForEntry33,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                          // os_version
            0x8086,                                      // vendor_id
            arraysize(kDeviceIDsForEntry33),             // DeviceIDs size
            kDeviceIDsForEntry33,                        // DeviceIDs
            GpuControlList::kMultiGpuCategorySecondary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        34,  // id
        "GpuControlListEntryDualGPUTest.CategoryPrimary",
        arraysize(kFeatureListForEntry34),  // features size
        kFeatureListForEntry34,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                        // os_version
            0x8086,                                    // vendor_id
            arraysize(kDeviceIDsForEntry34),           // DeviceIDs size
            kDeviceIDsForEntry34,                      // DeviceIDs
            GpuControlList::kMultiGpuCategoryPrimary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,        // multi_gpu_style
            nullptr,                                   // driver info
            nullptr,                                   // GL strings
            nullptr,                                   // machine model info
            nullptr,                                   // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        35,  // id
        "GpuControlListEntryDualGPUTest.CategoryDefault",
        arraysize(kFeatureListForEntry35),  // features size
        kFeatureListForEntry35,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry35),        // DeviceIDs size
            kDeviceIDsForEntry35,                   // DeviceIDs
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
        36,  // id
        "GpuControlListEntryDualGPUTest.ActiveSecondaryGPU",
        arraysize(kFeatureListForEntry36),  // features size
        kFeatureListForEntry36,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x8086,                                   // vendor_id
            arraysize(kDeviceIDsForEntry36),          // DeviceIDs size
            kDeviceIDsForEntry36,                     // DeviceIDs
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
    {
        37,  // id
        "GpuControlListEntryDualGPUTest.VendorOnlyActiveSecondaryGPU",
        arraysize(kFeatureListForEntry37),  // features size
        kFeatureListForEntry37,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
    {
        38,  // id
        "GpuControlListEntryDualGPUTest.ActivePrimaryGPU",
        arraysize(kFeatureListForEntry38),  // features size
        kFeatureListForEntry38,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x10de,                                   // vendor_id
            arraysize(kDeviceIDsForEntry38),          // DeviceIDs size
            kDeviceIDsForEntry38,                     // DeviceIDs
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
    {
        39,  // id
        "GpuControlListEntryDualGPUTest.VendorOnlyActivePrimaryGPU",
        arraysize(kFeatureListForEntry39),  // features size
        kFeatureListForEntry39,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x10de,                                   // vendor_id
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
    {
        40,  // id
        "GpuControlListEntryTest.PixelShaderVersion",
        arraysize(kFeatureListForEntry40),  // features size
        kFeatureListForEntry40,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kMoreForEntry40,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        41,  // id
        "GpuControlListEntryTest.OsVersionZeroLT",
        arraysize(kFeatureListForEntry41),  // features size
        kFeatureListForEntry41,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.2",
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
        42,  // id
        "GpuControlListEntryTest.OsVersionZeroAny",
        arraysize(kFeatureListForEntry42),  // features size
        kFeatureListForEntry42,             // features
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
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        43,  // id
        "GpuControlListEntryTest.OsComparisonAny",
        arraysize(kFeatureListForEntry43),  // features size
        kFeatureListForEntry43,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
        44,  // id
        "GpuControlListEntryTest.OsComparisonGE",
        arraysize(kFeatureListForEntry44),  // features size
        kFeatureListForEntry44,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "6",
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
        45,  // id
        "GpuControlListEntryTest.ExceptionWithoutVendorId",
        arraysize(kFeatureListForEntry45),  // features size
        kFeatureListForEntry45,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
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
        arraysize(kExceptionsForEntry45),  // exceptions count
        kExceptionsForEntry45,             // exceptions
    },
    {
        46,  // id
        "GpuControlListEntryTest.MultiGpuStyleAMDSwitchableDiscrete",
        arraysize(kFeatureListForEntry46),  // features size
        kFeatureListForEntry46,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
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
        47,  // id
        "GpuControlListEntryTest.MultiGpuStyleAMDSwitchableIntegrated",
        arraysize(kFeatureListForEntry47),  // features size
        kFeatureListForEntry47,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::
                kMultiGpuStyleAMDSwitchableIntegrated,  // multi_gpu_style
            nullptr,                                    // driver info
            nullptr,                                    // GL strings
            nullptr,                                    // machine model info
            nullptr,                                    // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        48,  // id
        "GpuControlListEntryTest.InProcessGPU",
        arraysize(kFeatureListForEntry48),  // features size
        kFeatureListForEntry48,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kMoreForEntry48,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        49,  // id
        "GpuControlListEntryTest.SameGPUTwiceTest",
        arraysize(kFeatureListForEntry49),  // features size
        kFeatureListForEntry49,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        50,  // id
        "GpuControlListEntryTest.NVidiaNumberingScheme",
        arraysize(kFeatureListForEntry50),  // features size
        kFeatureListForEntry50,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry50,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        51,  // id
        "GpuControlListTest.NeedsMoreInfo",
        arraysize(kFeatureListForEntry51),  // features size
        kFeatureListForEntry51,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry51,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        52,  // id
        "GpuControlListTest.NeedsMoreInfoForExceptions",
        arraysize(kFeatureListForEntry52),  // features size
        kFeatureListForEntry52,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
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
        arraysize(kExceptionsForEntry52),  // exceptions count
        kExceptionsForEntry52,             // exceptions
    },
    {
        53,  // id
        "GpuControlListTest.IgnorableEntries.0",
        arraysize(kFeatureListForEntry53),  // features size
        kFeatureListForEntry53,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        54,  // id
        "GpuControlListTest.IgnorableEntries.1",
        arraysize(kFeatureListForEntry54),  // features size
        kFeatureListForEntry54,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry54,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        55,  // id
        "GpuControlListTest.DisabledExtensionTest.0",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry55),  // DisabledExtensions size
        kDisabledExtensionsForEntry55,             // DisabledExtensions
        0,                                         // CrBugs size
        nullptr,                                   // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        56,  // id
        "GpuControlListTest.DisabledExtensionTest.1",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry56),  // DisabledExtensions size
        kDisabledExtensionsForEntry56,             // DisabledExtensions
        0,                                         // CrBugs size
        nullptr,                                   // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        57,  // id
        "GpuControlListEntryTest.DirectRendering",
        arraysize(kFeatureListForEntry57),  // features size
        kFeatureListForEntry57,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
            &kMoreForEntry57,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        58,  // id
        "GpuControlListTest.LinuxKernelVersion",
        arraysize(kFeatureListForEntry58),  // features size
        kFeatureListForEntry58,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "3.19.1", nullptr},                    // os_version
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
};
const size_t kGpuControlListTestingEntryCount = 58;
}  // namespace gpu
