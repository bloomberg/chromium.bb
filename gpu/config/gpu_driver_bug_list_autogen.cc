// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/config/gpu_driver_bug_list_autogen.h"

#include "gpu/config/gpu_driver_bug_list_arrays_and_structs_autogen.h"
#include "gpu/config/gpu_driver_bug_list_exceptions_autogen.h"

namespace gpu {

const char kGpuDriverBugListVersion[] = "10.1";

const size_t kGpuDriverBugListEntryCount = 172;
const GpuControlList::Entry kGpuDriverBugListEntries[172] = {
    {
        1,  // id
        "Imagination driver doesn't like uploading lots of buffer data "
        "constantly",
        arraysize(kFeatureListForEntry1),  // features size
        kFeatureListForEntry1,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
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
            &kGLStringsForEntry1,                   // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        2,  // id
        "ARM driver doesn't like uploading lots of buffer data constantly",
        arraysize(kFeatureListForEntry2),  // features size
        kFeatureListForEntry2,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        0,                                 // CrBugs size
        nullptr,                           // CrBugs
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
            &kGLStringsForEntry2,                   // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        6,  // id
        "Restore scissor on FBO change with Qualcomm GPUs on older versions of "
        "Android",
        arraysize(kFeatureListForEntry6),  // features size
        kFeatureListForEntry6,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        arraysize(kCrBugsForEntry6),       // CrBugs size
        kCrBugsForEntry6,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.3",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry6,                   // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        7,  // id
        "Work around a bug in offscreen buffers on NVIDIA GPUs on Macs",
        arraysize(kFeatureListForEntry7),  // features size
        kFeatureListForEntry7,             // features
        0,                                 // DisabledExtensions size
        nullptr,                           // DisabledExtensions
        arraysize(kCrBugsForEntry7),       // CrBugs size
        kCrBugsForEntry7,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
        17,  // id
        "Some drivers are unable to reset the D3D device in the GPU process "
        "sandbox",
        arraysize(kFeatureListForEntry17),  // features size
        kFeatureListForEntry17,             // features
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
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        19,  // id
        "Disable depth textures on older Qualcomm GPUs (legacy blacklist "
        "entry, original problem unclear)",
        arraysize(kFeatureListForEntry19),         // features size
        kFeatureListForEntry19,                    // features
        arraysize(kDisabledExtensionsForEntry19),  // DisabledExtensions size
        kDisabledExtensionsForEntry19,             // DisabledExtensions
        arraysize(kCrBugsForEntry19),              // CrBugs size
        kCrBugsForEntry19,                         // CrBugs
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
            &kGLStringsForEntry19,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        20,  // id
        "Disable EXT_draw_buffers on GeForce GT 650M on Mac OS X due to driver "
        "bugs",
        arraysize(kFeatureListForEntry20),  // features size
        kFeatureListForEntry20,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x10de,                                // vendor_id
            arraysize(kDeviceIDsForEntry20),       // DeviceIDs size
            kDeviceIDsForEntry20,                  // DeviceIDs
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
        21,  // id
        "Vivante GPUs are buggy with context switching",
        arraysize(kFeatureListForEntry21),  // features size
        kFeatureListForEntry21,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry21),       // CrBugs size
        kCrBugsForEntry21,                  // CrBugs
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
            &kGLStringsForEntry21,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        22,  // id
        "Imagination drivers are buggy with context switching",
        arraysize(kFeatureListForEntry22),  // features size
        kFeatureListForEntry22,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry22),       // CrBugs size
        kCrBugsForEntry22,                  // CrBugs
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
            &kGLStringsForEntry22,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        23,  // id
        "Disable OES_standard_derivative on Intel Pineview M Gallium drivers",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry23),  // DisabledExtensions size
        kDisabledExtensionsForEntry23,             // DisabledExtensions
        arraysize(kCrBugsForEntry23),              // CrBugs size
        kCrBugsForEntry23,                         // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry23),        // DeviceIDs size
            kDeviceIDsForEntry23,                   // DeviceIDs
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
        24,  // id
        "Mali-4xx drivers throw an error when a buffer object's size is set to "
        "0",
        arraysize(kFeatureListForEntry24),  // features size
        kFeatureListForEntry24,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry24),       // CrBugs size
        kCrBugsForEntry24,                  // CrBugs
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
            &kGLStringsForEntry24,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        26,  // id
        "Disable use of Direct3D 11 on Windows Vista and lower",
        arraysize(kFeatureListForEntry26),  // features size
        kFeatureListForEntry26,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        0,                                  // CrBugs size
        nullptr,                            // CrBugs
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
        27,  // id
        "Async Readpixels with GL_BGRA format is broken on Haswell chipset on "
        "Macs",
        arraysize(kFeatureListForEntry27),  // features size
        kFeatureListForEntry27,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry27),       // CrBugs size
        kCrBugsForEntry27,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry27),        // DeviceIDs size
            kDeviceIDsForEntry27,                   // DeviceIDs
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
        30,  // id
        "Multisampling is buggy on OSX when multiple monitors are connected",
        arraysize(kFeatureListForEntry30),  // features size
        kFeatureListForEntry30,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry30),       // CrBugs size
        kCrBugsForEntry30,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        31,  // id
        "The Mali-Txxx driver does not guarantee flush ordering",
        arraysize(kFeatureListForEntry31),  // features size
        kFeatureListForEntry31,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry31),       // CrBugs size
        kCrBugsForEntry31,                  // CrBugs
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
            &kGLStringsForEntry31,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        32,  // id
        "Share groups are not working on (older?) Broadcom drivers",
        arraysize(kFeatureListForEntry32),  // features size
        kFeatureListForEntry32,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry32),       // CrBugs size
        kCrBugsForEntry32,                  // CrBugs
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
            &kGLStringsForEntry32,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        33,  // id
        "Share group-related crashes and poor context switching perf on "
        "Imagination drivers",
        arraysize(kFeatureListForEntry33),  // features size
        kFeatureListForEntry33,             // features
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
            &kGLStringsForEntry33,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        34,  // id
        "Share groups are not working on (older?) Vivante drivers",
        arraysize(kFeatureListForEntry34),  // features size
        kFeatureListForEntry34,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry34),       // CrBugs size
        kCrBugsForEntry34,                  // CrBugs
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
            &kGLStringsForEntry34,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        35,  // id
        "Share-group related crashes on older NVIDIA drivers",
        arraysize(kFeatureListForEntry35),  // features size
        kFeatureListForEntry35,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry35),       // CrBugs size
        kCrBugsForEntry35,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.3",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry35,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        36,  // id
        "Share-group related crashes on Qualcomm drivers",
        arraysize(kFeatureListForEntry36),  // features size
        kFeatureListForEntry36,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry36),       // CrBugs size
        kCrBugsForEntry36,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.3",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry36,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        37,  // id
        "Program link fails in NVIDIA Linux if gl_Position is not set",
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
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry37,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        38,  // id
        "Non-virtual contexts on Qualcomm sometimes cause out-of-order frames",
        arraysize(kFeatureListForEntry38),  // features size
        kFeatureListForEntry38,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry38),       // CrBugs size
        kCrBugsForEntry38,                  // CrBugs
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
            &kGLStringsForEntry38,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        39,  // id
        "Multisampled renderbuffer allocation must be validated on some Macs",
        arraysize(kFeatureListForEntry39),  // features size
        kFeatureListForEntry39,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry39),       // CrBugs size
        kCrBugsForEntry39,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "10.10", nullptr},                     // os_version
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
        40,  // id
        "Framebuffer discarding causes flickering on old ARM drivers",
        arraysize(kFeatureListForEntry40),  // features size
        kFeatureListForEntry40,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry40),       // CrBugs size
        kCrBugsForEntry40,                  // CrBugs
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
            &kGLStringsForEntry40,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        42,  // id
        "Framebuffer discarding causes flickering on older IMG drivers",
        arraysize(kFeatureListForEntry42),  // features size
        kFeatureListForEntry42,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry42),       // CrBugs size
        kCrBugsForEntry42,                  // CrBugs
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
            &kGLStringsForEntry42,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        43,  // id
        "Framebuffer discarding doesn't accept trivial attachments on Vivante",
        arraysize(kFeatureListForEntry43),  // features size
        kFeatureListForEntry43,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry43),       // CrBugs size
        kCrBugsForEntry43,                  // CrBugs
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
            &kGLStringsForEntry43,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        44,  // id
        "Framebuffer discarding causes jumpy scrolling on Mali drivers",
        arraysize(kFeatureListForEntry44),  // features size
        kFeatureListForEntry44,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry44),       // CrBugs size
        kCrBugsForEntry44,                  // CrBugs
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
        45,  // id
        "Unfold short circuit on Mac OS X",
        arraysize(kFeatureListForEntry45),  // features size
        kFeatureListForEntry45,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry45),       // CrBugs size
        kCrBugsForEntry45,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        48,  // id
        "Force to use discrete GPU on older MacBookPro models",
        arraysize(kFeatureListForEntry48),  // features size
        kFeatureListForEntry48,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry48),       // CrBugs size
        kCrBugsForEntry48,                  // CrBugs
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
            &kMachineModelInfoForEntry48,           // machine model info
            &kMoreForEntry48,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        49,  // id
        "The first draw operation from an idle state is slow",
        arraysize(kFeatureListForEntry49),  // features size
        kFeatureListForEntry49,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry49),       // CrBugs size
        kCrBugsForEntry49,                  // CrBugs
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
            &kGLStringsForEntry49,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        51,  // id
        "TexSubImage is faster for full uploads on ANGLE",
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
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry51,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        52,  // id
        "ES3 MSAA was observed problematic on some Adreno 4xx (see "
        "crbug.com/471200#c9)",
        arraysize(kFeatureListForEntry52),  // features size
        kFeatureListForEntry52,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry52),       // CrBugs size
        kCrBugsForEntry52,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "6.0",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry52,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        54,  // id
        "Clear uniforms before first program use on all platforms",
        arraysize(kFeatureListForEntry54),  // features size
        kFeatureListForEntry54,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry54),       // CrBugs size
        kCrBugsForEntry54,                  // CrBugs
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
        arraysize(kExceptionsForEntry54),  // exceptions count
        kExceptionsForEntry54,             // exceptions
    },
    {
        55,  // id
        "Mesa drivers in Linux handle varyings without static use incorrectly",
        arraysize(kFeatureListForEntry55),  // features size
        kFeatureListForEntry55,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry55),       // CrBugs size
        kCrBugsForEntry55,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry55,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        56,  // id
        "Mesa drivers in ChromeOS handle varyings without static use "
        "incorrectly",
        arraysize(kFeatureListForEntry56),  // features size
        kFeatureListForEntry56,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry56),       // CrBugs size
        kCrBugsForEntry56,                  // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
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
        59,  // id
        "Multisampling is buggy in Intel IvyBridge",
        arraysize(kFeatureListForEntry59),  // features size
        kFeatureListForEntry59,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry59),       // CrBugs size
        kCrBugsForEntry59,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry59),        // DeviceIDs size
            kDeviceIDsForEntry59,                   // DeviceIDs
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
        64,  // id
        "Linux AMD drivers incorrectly return initial value of 1 for "
        "TEXTURE_MAX_ANISOTROPY",
        arraysize(kFeatureListForEntry64),  // features size
        kFeatureListForEntry64,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry64),       // CrBugs size
        kCrBugsForEntry64,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        65,  // id
        "Linux NVIDIA drivers don't have the correct defaults for vertex "
        "attributes",
        arraysize(kFeatureListForEntry65),  // features size
        kFeatureListForEntry65,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry65),       // CrBugs size
        kCrBugsForEntry65,                  // CrBugs
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
            &kGLStringsForEntry65,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        68,  // id
        "Disable partial swaps on Mesa drivers (detected with GL_RENDERER)",
        arraysize(kFeatureListForEntry68),  // features size
        kFeatureListForEntry68,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry68),       // CrBugs size
        kCrBugsForEntry68,                  // CrBugs
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
            &kGLStringsForEntry68,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        69,  // id
        "Some shaders in Skia need more than the min available vertex and "
        "fragment shader uniform vectors in case of OSMesa",
        arraysize(kFeatureListForEntry69),  // features size
        kFeatureListForEntry69,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry69),       // CrBugs size
        kCrBugsForEntry69,                  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
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
        "Disable D3D11 on older nVidia drivers",
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
        "Vivante's support of OES_standard_derivatives is buggy",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry71),  // DisabledExtensions size
        kDisabledExtensionsForEntry71,             // DisabledExtensions
        arraysize(kCrBugsForEntry71),              // CrBugs size
        kCrBugsForEntry71,                         // CrBugs
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
            &kGLStringsForEntry71,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        72,  // id
        "Use virtual contexts on NVIDIA with GLES 3.1",
        arraysize(kFeatureListForEntry72),  // features size
        kFeatureListForEntry72,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry72),       // CrBugs size
        kCrBugsForEntry72,                  // CrBugs
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
            &kGLStringsForEntry72,                  // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry72,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        74,  // id
        "Testing EGL sync fences was broken on most Qualcomm drivers",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry74),  // DisabledExtensions size
        kDisabledExtensionsForEntry74,             // DisabledExtensions
        arraysize(kCrBugsForEntry74),              // CrBugs size
        kCrBugsForEntry74,                         // CrBugs
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
            &kGLStringsForEntry74,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        75,  // id
        "Mali-4xx support of EXT_multisampled_render_to_texture is buggy on "
        "Android < 4.3",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry75),  // DisabledExtensions size
        kDisabledExtensionsForEntry75,             // DisabledExtensions
        arraysize(kCrBugsForEntry75),              // CrBugs size
        kCrBugsForEntry75,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.3",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry75,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        76,  // id
        "Testing EGL sync fences was broken on IMG",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry76),  // DisabledExtensions size
        kDisabledExtensionsForEntry76,             // DisabledExtensions
        arraysize(kCrBugsForEntry76),              // CrBugs size
        kCrBugsForEntry76,                         // CrBugs
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
            &kGLStringsForEntry76,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        77,  // id
        "Testing fences was broken on Mali ES2 drivers",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry77),  // DisabledExtensions size
        kDisabledExtensionsForEntry77,             // DisabledExtensions
        arraysize(kCrBugsForEntry77),              // CrBugs size
        kCrBugsForEntry77,                         // CrBugs
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
            &kGLStringsForEntry77,                  // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry77,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        78,  // id
        "Testing fences was broken on Broadcom drivers",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry78),  // DisabledExtensions size
        kDisabledExtensionsForEntry78,             // DisabledExtensions
        arraysize(kCrBugsForEntry78),              // CrBugs size
        kCrBugsForEntry78,                         // CrBugs
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
            &kGLStringsForEntry78,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        82,  // id
        "PBO mappings segfault on certain older Qualcomm drivers",
        arraysize(kFeatureListForEntry82),  // features size
        kFeatureListForEntry82,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry82),       // CrBugs size
        kCrBugsForEntry82,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.3",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry82,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        86,  // id
        "Disable use of Direct3D 11 on Matrox video cards",
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
            0x102b,                                 // vendor_id
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
        87,  // id
        "Disable use of Direct3D 11 on older AMD drivers",
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
            0x1002,                                 // vendor_id
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
        "Always rewrite vec/mat constructors to be consistent",
        arraysize(kFeatureListForEntry88),  // features size
        kFeatureListForEntry88,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry88),       // CrBugs size
        kCrBugsForEntry88,                  // CrBugs
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
        89,  // id
        "Mac drivers handle struct scopes incorrectly",
        arraysize(kFeatureListForEntry89),  // features size
        kFeatureListForEntry89,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry89),       // CrBugs size
        kCrBugsForEntry89,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        90,  // id
        "Linux AMD drivers handle struct scopes incorrectly",
        arraysize(kFeatureListForEntry90),  // features size
        kFeatureListForEntry90,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry90),       // CrBugs size
        kCrBugsForEntry90,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        91,  // id
        "ETC1 non-power-of-two sized textures crash older IMG drivers",
        arraysize(kFeatureListForEntry91),  // features size
        kFeatureListForEntry91,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry91),       // CrBugs size
        kCrBugsForEntry91,                  // CrBugs
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
            &kGLStringsForEntry91,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        92,  // id
        "Old Intel drivers cannot reliably support D3D11",
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
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry92,                 // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        93,  // id
        "The GL implementation on the Android emulator has problems with PBOs.",
        arraysize(kFeatureListForEntry93),  // features size
        kFeatureListForEntry93,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry93),       // CrBugs size
        kCrBugsForEntry93,                  // CrBugs
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
            &kGLStringsForEntry93,                  // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry93,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        94,  // id
        "Disable EGL_KHR_wait_sync on NVIDIA with GLES 3.1",
        0,                                         // feature size
        nullptr,                                   // features
        arraysize(kDisabledExtensionsForEntry94),  // DisabledExtensions size
        kDisabledExtensionsForEntry94,             // DisabledExtensions
        arraysize(kCrBugsForEntry94),              // CrBugs size
        kCrBugsForEntry94,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
             "5.0.2", nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry94,                  // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry94,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        95,  // id
        "glClear does not always work on these drivers",
        arraysize(kFeatureListForEntry95),  // features size
        kFeatureListForEntry95,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry95),       // CrBugs size
        kCrBugsForEntry95,                  // CrBugs
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
            &kGLStringsForEntry95,                  // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry95,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        97,  // id
        "Multisampling has poor performance in Intel BayTrail",
        arraysize(kFeatureListForEntry97),  // features size
        kFeatureListForEntry97,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry97),       // CrBugs size
        kCrBugsForEntry97,                  // CrBugs
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
            &kGLStringsForEntry97,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        98,  // id
        "PowerVR SGX 540 drivers throw GL_OUT_OF_MEMORY error when a buffer "
        "object's size is set to 0",
        arraysize(kFeatureListForEntry98),  // features size
        kFeatureListForEntry98,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry98),       // CrBugs size
        kCrBugsForEntry98,                  // CrBugs
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
            &kGLStringsForEntry98,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        99,  // id
        "Qualcomm driver before Lollipop deletes egl sync objects after "
        "context destruction",
        arraysize(kFeatureListForEntry99),  // features size
        kFeatureListForEntry99,             // features
        0,                                  // DisabledExtensions size
        nullptr,                            // DisabledExtensions
        arraysize(kCrBugsForEntry99),       // CrBugs size
        kCrBugsForEntry99,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "5.0.0", nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry99,                  // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        100,  // id
        "Disable Direct3D11 on systems with AMD switchable graphics",
        arraysize(kFeatureListForEntry100),  // features size
        kFeatureListForEntry100,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry100),       // CrBugs size
        kCrBugsForEntry100,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
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
        101,  // id
        "The Mali-Txxx driver hangs when reading from currently displayed "
        "buffer",
        arraysize(kFeatureListForEntry101),  // features size
        kFeatureListForEntry101,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry101),       // CrBugs size
        kCrBugsForEntry101,                  // CrBugs
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
            &kGLStringsForEntry101,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        102,  // id
        "Adreno 420 driver loses FBO attachment contents on bound FBO deletion",
        arraysize(kFeatureListForEntry102),  // features size
        kFeatureListForEntry102,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry102),       // CrBugs size
        kCrBugsForEntry102,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kGT, GpuControlList::kVersionStyleNumerical,
             "5.0.2", nullptr},                     // os_version
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
        103,  // id
        "Adreno 420 driver drops draw calls after FBO invalidation",
        arraysize(kFeatureListForEntry103),  // features size
        kFeatureListForEntry103,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry103),       // CrBugs size
        kCrBugsForEntry103,                  // CrBugs
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
            &kGLStringsForEntry103,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        104,  // id
        "EXT_occlusion_query_boolean hangs on MediaTek MT8135 pre-Lollipop",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry104),  // DisabledExtensions size
        kDisabledExtensionsForEntry104,             // DisabledExtensions
        0,                                          // CrBugs size
        nullptr,                                    // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "5.0.0", nullptr},                     // os_version
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
        "Framebuffer discarding causes corruption on Mali-4xx",
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
        "EXT_occlusion_query_boolean hangs on PowerVR SGX 544 (IMG) drivers",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry106),  // DisabledExtensions size
        kDisabledExtensionsForEntry106,             // DisabledExtensions
        0,                                          // CrBugs size
        nullptr,                                    // CrBugs
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
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        107,  // id
        "Workaround IMG PowerVR G6xxx drivers bugs",
        arraysize(kFeatureListForEntry107),         // features size
        kFeatureListForEntry107,                    // features
        arraysize(kDisabledExtensionsForEntry107),  // DisabledExtensions size
        kDisabledExtensionsForEntry107,             // DisabledExtensions
        arraysize(kCrBugsForEntry107),              // CrBugs size
        kCrBugsForEntry107,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kBetween, GpuControlList::kVersionStyleNumerical,
             "5.0.0", "5.1.99"},                    // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry107,                // driver info
            &kGLStringsForEntry107,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        108,  // id
        "Mali-4xx does not support GL_RGB format",
        arraysize(kFeatureListForEntry108),  // features size
        kFeatureListForEntry108,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry108),       // CrBugs size
        kCrBugsForEntry108,                  // CrBugs
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
            &kGLStringsForEntry108,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        109,  // id
        "MakeCurrent is slow on Linux with NVIDIA drivers",
        arraysize(kFeatureListForEntry109),  // features size
        kFeatureListForEntry109,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry109),       // CrBugs size
        kCrBugsForEntry109,                  // CrBugs
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
            &kGLStringsForEntry109,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        110,  // id
        "EGL Sync server causes crashes on Adreno 2xx and 3xx drivers",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry110),  // DisabledExtensions size
        kDisabledExtensionsForEntry110,             // DisabledExtensions
        arraysize(kCrBugsForEntry110),              // CrBugs size
        kCrBugsForEntry110,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
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
        "Discard Framebuffer breaks WebGL on Mali-4xx Linux",
        arraysize(kFeatureListForEntry111),  // features size
        kFeatureListForEntry111,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry111),       // CrBugs size
        kCrBugsForEntry111,                  // CrBugs
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
            &kGLStringsForEntry111,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        112,  // id
        "EXT_disjoint_timer_query fails after 2 queries on adreno 3xx in "
        "lollypop",
        arraysize(kFeatureListForEntry112),  // features size
        kFeatureListForEntry112,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry112),       // CrBugs size
        kCrBugsForEntry112,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
             "5.0.0", nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry112,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        113,  // id
        "EXT_disjoint_timer_query fails after 256 queries on adreno 4xx",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry113),  // DisabledExtensions size
        kDisabledExtensionsForEntry113,             // DisabledExtensions
        arraysize(kCrBugsForEntry113),              // CrBugs size
        kCrBugsForEntry113,                         // CrBugs
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
            &kGLStringsForEntry113,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        115,  // id
        "glGetIntegerv with GL_GPU_DISJOINT_EXT causes GL_INVALID_ENUM error",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry115),  // DisabledExtensions size
        kDisabledExtensionsForEntry115,             // DisabledExtensions
        arraysize(kCrBugsForEntry115),              // CrBugs size
        kCrBugsForEntry115,                         // CrBugs
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
            &kGLStringsForEntry115,                 // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry115,                      // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        116,  // id
        "Adreno 420 support for EXT_multisampled_render_to_texture is buggy on "
        "Android < 5.1",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry116),  // DisabledExtensions size
        kDisabledExtensionsForEntry116,             // DisabledExtensions
        arraysize(kCrBugsForEntry116),              // CrBugs size
        kCrBugsForEntry116,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "5.1",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry116,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        117,  // id
        "GL_KHR_blend_equation_advanced breaks blending on Adreno 4xx",
        arraysize(kFeatureListForEntry117),  // features size
        kFeatureListForEntry117,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry117),       // CrBugs size
        kCrBugsForEntry117,                  // CrBugs
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
            &kGLStringsForEntry117,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        118,  // id
        "NVIDIA 331 series drivers shader compiler may crash when attempting "
        "to optimize pow()",
        arraysize(kFeatureListForEntry118),  // features size
        kFeatureListForEntry118,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry118),       // CrBugs size
        kCrBugsForEntry118,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry118,                // driver info
            &kGLStringsForEntry118,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        119,  // id
        "Context lost recovery often fails on Mali-400/450 on Android.",
        arraysize(kFeatureListForEntry119),  // features size
        kFeatureListForEntry119,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry119),       // CrBugs size
        kCrBugsForEntry119,                  // CrBugs
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
            &kGLStringsForEntry119,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        120,  // id
        "CHROMIUM_copy_texture is slow on Mali pre-Lollipop",
        arraysize(kFeatureListForEntry120),  // features size
        kFeatureListForEntry120,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry120),       // CrBugs size
        kCrBugsForEntry120,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "5.0.0", nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry120,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        123,  // id
        "NVIDIA drivers before 346 lack features in NV_path_rendering and "
        "related extensions to implement driver level path rendering.",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry123),  // DisabledExtensions size
        kDisabledExtensionsForEntry123,             // DisabledExtensions
        arraysize(kCrBugsForEntry123),              // CrBugs size
        kCrBugsForEntry123,                         // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
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
        125,  // id
        "glFinish doesn't clear caches on Android",
        arraysize(kFeatureListForEntry125),  // features size
        kFeatureListForEntry125,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry125),       // CrBugs size
        kCrBugsForEntry125,                  // CrBugs
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
            &kGLStringsForEntry125,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        126,  // id
        "Program binaries contain incorrect bound attribute locations on "
        "Adreno 3xx GPUs",
        arraysize(kFeatureListForEntry126),  // features size
        kFeatureListForEntry126,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry126),       // CrBugs size
        kCrBugsForEntry126,                  // CrBugs
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
            &kGLStringsForEntry126,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        127,  // id
        "Android Adreno crashes on binding incomplete cube map texture to FBO",
        arraysize(kFeatureListForEntry127),  // features size
        kFeatureListForEntry127,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry127),       // CrBugs size
        kCrBugsForEntry127,                  // CrBugs
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
            &kGLStringsForEntry127,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        128,  // id
        "Linux ATI drivers crash on binding incomplete cube map texture to FBO",
        arraysize(kFeatureListForEntry128),  // features size
        kFeatureListForEntry128,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry128),       // CrBugs size
        kCrBugsForEntry128,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        129,  // id
        "ANGLE crash on glReadPixels from incomplete cube map texture",
        arraysize(kFeatureListForEntry129),  // features size
        kFeatureListForEntry129,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry129),       // CrBugs size
        kCrBugsForEntry129,                  // CrBugs
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
            &kGLStringsForEntry129,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        130,  // id
        "NVIDIA fails glReadPixels from incomplete cube map texture",
        arraysize(kFeatureListForEntry130),  // features size
        kFeatureListForEntry130,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry130),       // CrBugs size
        kCrBugsForEntry130,                  // CrBugs
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
            &kGLStringsForEntry130,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        131,  // id
        "Linux Mesa drivers crash on glTexSubImage2D() to texture storage "
        "bound to FBO",
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
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
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
        "On Intel GPUs MSAA performance is not acceptable for GPU "
        "rasterization",
        arraysize(kFeatureListForEntry132),  // features size
        kFeatureListForEntry132,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry132),       // CrBugs size
        kCrBugsForEntry132,                  // CrBugs
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
    {
        133,  // id
        "CHROMIUM_copy_texture with 1MB copy per flush to avoid unwanted cache "
        "growth on Adreno",
        arraysize(kFeatureListForEntry133),  // features size
        kFeatureListForEntry133,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry133),       // CrBugs size
        kCrBugsForEntry133,                  // CrBugs
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
            &kGLStringsForEntry133,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        134,  // id
        "glReadPixels fails on FBOs with SRGB_ALPHA textures",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry134),  // DisabledExtensions size
        kDisabledExtensionsForEntry134,             // DisabledExtensions
        arraysize(kCrBugsForEntry134),              // CrBugs size
        kCrBugsForEntry134,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "5.0",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry134,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        135,  // id
        "Screen flickers on 2009 iMacs",
        arraysize(kFeatureListForEntry135),  // features size
        kFeatureListForEntry135,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry135),       // CrBugs size
        kCrBugsForEntry135,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            arraysize(kDeviceIDsForEntry135),       // DeviceIDs size
            kDeviceIDsForEntry135,                  // DeviceIDs
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
        136,  // id
        "glGenerateMipmap fails if the zero texture level is not set on some "
        "Mac drivers",
        arraysize(kFeatureListForEntry136),  // features size
        kFeatureListForEntry136,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry136),       // CrBugs size
        kCrBugsForEntry136,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        137,  // id
        "NVIDIA fails glReadPixels from incomplete cube map texture",
        arraysize(kFeatureListForEntry137),  // features size
        kFeatureListForEntry137,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry137),       // CrBugs size
        kCrBugsForEntry137,                  // CrBugs
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
            &kGLStringsForEntry137,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        138,  // id
        "NVIDIA drivers before 346 lack features in NV_path_rendering and "
        "related extensions to implement driver level path rendering.",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry138),  // DisabledExtensions size
        kDisabledExtensionsForEntry138,             // DisabledExtensions
        arraysize(kCrBugsForEntry138),              // CrBugs size
        kCrBugsForEntry138,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry138,                // driver info
            &kGLStringsForEntry138,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        139,  // id
        "Mesa drivers wrongly report supporting GL_EXT_texture_rg with GLES "
        "2.0 prior version 11.1",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry139),  // DisabledExtensions size
        kDisabledExtensionsForEntry139,             // DisabledExtensions
        arraysize(kCrBugsForEntry139),              // CrBugs size
        kCrBugsForEntry139,                         // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry139,                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry139,                      // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        140,  // id
        "glReadPixels fails on FBOs with SRGB_ALPHA textures, Nexus 5X",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry140),  // DisabledExtensions size
        kDisabledExtensionsForEntry140,             // DisabledExtensions
        arraysize(kCrBugsForEntry140),              // CrBugs size
        kCrBugsForEntry140,                         // CrBugs
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
            &kGLStringsForEntry140,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        141,  // id
        "Framebuffer discarding can hurt performance on non-tilers",
        arraysize(kFeatureListForEntry141),  // features size
        kFeatureListForEntry141,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry141),       // CrBugs size
        kCrBugsForEntry141,                  // CrBugs
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
        142,  // id
        "Pack parameters work incorrectly with pack buffer bound",
        arraysize(kFeatureListForEntry142),  // features size
        kFeatureListForEntry142,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry142),       // CrBugs size
        kCrBugsForEntry142,                  // CrBugs
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
            &kGLStringsForEntry142,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        143,  // id
        "Timer queries crash on Intel GPUs on Linux",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry143),  // DisabledExtensions size
        kDisabledExtensionsForEntry143,             // DisabledExtensions
        arraysize(kCrBugsForEntry143),              // CrBugs size
        kCrBugsForEntry143,                         // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry143,                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        144,  // id
        "Pack parameters work incorrectly with pack buffer bound",
        arraysize(kFeatureListForEntry144),  // features size
        kFeatureListForEntry144,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry144),       // CrBugs size
        kCrBugsForEntry144,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        145,  // id
        "EGLImage ref counting across EGLContext/threads is broken",
        arraysize(kFeatureListForEntry145),  // features size
        kFeatureListForEntry145,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry145),       // CrBugs size
        kCrBugsForEntry145,                  // CrBugs
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
            &kGLStringsForEntry145,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        147,  // id
        "Limit max texure size to 4096 on all of Android",
        arraysize(kFeatureListForEntry147),  // features size
        kFeatureListForEntry147,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        0,                                   // CrBugs size
        nullptr,                             // CrBugs
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
        148,  // id
        "Mali-4xx GPU on JB doesn't support DetachGLContext",
        arraysize(kFeatureListForEntry148),  // features size
        kFeatureListForEntry148,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        0,                                   // CrBugs size
        nullptr,                             // CrBugs
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
            &kGLStringsForEntry148,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        149,  // id
        "Direct composition flashes black initially on Win <10",
        arraysize(kFeatureListForEntry149),  // features size
        kFeatureListForEntry149,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry149),       // CrBugs size
        kCrBugsForEntry149,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "10.0", nullptr},                      // os_version
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
        150,  // id
        "Alignment works incorrectly with unpack buffer bound",
        arraysize(kFeatureListForEntry150),  // features size
        kFeatureListForEntry150,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry150),       // CrBugs size
        kCrBugsForEntry150,                  // CrBugs
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
            &kGLStringsForEntry150,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        151,  // id
        "Alignment works incorrectly with unpack buffer bound",
        arraysize(kFeatureListForEntry151),  // features size
        kFeatureListForEntry151,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry151),       // CrBugs size
        kCrBugsForEntry151,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        152,  // id
        "copyTexImage2D fails when reading from IOSurface on multiple GPU "
        "types.",
        arraysize(kFeatureListForEntry152),  // features size
        kFeatureListForEntry152,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry152),       // CrBugs size
        kCrBugsForEntry152,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        153,  // id
        "Vivante GC1000 with EXT_multisampled_render_to_texture fails "
        "glReadPixels",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry153),  // DisabledExtensions size
        kDisabledExtensionsForEntry153,             // DisabledExtensions
        arraysize(kCrBugsForEntry153),              // CrBugs size
        kCrBugsForEntry153,                         // CrBugs
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
            &kGLStringsForEntry153,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        156,  // id
        "glEGLImageTargetTexture2DOES crashes",
        arraysize(kFeatureListForEntry156),  // features size
        kFeatureListForEntry156,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry156),       // CrBugs size
        kCrBugsForEntry156,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kBetween, GpuControlList::kVersionStyleNumerical,
             "4.4", "4.4.4"},                       // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry156,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        157,  // id
        "Testing fences was broken on Mali ES2 drivers for specific phone "
        "models",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry157),  // DisabledExtensions size
        kDisabledExtensionsForEntry157,             // DisabledExtensions
        arraysize(kCrBugsForEntry157),              // CrBugs size
        kCrBugsForEntry157,                         // CrBugs
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
            &kGLStringsForEntry157,                 // GL strings
            &kMachineModelInfoForEntry157,          // machine model info
            &kMoreForEntry157,                      // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        158,  // id
        "IOSurface use becomes pathologically slow over time on 10.10.",
        arraysize(kFeatureListForEntry158),  // features size
        kFeatureListForEntry158,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry158),       // CrBugs size
        kCrBugsForEntry158,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
             "10.10", nullptr},                     // os_version
            0x10de,                                 // vendor_id
            arraysize(kDeviceIDsForEntry158),       // DeviceIDs size
            kDeviceIDsForEntry158,                  // DeviceIDs
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
        159,  // id
        "Framebuffer discarding can hurt performance on non-tilers",
        arraysize(kFeatureListForEntry159),  // features size
        kFeatureListForEntry159,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry159),       // CrBugs size
        kCrBugsForEntry159,                  // CrBugs
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
            &kGLStringsForEntry159,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        160,  // id
        "Framebuffer discarding not useful on NVIDIA Kepler architecture and "
        "later",
        arraysize(kFeatureListForEntry160),  // features size
        kFeatureListForEntry160,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry160),       // CrBugs size
        kCrBugsForEntry160,                  // CrBugs
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
            &kGLStringsForEntry160,                 // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry160,                      // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        161,  // id
        "Framebuffer discarding not useful on NVIDIA Kepler architecture and "
        "later",
        arraysize(kFeatureListForEntry161),  // features size
        kFeatureListForEntry161,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry161),       // CrBugs size
        kCrBugsForEntry161,                  // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry161,                 // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry161,                      // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        162,  // id
        "Framebuffer discarding not useful on NVIDIA Kepler architecture and "
        "later",
        arraysize(kFeatureListForEntry162),  // features size
        kFeatureListForEntry162,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry162),       // CrBugs size
        kCrBugsForEntry162,                  // CrBugs
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
            &kGLStringsForEntry162,                 // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry162,                      // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        163,  // id
        "Multisample renderbuffers with format GL_RGB8 have performance issues "
        "on Intel GPUs.",
        arraysize(kFeatureListForEntry163),  // features size
        kFeatureListForEntry163,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry163),       // CrBugs size
        kCrBugsForEntry163,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
        164,  // id
        "glColorMask does not work for multisample renderbuffers on old AMD "
        "GPUs.",
        arraysize(kFeatureListForEntry164),  // features size
        kFeatureListForEntry164,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry164),       // CrBugs size
        kCrBugsForEntry164,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            arraysize(kDeviceIDsForEntry164),       // DeviceIDs size
            kDeviceIDsForEntry164,                  // DeviceIDs
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
        165,  // id
        "Unpacking overlapping rows from unpack buffers is unstable on NVIDIA "
        "GL driver",
        arraysize(kFeatureListForEntry165),  // features size
        kFeatureListForEntry165,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry165),       // CrBugs size
        kCrBugsForEntry165,                  // CrBugs
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
            &kGLStringsForEntry165,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        167,  // id
        "glEGLImageTargetTexture2DOES crashes on Mali-400",
        arraysize(kFeatureListForEntry167),  // features size
        kFeatureListForEntry167,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry167),       // CrBugs size
        kCrBugsForEntry167,                  // CrBugs
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
            &kGLStringsForEntry167,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        168,  // id
        "VirtualBox driver doesn't correctly support partial swaps.",
        arraysize(kFeatureListForEntry168),  // features size
        kFeatureListForEntry168,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry168),       // CrBugs size
        kCrBugsForEntry168,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x80ee,                                 // vendor_id
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
        169,  // id
        "Mac Drivers store texture level parameters on int16_t that overflow",
        arraysize(kFeatureListForEntry169),  // features size
        kFeatureListForEntry169,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry169),       // CrBugs size
        kCrBugsForEntry169,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "10.12.2", nullptr},                   // os_version
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
        170,  // id
        "Zero copy DXGI video hangs on shutdown on Win < 8.1",
        arraysize(kFeatureListForEntry170),  // features size
        kFeatureListForEntry170,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry170),       // CrBugs size
        kCrBugsForEntry170,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "8.1",
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
        172,  // id
        "Limited enabling of Chromium GL_INTEL_framebuffer_CMAA",
        arraysize(kFeatureListForEntry172),  // features size
        kFeatureListForEntry172,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry172),       // CrBugs size
        kCrBugsForEntry172,                  // CrBugs
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
        arraysize(kExceptionsForEntry172),  // exceptions count
        kExceptionsForEntry172,             // exceptions
    },
    {
        174,  // id
        "Adreno 4xx support for EXT_multisampled_render_to_texture is buggy on "
        "Android 7.0",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry174),  // DisabledExtensions size
        kDisabledExtensionsForEntry174,             // DisabledExtensions
        arraysize(kCrBugsForEntry174),              // CrBugs size
        kCrBugsForEntry174,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kBetween, GpuControlList::kVersionStyleNumerical,
             "7.0.0", "7.0.99"},                    // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry174,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        175,  // id
        "Adreno 5xx support for EXT_multisampled_render_to_texture is buggy on "
        "Android < 7.0",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry175),  // DisabledExtensions size
        kDisabledExtensionsForEntry175,             // DisabledExtensions
        arraysize(kCrBugsForEntry175),              // CrBugs size
        kCrBugsForEntry175,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "7.0",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry175,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        176,  // id
        "glClear does not work on Acer Predator GT-810",
        arraysize(kFeatureListForEntry176),  // features size
        kFeatureListForEntry176,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry176),       // CrBugs size
        kCrBugsForEntry176,                  // CrBugs
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
            &kGLStringsForEntry176,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        177,  // id
        "glGetFragData{Location|Index} works incorrectly on Max",
        arraysize(kFeatureListForEntry177),  // features size
        kFeatureListForEntry177,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry177),       // CrBugs size
        kCrBugsForEntry177,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        178,  // id
        "GL_KHR_blend_equation_advanced is incorrectly implemented on Intel "
        "BayTrail on KitKat",
        arraysize(kFeatureListForEntry178),  // features size
        kFeatureListForEntry178,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry178),       // CrBugs size
        kCrBugsForEntry178,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "5.0",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry178,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        179,  // id
        "glResumeTransformFeedback works incorrectly on Intel GPUs",
        arraysize(kFeatureListForEntry179),  // features size
        kFeatureListForEntry179,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry179),       // CrBugs size
        kCrBugsForEntry179,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
        180,  // id
        "eglCreateImageKHR fails for one component textures on PowerVR",
        arraysize(kFeatureListForEntry180),  // features size
        kFeatureListForEntry180,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry180),       // CrBugs size
        kCrBugsForEntry180,                  // CrBugs
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
            &kGLStringsForEntry180,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        181,  // id
        "glTexStorage* are buggy when base mipmap level is not 0",
        arraysize(kFeatureListForEntry181),  // features size
        kFeatureListForEntry181,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry181),       // CrBugs size
        kCrBugsForEntry181,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "10.12.4", nullptr},                   // os_version
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
        182,  // id
        "Frequent hang in glClear on old android versions on Mali-T7xx",
        arraysize(kFeatureListForEntry182),  // features size
        kFeatureListForEntry182,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry182),       // CrBugs size
        kCrBugsForEntry182,                  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "6.0",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry182,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        183,  // id
        "Result of abs(i) where i is an integer in vertex shader is wrong",
        arraysize(kFeatureListForEntry183),  // features size
        kFeatureListForEntry183,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry183),       // CrBugs size
        kCrBugsForEntry183,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
        184,  // id
        "Rewrite texelFetchOffset to texelFetch for Intel Mac",
        arraysize(kFeatureListForEntry184),  // features size
        kFeatureListForEntry184,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry184),       // CrBugs size
        kCrBugsForEntry184,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
        185,  // id
        "Zero-copy NV12 video displays incorrect colors on NVIDIA drivers.",
        arraysize(kFeatureListForEntry185),  // features size
        kFeatureListForEntry185,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry185),       // CrBugs size
        kCrBugsForEntry185,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
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
        186,  // id
        "Rewrite condition in for and while loops for Intel Mac",
        arraysize(kFeatureListForEntry186),  // features size
        kFeatureListForEntry186,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry186),       // CrBugs size
        kCrBugsForEntry186,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
        187,  // id
        "Rewrite do-while loops to simpler constructs on Mac",
        arraysize(kFeatureListForEntry187),  // features size
        kFeatureListForEntry187,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry187),       // CrBugs size
        kCrBugsForEntry187,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "10.11", nullptr},                     // os_version
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
        188,  // id
        "AVSampleBufferDisplayLayer leaks IOSurfaces on 10.9.",
        arraysize(kFeatureListForEntry188),  // features size
        kFeatureListForEntry188,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry188),       // CrBugs size
        kCrBugsForEntry188,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
             "10.10", nullptr},                     // os_version
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
        189,  // id
        "Do TexImage2D first before CopyTexImage2D for cube map texture on "
        "Intel Mac 10.11",
        arraysize(kFeatureListForEntry189),  // features size
        kFeatureListForEntry189,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry189),       // CrBugs size
        kCrBugsForEntry189,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
             "10.11", nullptr},                     // os_version
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
        190,  // id
        "Disable partial swaps on Mesa drivers (detected with GL_VERSION)",
        arraysize(kFeatureListForEntry190),  // features size
        kFeatureListForEntry190,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry190),       // CrBugs size
        kCrBugsForEntry190,                  // CrBugs
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
            &kGLStringsForEntry190,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        191,  // id
        "Emulate GLSL function isnan() on Intel Mac",
        arraysize(kFeatureListForEntry191),  // features size
        kFeatureListForEntry191,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry191),       // CrBugs size
        kCrBugsForEntry191,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            arraysize(kDeviceIDsForEntry191),       // DeviceIDs size
            kDeviceIDsForEntry191,                  // DeviceIDs
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
        192,  // id
        "Decode and encode before generateMipmap for srgb format textures on "
        "os except macosx",
        arraysize(kFeatureListForEntry192),  // features size
        kFeatureListForEntry192,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry192),       // CrBugs size
        kCrBugsForEntry192,                  // CrBugs
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
            &kMoreForEntry192,                      // more data
        },
        arraysize(kExceptionsForEntry192),  // exceptions count
        kExceptionsForEntry192,             // exceptions
    },
    {
        193,  // id
        "Decode and encode before generateMipmap for srgb format textures on "
        "macosx",
        arraysize(kFeatureListForEntry193),  // features size
        kFeatureListForEntry193,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry193),       // CrBugs size
        kCrBugsForEntry193,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        194,  // id
        "Init first two levels before CopyTexImage2D for cube map texture on "
        "Intel Mac 10.12",
        arraysize(kFeatureListForEntry194),  // features size
        kFeatureListForEntry194,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry194),       // CrBugs size
        kCrBugsForEntry194,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
             "10.12", nullptr},                     // os_version
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
        195,  // id
        "Insert statements to reference all members in unused std140/shared "
        "blocks on Mac",
        arraysize(kFeatureListForEntry195),  // features size
        kFeatureListForEntry195,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry195),       // CrBugs size
        kCrBugsForEntry195,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        196,  // id
        "Tex(Sub)Image3D performs incorrectly when uploading from unpack "
        "buffer with GL_UNPACK_IMAGE_HEIGHT greater than zero on Intel Macs",
        arraysize(kFeatureListForEntry196),  // features size
        kFeatureListForEntry196,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry196),       // CrBugs size
        kCrBugsForEntry196,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
        197,  // id
        "adjust src/dst region if blitting pixels outside read framebuffer on "
        "Mac",
        arraysize(kFeatureListForEntry197),  // features size
        kFeatureListForEntry197,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry197),       // CrBugs size
        kCrBugsForEntry197,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        198,  // id
        "adjust src/dst region if blitting pixels outside read framebuffer on "
        "Linux Intel",
        arraysize(kFeatureListForEntry198),  // features size
        kFeatureListForEntry198,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry198),       // CrBugs size
        kCrBugsForEntry198,                  // CrBugs
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
        199,  // id
        "adjust src/dst region if blitting pixels outside read framebuffer on "
        "Linux AMD",
        arraysize(kFeatureListForEntry199),  // features size
        kFeatureListForEntry199,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry199),       // CrBugs size
        kCrBugsForEntry199,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        200,  // id
        "ES3 support is unreliable on some older drivers",
        arraysize(kFeatureListForEntry200),  // features size
        kFeatureListForEntry200,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry200),       // CrBugs size
        kCrBugsForEntry200,                  // CrBugs
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
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        201,  // id
        "AMD drivers in Linux require invariant qualifier to match between "
        "vertex and fragment shaders",
        arraysize(kFeatureListForEntry201),  // features size
        kFeatureListForEntry201,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry201),       // CrBugs size
        kCrBugsForEntry201,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        202,  // id
        "Mac driver GL 4.1 requires invariant and centroid to match between "
        "shaders",
        arraysize(kFeatureListForEntry202),  // features size
        kFeatureListForEntry202,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry202),       // CrBugs size
        kCrBugsForEntry202,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        203,  // id
        "Mesa driver GL 3.3 requires invariant and centroid to match between "
        "shaders",
        arraysize(kFeatureListForEntry203),  // features size
        kFeatureListForEntry203,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry203),       // CrBugs size
        kCrBugsForEntry203,                  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry203,                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            &kMoreForEntry203,                      // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        205,  // id
        "Adreno 5xx support for EXT_multisampled_render_to_texture is buggy on "
        "Android 7.1",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry205),  // DisabledExtensions size
        kDisabledExtensionsForEntry205,             // DisabledExtensions
        arraysize(kCrBugsForEntry205),              // CrBugs size
        kCrBugsForEntry205,                         // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
             "7.1.0", nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForEntry205,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        206,  // id
        "Disable KHR_blend_equation_advanced until cc shaders are updated",
        0,                                          // feature size
        nullptr,                                    // features
        arraysize(kDisabledExtensionsForEntry206),  // DisabledExtensions size
        kDisabledExtensionsForEntry206,             // DisabledExtensions
        arraysize(kCrBugsForEntry206),              // CrBugs size
        kCrBugsForEntry206,                         // CrBugs
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
        207,  // id
        "Decode and Encode before generateMipmap for srgb format textures on "
        "Windows",
        arraysize(kFeatureListForEntry207),  // features size
        kFeatureListForEntry207,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry207),       // CrBugs size
        kCrBugsForEntry207,                  // CrBugs
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
        208,  // id
        "Decode and Encode before generateMipmap for srgb format textures on "
        "Linux Mesa ANGLE path",
        arraysize(kFeatureListForEntry208),  // features size
        kFeatureListForEntry208,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry208),       // CrBugs size
        kCrBugsForEntry208,                  // CrBugs
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
            &kGLStringsForEntry208,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        209,  // id
        "Decode and Encode before generateMipmap for srgb format textures on "
        "Chromeos Intel",
        arraysize(kFeatureListForEntry209),  // features size
        kFeatureListForEntry209,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry209),       // CrBugs size
        kCrBugsForEntry209,                  // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
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
        210,  // id
        "Decode and Encode before generateMipmap for srgb format textures on "
        "Linux AMD",
        arraysize(kFeatureListForEntry210),  // features size
        kFeatureListForEntry210,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry210),       // CrBugs size
        kCrBugsForEntry210,                  // CrBugs
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        211,  // id
        "Rewrite -float to 0.0 - float for Intel Mac",
        arraysize(kFeatureListForEntry211),  // features size
        kFeatureListForEntry211,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry211),       // CrBugs size
        kCrBugsForEntry211,                  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
             "10.11", nullptr},                     // os_version
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
        212,  // id
        "Program binaries don't contain transform feedback varyings on "
        "Qualcomm GPUs",
        arraysize(kFeatureListForEntry212),  // features size
        kFeatureListForEntry212,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry212),       // CrBugs size
        kCrBugsForEntry212,                  // CrBugs
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
            &kGLStringsForEntry212,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        213,  // id
        "The Mali-Gxx driver does not guarantee flush ordering",
        arraysize(kFeatureListForEntry213),  // features size
        kFeatureListForEntry213,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry213),       // CrBugs size
        kCrBugsForEntry213,                  // CrBugs
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
            &kGLStringsForEntry213,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        214,  // id
        "Some Adreno 3xx don't setup scissor state correctly when FBO0 is "
        "bound, nor support MSAA properly.",
        arraysize(kFeatureListForEntry214),  // features size
        kFeatureListForEntry214,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry214),       // CrBugs size
        kCrBugsForEntry214,                  // CrBugs
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
            &kGLStringsForEntry214,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        215,  // id
        "Fake no-op GPU driver bug workaround for testing",
        arraysize(kFeatureListForEntry215),  // features size
        kFeatureListForEntry215,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry215),       // CrBugs size
        kCrBugsForEntry215,                  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0xbad9,                                 // vendor_id
            arraysize(kDeviceIDsForEntry215),       // DeviceIDs size
            kDeviceIDsForEntry215,                  // DeviceIDs
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
        216,  // id
        "Pack parameters work incorrectly with pack buffer bound",
        arraysize(kFeatureListForEntry216),  // features size
        kFeatureListForEntry216,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry216),       // CrBugs size
        kCrBugsForEntry216,                  // CrBugs
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
            &kGLStringsForEntry216,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        217,  // id
        "Alignment works incorrectly with unpack buffer bound",
        arraysize(kFeatureListForEntry217),  // features size
        kFeatureListForEntry217,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry217),       // CrBugs size
        kCrBugsForEntry217,                  // CrBugs
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
            &kGLStringsForEntry217,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        219,  // id
        "Zero-copy DXGI video hangs or displays incorrect colors on AMD "
        "drivers",
        arraysize(kFeatureListForEntry219),  // features size
        kFeatureListForEntry219,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry219),       // CrBugs size
        kCrBugsForEntry219,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
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
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        220,  // id
        "NV12 DXGI video displays incorrect colors on older AMD drivers",
        arraysize(kFeatureListForEntry220),  // features size
        kFeatureListForEntry220,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry220),       // CrBugs size
        kCrBugsForEntry220,                  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x1002,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            &kDriverInfoForEntry220,                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        221,  // id
        "Very large instanced draw calls crash on some Adreno 3xx drivers",
        arraysize(kFeatureListForEntry221),  // features size
        kFeatureListForEntry221,             // features
        0,                                   // DisabledExtensions size
        nullptr,                             // DisabledExtensions
        arraysize(kCrBugsForEntry221),       // CrBugs size
        kCrBugsForEntry221,                  // CrBugs
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
            &kGLStringsForEntry221,                 // GL strings
            nullptr,                                // machine model info
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
};
}  // namespace gpu
