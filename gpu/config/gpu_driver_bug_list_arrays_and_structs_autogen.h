// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_CONFIG_GPU_DRIVER_BUG_LIST_ARRAYS_AND_STRUCTS_AUTOGEN_H_
#define GPU_CONFIG_GPU_DRIVER_BUG_LIST_ARRAYS_AND_STRUCTS_AUTOGEN_H_

#include "gpu/config/gpu_driver_bug_workaround_type.h"

namespace gpu {
const int kFeatureListForEntry1[1] = {
    USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS,
};

const GpuControlList::GLStrings kGLStringsForEntry1 = {
    "Imagination.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry2[1] = {
    USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS,
};

const GpuControlList::GLStrings kGLStringsForEntry2 = {
    "ARM.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry6[1] = {
    RESTORE_SCISSOR_ON_FBO_CHANGE,
};

const uint32_t kCrBugsForEntry6[2] = {
    165493, 222018,
};

const GpuControlList::GLStrings kGLStringsForEntry6 = {
    "Qualcomm.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry7[1] = {
    NEEDS_OFFSCREEN_BUFFER_WORKAROUND,
};

const uint32_t kCrBugsForEntry7[1] = {
    89557,
};

const int kFeatureListForEntry17[1] = {
    EXIT_ON_CONTEXT_LOST,
};

const int kFeatureListForEntry19[1] = {
    DISABLE_DEPTH_TEXTURE,
};

const char* kDisabledExtensionsForEntry19[1] = {
    "GL_OES_depth_texture",
};

const uint32_t kCrBugsForEntry19[1] = {
    682075,
};

const GpuControlList::GLStrings kGLStringsForEntry19 = {
    nullptr, "Adreno \\(TM\\) [23].*", nullptr, nullptr,
};

const int kFeatureListForEntry20[1] = {
    DISABLE_EXT_DRAW_BUFFERS,
};

const uint32_t kDeviceIDsForEntry20[1] = {
    0x0fd5,
};

const int kFeatureListForEntry21[1] = {
    UNBIND_FBO_ON_CONTEXT_SWITCH,
};

const uint32_t kCrBugsForEntry21[2] = {
    179250, 235935,
};

const GpuControlList::GLStrings kGLStringsForEntry21 = {
    nullptr, nullptr, ".*GL_VIV_shader_binary.*", nullptr,
};

const int kFeatureListForEntry22[1] = {
    UNBIND_FBO_ON_CONTEXT_SWITCH,
};

const uint32_t kCrBugsForEntry22[1] = {
    230896,
};

const GpuControlList::GLStrings kGLStringsForEntry22 = {
    "Imagination.*", nullptr, nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry23[1] = {
    "GL_OES_standard_derivatives",
};

const uint32_t kCrBugsForEntry23[1] = {
    243038,
};

const uint32_t kDeviceIDsForEntry23[2] = {
    0xa011, 0xa012,
};

const int kFeatureListForEntry24[1] = {
    USE_NON_ZERO_SIZE_FOR_CLIENT_SIDE_STREAM_BUFFERS,
};

const uint32_t kCrBugsForEntry24[1] = {
    231082,
};

const GpuControlList::GLStrings kGLStringsForEntry24 = {
    "ARM.*", ".*Mali-4.*", nullptr, nullptr,
};

const int kFeatureListForEntry26[1] = {
    DISABLE_D3D11,
};

const int kFeatureListForEntry27[1] = {
    SWIZZLE_RGBA_FOR_ASYNC_READPIXELS,
};

const uint32_t kCrBugsForEntry27[1] = {
    265115,
};

const uint32_t kDeviceIDsForEntry27[11] = {
    0x0402, 0x0406, 0x040a, 0x0412, 0x0416, 0x041a,
    0x0a04, 0x0a16, 0x0a22, 0x0a26, 0x0a2a,
};

const int kFeatureListForEntry30[1] = {
    DISABLE_MULTIMONITOR_MULTISAMPLING,
};

const uint32_t kCrBugsForEntry30[1] = {
    237931,
};

const int kFeatureListForEntry31[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const uint32_t kCrBugsForEntry31[5] = {
    154715, 10068, 269829, 294779, 285292,
};

const GpuControlList::GLStrings kGLStringsForEntry31 = {
    "ARM.*", "Mali-T.*", nullptr, nullptr,
};

const int kFeatureListForEntry32[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const uint32_t kCrBugsForEntry32[1] = {
    179815,
};

const GpuControlList::GLStrings kGLStringsForEntry32 = {
    "Broadcom.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry33[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const GpuControlList::GLStrings kGLStringsForEntry33 = {
    "Imagination.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry34[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const uint32_t kCrBugsForEntry34[3] = {
    179250, 229643, 230896,
};

const GpuControlList::GLStrings kGLStringsForEntry34 = {
    nullptr, nullptr, ".*GL_VIV_shader_binary.*", nullptr,
};

const int kFeatureListForEntry35[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const uint32_t kCrBugsForEntry35[1] = {
    163464,
};

const GpuControlList::GLStrings kGLStringsForEntry35 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry36[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const uint32_t kCrBugsForEntry36[2] = {
    163464, 233612,
};

const GpuControlList::GLStrings kGLStringsForEntry36 = {
    "Qualcomm.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry37[1] = {
    INIT_GL_POSITION_IN_VERTEX_SHADER,
};

const uint32_t kCrBugsForEntry37[1] = {
    286468,
};

const GpuControlList::GLStrings kGLStringsForEntry37 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry38[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const uint32_t kCrBugsForEntry38[1] = {
    289461,
};

const GpuControlList::GLStrings kGLStringsForEntry38 = {
    "Qualcomm.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry39[1] = {
    VALIDATE_MULTISAMPLE_BUFFER_ALLOCATION,
};

const uint32_t kCrBugsForEntry39[1] = {
    290391,
};

const int kFeatureListForEntry40[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry40[1] = {
    290876,
};

const GpuControlList::GLStrings kGLStringsForEntry40 = {
    "ARM.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry42[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry42[2] = {
    290876, 488463,
};

const GpuControlList::GLStrings kGLStringsForEntry42 = {
    "Imagination.*", "PowerVR SGX 5.*", nullptr, nullptr,
};

const int kFeatureListForEntry43[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry43[1] = {
    299494,
};

const GpuControlList::GLStrings kGLStringsForEntry43 = {
    nullptr, nullptr, ".*GL_VIV_shader_binary.*", nullptr,
};

const int kFeatureListForEntry44[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry44[1] = {
    301988,
};

const int kFeatureListForEntry45[1] = {
    UNFOLD_SHORT_CIRCUIT_AS_TERNARY_OPERATION,
};

const uint32_t kCrBugsForEntry45[1] = {
    307751,
};

const int kFeatureListForEntry48[1] = {
    FORCE_DISCRETE_GPU,
};

const uint32_t kCrBugsForEntry48[1] = {
    113703,
};

const char* kMachineModelNameForEntry48[1] = {
    "MacBookPro",
};

const GpuControlList::MachineModelInfo kMachineModelInfoForEntry48 = {
    arraysize(kMachineModelNameForEntry48),  // machine model name size
    kMachineModelNameForEntry48,             // machine model names
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "8",
     nullptr},  // machine model version
};

const GpuControlList::More kMoreForEntry48 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "2",
     nullptr},  // gpu_count
};

const int kFeatureListForEntry49[1] = {
    WAKE_UP_GPU_BEFORE_DRAWING,
};

const uint32_t kCrBugsForEntry49[1] = {
    309734,
};

const GpuControlList::GLStrings kGLStringsForEntry49 = {
    "Qualcomm.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry51[1] = {
    TEXSUBIMAGE_FASTER_THAN_TEXIMAGE,
};

const GpuControlList::GLStrings kGLStringsForEntry51 = {
    nullptr, "ANGLE.*", nullptr, nullptr,
};

const int kFeatureListForEntry52[1] = {
    DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE,
};

const uint32_t kCrBugsForEntry52[4] = {
    449116, 471200, 612474, 682075,
};

const GpuControlList::GLStrings kGLStringsForEntry52 = {
    nullptr, "Adreno \\(TM\\) 4.*", nullptr, nullptr,
};

const int kFeatureListForEntry54[1] = {
    CLEAR_UNIFORMS_BEFORE_FIRST_PROGRAM_USE,
};

const uint32_t kCrBugsForEntry54[2] = {
    124764, 349137,
};

const int kFeatureListForEntry55[1] = {
    COUNT_ALL_IN_VARYINGS_PACKING,
};

const uint32_t kCrBugsForEntry55[1] = {
    333885,
};

const GpuControlList::DriverInfo kDriverInfoForEntry55 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry56[1] = {
    COUNT_ALL_IN_VARYINGS_PACKING,
};

const uint32_t kCrBugsForEntry56[1] = {
    333885,
};

const GpuControlList::DriverInfo kDriverInfoForEntry56 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry59[1] = {
    DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE,
};

const uint32_t kCrBugsForEntry59[1] = {
    116370,
};

const uint32_t kDeviceIDsForEntry59[5] = {
    0x0152, 0x0156, 0x015a, 0x0162, 0x0166,
};

const int kFeatureListForEntry64[1] = {
    INIT_TEXTURE_MAX_ANISOTROPY,
};

const uint32_t kCrBugsForEntry64[1] = {
    348237,
};

const int kFeatureListForEntry65[1] = {
    INIT_VERTEX_ATTRIBUTES,
};

const uint32_t kCrBugsForEntry65[1] = {
    351528,
};

const GpuControlList::GLStrings kGLStringsForEntry65 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry68[1] = {
    DISABLE_POST_SUB_BUFFERS_FOR_ONSCREEN_SURFACES,
};

const uint32_t kCrBugsForEntry68[1] = {
    339493,
};

const GpuControlList::GLStrings kGLStringsForEntry68 = {
    nullptr, ".*Mesa.*", nullptr, nullptr,
};

const int kFeatureListForEntry69[3] = {
    MAX_VARYING_VECTORS_16, MAX_VERTEX_UNIFORM_VECTORS_256,
    MAX_FRAGMENT_UNIFORM_VECTORS_32,
};

const uint32_t kCrBugsForEntry69[1] = {
    174845,
};

const GpuControlList::DriverInfo kDriverInfoForEntry69 = {
    "osmesa",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry70[1] = {
    DISABLE_D3D11,
};

const uint32_t kCrBugsForEntry70[1] = {
    349929,
};

const GpuControlList::DriverInfo kDriverInfoForEntry70 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
     "8.17.12.6973", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const char* kDisabledExtensionsForEntry71[1] = {
    "GL_OES_standard_derivatives",
};

const uint32_t kCrBugsForEntry71[1] = {
    368005,
};

const GpuControlList::GLStrings kGLStringsForEntry71 = {
    nullptr, nullptr, ".*GL_VIV_shader_binary.*", nullptr,
};

const int kFeatureListForEntry72[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const uint32_t kCrBugsForEntry72[1] = {
    369316,
};

const GpuControlList::GLStrings kGLStringsForEntry72 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry72 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "3.1",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const char* kDisabledExtensionsForEntry74[1] = {
    "EGL_KHR_fence_sync",
};

const uint32_t kCrBugsForEntry74[2] = {
    278606, 382686,
};

const GpuControlList::GLStrings kGLStringsForEntry74 = {
    "Qualcomm.*", nullptr, nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry75[1] = {
    "GL_EXT_multisampled_render_to_texture",
};

const uint32_t kCrBugsForEntry75[1] = {
    362435,
};

const GpuControlList::GLStrings kGLStringsForEntry75 = {
    "ARM.*", ".*Mali-4.*", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry76[1] = {
    "EGL_KHR_fence_sync",
};

const uint32_t kCrBugsForEntry76[1] = {
    371530,
};

const GpuControlList::GLStrings kGLStringsForEntry76 = {
    "Imagination Technologies.*", nullptr, nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry77[1] = {
    "EGL_KHR_fence_sync",
};

const uint32_t kCrBugsForEntry77[4] = {
    378691, 373360, 371530, 398964,
};

const GpuControlList::GLStrings kGLStringsForEntry77 = {
    "ARM.*", "Mali.*", nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry77 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "3.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const char* kDisabledExtensionsForEntry78[1] = {
    "EGL_KHR_fence_sync",
};

const uint32_t kCrBugsForEntry78[3] = {
    378691, 373360, 371530,
};

const GpuControlList::GLStrings kGLStringsForEntry78 = {
    "Broadcom.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry82[1] = {
    DISABLE_ASYNC_READPIXELS,
};

const uint32_t kCrBugsForEntry82[1] = {
    394510,
};

const GpuControlList::GLStrings kGLStringsForEntry82 = {
    "Qualcomm.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry86[1] = {
    DISABLE_D3D11,
};

const uint32_t kCrBugsForEntry86[1] = {
    395861,
};

const int kFeatureListForEntry87[1] = {
    DISABLE_D3D11,
};

const uint32_t kCrBugsForEntry87[1] = {
    402134,
};

const GpuControlList::DriverInfo kDriverInfoForEntry87 = {
    nullptr,  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "2011.1",
     nullptr},  // driver_date
};

const int kFeatureListForEntry88[1] = {
    SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS,
};

const uint32_t kCrBugsForEntry88[1] = {
    398694,
};

const int kFeatureListForEntry89[1] = {
    REGENERATE_STRUCT_NAMES,
};

const uint32_t kCrBugsForEntry89[1] = {
    403957,
};

const int kFeatureListForEntry90[1] = {
    REGENERATE_STRUCT_NAMES,
};

const uint32_t kCrBugsForEntry90[1] = {
    403957,
};

const int kFeatureListForEntry91[1] = {
    ETC1_POWER_OF_TWO_ONLY,
};

const uint32_t kCrBugsForEntry91[2] = {
    150500, 414816,
};

const GpuControlList::GLStrings kGLStringsForEntry91 = {
    "Imagination.*", "PowerVR SGX 5.*", nullptr, nullptr,
};

const int kFeatureListForEntry92[1] = {
    DISABLE_D3D11,
};

const uint32_t kCrBugsForEntry92[1] = {
    363721,
};

const GpuControlList::DriverInfo kDriverInfoForEntry92 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "8.16",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry93[1] = {
    DISABLE_ASYNC_READPIXELS,
};

const uint32_t kCrBugsForEntry93[1] = {
    340882,
};

const GpuControlList::GLStrings kGLStringsForEntry93 = {
    "VMware.*", "Gallium.*", nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry93 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "3.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const char* kDisabledExtensionsForEntry94[1] = {
    "EGL_KHR_wait_sync",
};

const uint32_t kCrBugsForEntry94[1] = {
    433057,
};

const GpuControlList::GLStrings kGLStringsForEntry94 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry94 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "3.1",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const int kFeatureListForEntry95[1] = {
    GL_CLEAR_BROKEN,
};

const uint32_t kCrBugsForEntry95[1] = {
    421271,
};

const GpuControlList::GLStrings kGLStringsForEntry95 = {
    "Imagination.*", nullptr, nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry95 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "3.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const int kFeatureListForEntry97[1] = {
    DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE,
};

const uint32_t kCrBugsForEntry97[1] = {
    443517,
};

const GpuControlList::GLStrings kGLStringsForEntry97 = {
    "Intel", "Intel.*BayTrail", nullptr, nullptr,
};

const int kFeatureListForEntry98[1] = {
    USE_NON_ZERO_SIZE_FOR_CLIENT_SIDE_STREAM_BUFFERS,
};

const uint32_t kCrBugsForEntry98[1] = {
    451501,
};

const GpuControlList::GLStrings kGLStringsForEntry98 = {
    "Imagination.*", "PowerVR SGX 540", nullptr, nullptr,
};

const int kFeatureListForEntry99[1] = {
    IGNORE_EGL_SYNC_FAILURES,
};

const uint32_t kCrBugsForEntry99[1] = {
    453857,
};

const GpuControlList::GLStrings kGLStringsForEntry99 = {
    "Qualcomm.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry100[1] = {
    DISABLE_D3D11,
};

const uint32_t kCrBugsForEntry100[1] = {
    451420,
};

const int kFeatureListForEntry101[1] = {
    DISABLE_POST_SUB_BUFFERS_FOR_ONSCREEN_SURFACES,
};

const uint32_t kCrBugsForEntry101[1] = {
    457511,
};

const GpuControlList::GLStrings kGLStringsForEntry101 = {
    "ARM.*", "Mali-T.*", nullptr, nullptr,
};

const int kFeatureListForEntry102[1] = {
    UNBIND_ATTACHMENTS_ON_BOUND_RENDER_FBO_DELETE,
};

const uint32_t kCrBugsForEntry102[1] = {
    457027,
};

const GpuControlList::GLStrings kGLStringsForEntry102 = {
    nullptr, "Adreno \\(TM\\) 4.*", nullptr, nullptr,
};

const int kFeatureListForEntry103[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry103[1] = {
    443060,
};

const GpuControlList::GLStrings kGLStringsForEntry103 = {
    nullptr, "Adreno \\(TM\\) 4.*", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry104[1] = {
    "GL_EXT_occlusion_query_boolean",
};

const GpuControlList::GLStrings kGLStringsForEntry104 = {
    "Imagination.*", "PowerVR Rogue Han", nullptr, nullptr,
};

const int kFeatureListForEntry105[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry105[2] = {
    449488, 451230,
};

const GpuControlList::GLStrings kGLStringsForEntry105 = {
    nullptr, "Mali-4.*", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry106[1] = {
    "GL_EXT_occlusion_query_boolean",
};

const GpuControlList::GLStrings kGLStringsForEntry106 = {
    "Imagination.*", "PowerVR SGX 544", nullptr, nullptr,
};

const int kFeatureListForEntry107[1] = {
    AVOID_EGL_IMAGE_TARGET_TEXTURE_REUSE,
};

const char* kDisabledExtensionsForEntry107[1] = {
    "EGL_KHR_wait_sync",
};

const uint32_t kCrBugsForEntry107[1] = {
    480992,
};

const GpuControlList::DriverInfo kDriverInfoForEntry107 = {
    nullptr,  // driver_vendor
    {GpuControlList::kBetween, GpuControlList::kVersionStyleNumerical, "1.3",
     "1.4"},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry107 = {
    "Imagination.*", "PowerVR Rogue.*", nullptr, nullptr,
};

const int kFeatureListForEntry108[1] = {
    DISABLE_GL_RGB_FORMAT,
};

const uint32_t kCrBugsForEntry108[1] = {
    449150,
};

const GpuControlList::GLStrings kGLStringsForEntry108 = {
    "ARM.*", ".*Mali-4.*", nullptr, nullptr,
};

const int kFeatureListForEntry109[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const uint32_t kCrBugsForEntry109[2] = {
    449150, 514510,
};

const GpuControlList::GLStrings kGLStringsForEntry109 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry110[1] = {
    "EGL_KHR_wait_sync",
};

const uint32_t kCrBugsForEntry110[1] = {
    482298,
};

const GpuControlList::DriverInfo kDriverInfoForEntry110 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "95",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry110 = {
    "Qualcomm.*", "Adreno \\(TM\\) [23].*", nullptr, nullptr,
};

const int kFeatureListForEntry111[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry111[1] = {
    485814,
};

const GpuControlList::GLStrings kGLStringsForEntry111 = {
    "ARM.*", ".*Mali-4.*", nullptr, nullptr,
};

const int kFeatureListForEntry112[1] = {
    DISABLE_TIMESTAMP_QUERIES,
};

const uint32_t kCrBugsForEntry112[1] = {
    477514,
};

const GpuControlList::GLStrings kGLStringsForEntry112 = {
    "Qualcomm.*", "Adreno \\(TM\\) 3.*", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry113[1] = {
    "GL_EXT_disjoint_timer_query",
};

const uint32_t kCrBugsForEntry113[1] = {
    477514,
};

const GpuControlList::GLStrings kGLStringsForEntry113 = {
    nullptr, "Adreno \\(TM\\) 4.*", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry115[1] = {
    "GL_EXT_disjoint_timer_query",
};

const uint32_t kCrBugsForEntry115[1] = {
    462553,
};

const GpuControlList::GLStrings kGLStringsForEntry115 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry115 = {
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

const char* kDisabledExtensionsForEntry116[1] = {
    "GL_EXT_multisampled_render_to_texture",
};

const uint32_t kCrBugsForEntry116[1] = {
    490379,
};

const GpuControlList::GLStrings kGLStringsForEntry116 = {
    nullptr, "Adreno \\(TM\\) 4.*", nullptr, nullptr,
};

const int kFeatureListForEntry117[1] = {
    DISABLE_BLEND_EQUATION_ADVANCED,
};

const uint32_t kCrBugsForEntry117[1] = {
    488485,
};

const GpuControlList::GLStrings kGLStringsForEntry117 = {
    "Qualcomm.*", ".*4\\d\\d", nullptr, nullptr,
};

const int kFeatureListForEntry118[1] = {
    REMOVE_POW_WITH_CONSTANT_EXPONENT,
};

const uint32_t kCrBugsForEntry118[1] = {
    477306,
};

const GpuControlList::DriverInfo kDriverInfoForEntry118 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical, "331",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry118 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry119[1] = {
    EXIT_ON_CONTEXT_LOST,
};

const uint32_t kCrBugsForEntry119[1] = {
    496438,
};

const GpuControlList::GLStrings kGLStringsForEntry119 = {
    "ARM.*", ".*Mali-4.*", nullptr, nullptr,
};

const int kFeatureListForEntry120[1] = {
    MAX_COPY_TEXTURE_CHROMIUM_SIZE_262144,
};

const uint32_t kCrBugsForEntry120[1] = {
    498443,
};

const GpuControlList::GLStrings kGLStringsForEntry120 = {
    "ARM.*", "Mali.*", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry123[1] = {
    "GL_NV_path_rendering",
};

const uint32_t kCrBugsForEntry123[1] = {
    344330,
};

const GpuControlList::DriverInfo kDriverInfoForEntry123 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "346",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry125[1] = {
    UNBIND_EGL_CONTEXT_TO_FLUSH_DRIVER_CACHES,
};

const uint32_t kCrBugsForEntry125[1] = {
    509727,
};

const GpuControlList::GLStrings kGLStringsForEntry125 = {
    nullptr, "Adreno.*", nullptr, nullptr,
};

const int kFeatureListForEntry126[1] = {
    DISABLE_PROGRAM_CACHE,
};

const uint32_t kCrBugsForEntry126[1] = {
    510637,
};

const GpuControlList::GLStrings kGLStringsForEntry126 = {
    nullptr, "Adreno \\(TM\\) 3.*", nullptr, nullptr,
};

const int kFeatureListForEntry127[1] = {
    FORCE_CUBE_MAP_POSITIVE_X_ALLOCATION,
};

const uint32_t kCrBugsForEntry127[1] = {
    518889,
};

const GpuControlList::GLStrings kGLStringsForEntry127 = {
    nullptr, "Adreno.*", nullptr, nullptr,
};

const int kFeatureListForEntry128[1] = {
    FORCE_CUBE_MAP_POSITIVE_X_ALLOCATION,
};

const uint32_t kCrBugsForEntry128[1] = {
    518889,
};

const int kFeatureListForEntry129[1] = {
    FORCE_CUBE_COMPLETE,
};

const uint32_t kCrBugsForEntry129[1] = {
    518889,
};

const GpuControlList::GLStrings kGLStringsForEntry129 = {
    nullptr, "ANGLE.*", nullptr, nullptr,
};

const int kFeatureListForEntry130[1] = {
    FORCE_CUBE_COMPLETE,
};

const uint32_t kCrBugsForEntry130[1] = {
    518889,
};

const GpuControlList::GLStrings kGLStringsForEntry130 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry131[1] = {
    DISABLE_TEXTURE_STORAGE,
};

const uint32_t kCrBugsForEntry131[1] = {
    521904,
};

const GpuControlList::DriverInfo kDriverInfoForEntry131 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "10.6",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry132[1] = {
    MSAA_IS_SLOW,
};

const uint32_t kCrBugsForEntry132[1] = {
    527565,
};

const int kFeatureListForEntry133[1] = {
    MAX_COPY_TEXTURE_CHROMIUM_SIZE_1048576,
};

const uint32_t kCrBugsForEntry133[1] = {
    542478,
};

const GpuControlList::GLStrings kGLStringsForEntry133 = {
    nullptr, "Adreno.*", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry134[1] = {
    "GL_EXT_sRGB",
};

const uint32_t kCrBugsForEntry134[1] = {
    550292,
};

const GpuControlList::GLStrings kGLStringsForEntry134 = {
    "Qualcomm.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry135[2] = {
    DISABLE_OVERLAY_CA_LAYERS, DISABLE_POST_SUB_BUFFERS_FOR_ONSCREEN_SURFACES,
};

const uint32_t kCrBugsForEntry135[1] = {
    543324,
};

const uint32_t kDeviceIDsForEntry135[4] = {
    0x9440, 0x944a, 0x9488, 0x9490,
};

const int kFeatureListForEntry136[1] = {
    SET_ZERO_LEVEL_BEFORE_GENERATING_MIPMAP,
};

const uint32_t kCrBugsForEntry136[1] = {
    560499,
};

const int kFeatureListForEntry137[1] = {
    FORCE_CUBE_COMPLETE,
};

const uint32_t kCrBugsForEntry137[1] = {
    518889,
};

const GpuControlList::GLStrings kGLStringsForEntry137 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry138[1] = {
    "GL_NV_path_rendering",
};

const uint32_t kCrBugsForEntry138[1] = {
    344330,
};

const GpuControlList::DriverInfo kDriverInfoForEntry138 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "346",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry138 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry139[1] = {
    "GL_EXT_texture_rg",
};

const uint32_t kCrBugsForEntry139[1] = {
    545904,
};

const GpuControlList::DriverInfo kDriverInfoForEntry139 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "11.1",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::More kMoreForEntry139 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "3.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const char* kDisabledExtensionsForEntry140[1] = {
    "GL_EXT_sRGB",
};

const uint32_t kCrBugsForEntry140[2] = {
    550292, 565179,
};

const GpuControlList::GLStrings kGLStringsForEntry140 = {
    "Qualcomm", "Adreno \\(TM\\) 4.*", nullptr, nullptr,
};

const int kFeatureListForEntry141[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry141[1] = {
    570897,
};

const int kFeatureListForEntry142[1] = {
    PACK_PARAMETERS_WORKAROUND_WITH_PACK_BUFFER,
};

const uint32_t kCrBugsForEntry142[1] = {
    563714,
};

const GpuControlList::GLStrings kGLStringsForEntry142 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry143[2] = {
    "GL_ARB_timer_query", "GL_EXT_timer_query",
};

const uint32_t kCrBugsForEntry143[2] = {
    540543, 576991,
};

const GpuControlList::DriverInfo kDriverInfoForEntry143 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry144[1] = {
    PACK_PARAMETERS_WORKAROUND_WITH_PACK_BUFFER,
};

const uint32_t kCrBugsForEntry144[1] = {
    563714,
};

const int kFeatureListForEntry145[1] = {
    BROKEN_EGL_IMAGE_REF_COUNTING,
};

const uint32_t kCrBugsForEntry145[1] = {
    585250,
};

const GpuControlList::GLStrings kGLStringsForEntry145 = {
    "Qualcomm.*", "Adreno \\(TM\\) [45].*", nullptr, nullptr,
};

const int kFeatureListForEntry147[1] = {
    MAX_TEXTURE_SIZE_LIMIT_4096,
};

const int kFeatureListForEntry148[1] = {
    SURFACE_TEXTURE_CANT_DETACH,
};

const GpuControlList::GLStrings kGLStringsForEntry148 = {
    nullptr, ".*Mali-4.*", nullptr, nullptr,
};

const int kFeatureListForEntry149[1] = {
    DISABLE_DIRECT_COMPOSITION,
};

const uint32_t kCrBugsForEntry149[1] = {
    588588,
};

const int kFeatureListForEntry150[1] = {
    UNPACK_ALIGNMENT_WORKAROUND_WITH_UNPACK_BUFFER,
};

const uint32_t kCrBugsForEntry150[1] = {
    563714,
};

const GpuControlList::GLStrings kGLStringsForEntry150 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry151[1] = {
    UNPACK_ALIGNMENT_WORKAROUND_WITH_UNPACK_BUFFER,
};

const uint32_t kCrBugsForEntry151[1] = {
    563714,
};

const int kFeatureListForEntry152[1] = {
    USE_INTERMEDIARY_FOR_COPY_TEXTURE_IMAGE,
};

const uint32_t kCrBugsForEntry152[1] = {
    581777,
};

const char* kDisabledExtensionsForEntry153[1] = {
    "GL_EXT_multisampled_render_to_texture",
};

const uint32_t kCrBugsForEntry153[1] = {
    594016,
};

const GpuControlList::GLStrings kGLStringsForEntry153 = {
    "Vivante Corporation", "Vivante GC1000", nullptr, nullptr,
};

const int kFeatureListForEntry156[1] = {
    AVDA_DONT_COPY_PICTURES,
};

const uint32_t kCrBugsForEntry156[1] = {
    598474,
};

const GpuControlList::GLStrings kGLStringsForEntry156 = {
    "Imagination.*", "PowerVR SGX 544MP", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry157[1] = {
    "EGL_KHR_fence_sync",
};

const uint32_t kCrBugsForEntry157[1] = {
    589814,
};

const GpuControlList::GLStrings kGLStringsForEntry157 = {
    "ARM.*", "Mali.*", nullptr, nullptr,
};

const char* kMachineModelNameForEntry157[2] = {
    "SM-G361H", "SM-G531H",
};

const GpuControlList::MachineModelInfo kMachineModelInfoForEntry157 = {
    arraysize(kMachineModelNameForEntry157),  // machine model name size
    kMachineModelNameForEntry157,             // machine model names
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // machine model version
};

const GpuControlList::More kMoreForEntry157 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "3.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const int kFeatureListForEntry158[1] = {
    DISABLE_OVERLAY_CA_LAYERS,
};

const uint32_t kCrBugsForEntry158[1] = {
    580616,
};

const uint32_t kDeviceIDsForEntry158[1] = {
    0x0fd5,
};

const int kFeatureListForEntry159[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry159[1] = {
    570897,
};

const GpuControlList::GLStrings kGLStringsForEntry159 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry160[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry160[1] = {
    601753,
};

const GpuControlList::GLStrings kGLStringsForEntry160 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry160 = {
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

const int kFeatureListForEntry161[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry161[1] = {
    601753,
};

const GpuControlList::GLStrings kGLStringsForEntry161 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry161 = {
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

const int kFeatureListForEntry162[1] = {
    DISABLE_DISCARD_FRAMEBUFFER,
};

const uint32_t kCrBugsForEntry162[1] = {
    601753,
};

const GpuControlList::GLStrings kGLStringsForEntry162 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry162 = {
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

const int kFeatureListForEntry163[1] = {
    DISABLE_WEBGL_RGB_MULTISAMPLING_USAGE,
};

const uint32_t kCrBugsForEntry163[1] = {
    607130,
};

const int kFeatureListForEntry164[1] = {
    DISABLE_MULTISAMPLING_COLOR_MASK_USAGE,
};

const uint32_t kCrBugsForEntry164[1] = {
    595948,
};

const uint32_t kDeviceIDsForEntry164[4] = {
    0x6720, 0x6740, 0x6741, 0x68b8,
};

const int kFeatureListForEntry165[1] = {
    UNPACK_OVERLAPPING_ROWS_SEPARATELY_UNPACK_BUFFER,
};

const uint32_t kCrBugsForEntry165[1] = {
    596774,
};

const GpuControlList::GLStrings kGLStringsForEntry165 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry167[1] = {
    AVDA_DONT_COPY_PICTURES,
};

const uint32_t kCrBugsForEntry167[1] = {
    610516,
};

const GpuControlList::GLStrings kGLStringsForEntry167 = {
    "ARM.*", ".*Mali-4.*", nullptr, nullptr,
};

const int kFeatureListForEntry168[1] = {
    DISABLE_POST_SUB_BUFFERS_FOR_ONSCREEN_SURFACES,
};

const uint32_t kCrBugsForEntry168[1] = {
    613722,
};

const int kFeatureListForEntry169[1] = {
    USE_SHADOWED_TEX_LEVEL_PARAMS,
};

const uint32_t kCrBugsForEntry169[1] = {
    610153,
};

const int kFeatureListForEntry170[1] = {
    DISABLE_DXGI_ZERO_COPY_VIDEO,
};

const uint32_t kCrBugsForEntry170[1] = {
    621190,
};

const int kFeatureListForEntry172[1] = {
    DISABLE_FRAMEBUFFER_CMAA,
};

const uint32_t kCrBugsForEntry172[1] = {
    535198,
};

const GpuControlList::DriverInfo kDriverInfoForEntry172Exception0 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::GLStrings kGLStringsForEntry172Exception0 = {
    "Intel.*", ".*Intel.*(Braswell|Broadwell|Skylake).*", nullptr, nullptr,
};

const GpuControlList::More kMoreForEntry172Exception0 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "3.1",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const char* kDisabledExtensionsForEntry174[1] = {
    "GL_EXT_multisampled_render_to_texture",
};

const uint32_t kCrBugsForEntry174[1] = {
    612474,
};

const GpuControlList::GLStrings kGLStringsForEntry174 = {
    nullptr, "Adreno \\(TM\\) 4.*", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry175[1] = {
    "GL_EXT_multisampled_render_to_texture",
};

const uint32_t kCrBugsForEntry175[1] = {
    612474,
};

const GpuControlList::GLStrings kGLStringsForEntry175 = {
    nullptr, "Adreno \\(TM\\) 5.*", nullptr, nullptr,
};

const int kFeatureListForEntry176[1] = {
    GL_CLEAR_BROKEN,
};

const uint32_t kCrBugsForEntry176[1] = {
    633634,
};

const GpuControlList::GLStrings kGLStringsForEntry176 = {
    "Intel", ".*Atom.*x5/x7.*", nullptr, nullptr,
};

const int kFeatureListForEntry177[1] = {
    GET_FRAG_DATA_INFO_BUG,
};

const uint32_t kCrBugsForEntry177[1] = {
    638340,
};

const int kFeatureListForEntry178[1] = {
    DISABLE_BLEND_EQUATION_ADVANCED,
};

const uint32_t kCrBugsForEntry178[1] = {
    639470,
};

const GpuControlList::GLStrings kGLStringsForEntry178 = {
    "Intel.*", "Intel(R) HD Graphics for BayTrail", nullptr, nullptr,
};

const int kFeatureListForEntry179[1] = {
    REBIND_TRANSFORM_FEEDBACK_BEFORE_RESUME,
};

const uint32_t kCrBugsForEntry179[1] = {
    638514,
};

const int kFeatureListForEntry180[1] = {
    AVOID_ONE_COMPONENT_EGL_IMAGES,
};

const uint32_t kCrBugsForEntry180[2] = {
    579060, 632461,
};

const GpuControlList::GLStrings kGLStringsForEntry180 = {
    "Imagination.*", "PowerVR .*", nullptr, nullptr,
};

const int kFeatureListForEntry181[1] = {
    RESET_BASE_MIPMAP_LEVEL_BEFORE_TEXSTORAGE,
};

const uint32_t kCrBugsForEntry181[1] = {
    640506,
};

const int kFeatureListForEntry182[1] = {
    GL_CLEAR_BROKEN,
};

const uint32_t kCrBugsForEntry182[1] = {
    638691,
};

const GpuControlList::GLStrings kGLStringsForEntry182 = {
    nullptr, ".*Mali-T7.*", nullptr, nullptr,
};

const int kFeatureListForEntry183[1] = {
    EMULATE_ABS_INT_FUNCTION,
};

const uint32_t kCrBugsForEntry183[1] = {
    642227,
};

const int kFeatureListForEntry184[1] = {
    REWRITE_TEXELFETCHOFFSET_TO_TEXELFETCH,
};

const uint32_t kCrBugsForEntry184[1] = {
    642605,
};

const int kFeatureListForEntry185[1] = {
    DISABLE_DXGI_ZERO_COPY_VIDEO,
};

const uint32_t kCrBugsForEntry185[1] = {
    635319,
};

const int kFeatureListForEntry186[1] = {
    ADD_AND_TRUE_TO_LOOP_CONDITION,
};

const uint32_t kCrBugsForEntry186[1] = {
    644669,
};

const int kFeatureListForEntry187[1] = {
    REWRITE_DO_WHILE_LOOPS,
};

const uint32_t kCrBugsForEntry187[1] = {
    644669,
};

const int kFeatureListForEntry188[1] = {
    DISABLE_AV_SAMPLE_BUFFER_DISPLAY_LAYER,
};

const uint32_t kCrBugsForEntry188[1] = {
    632178,
};

const int kFeatureListForEntry189[1] = {
    INIT_ONE_CUBE_MAP_LEVEL_BEFORE_COPYTEXIMAGE,
};

const uint32_t kCrBugsForEntry189[1] = {
    648197,
};

const int kFeatureListForEntry190[1] = {
    DISABLE_POST_SUB_BUFFERS_FOR_ONSCREEN_SURFACES,
};

const uint32_t kCrBugsForEntry190[1] = {
    339493,
};

const GpuControlList::GLStrings kGLStringsForEntry190 = {
    nullptr, nullptr, nullptr, ".*Mesa.*",
};

const int kFeatureListForEntry191[1] = {
    EMULATE_ISNAN_ON_FLOAT,
};

const uint32_t kCrBugsForEntry191[1] = {
    650547,
};

const uint32_t kDeviceIDsForEntry191[25] = {
    0x1902, 0x1906, 0x190A, 0x190B, 0x190E, 0x1912, 0x1913, 0x1915, 0x1916,
    0x1917, 0x191A, 0x191B, 0x191D, 0x191E, 0x1921, 0x1923, 0x1926, 0x1927,
    0x192A, 0x192B, 0x192D, 0x1932, 0x193A, 0x193B, 0x193D,
};

const int kFeatureListForEntry192[1] = {
    DECODE_ENCODE_SRGB_FOR_GENERATEMIPMAP,
};

const uint32_t kCrBugsForEntry192[1] = {
    634519,
};

const GpuControlList::More kMoreForEntry192 = {
    GpuControlList::kGLTypeGL,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.4",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const int kFeatureListForEntry193[1] = {
    DECODE_ENCODE_SRGB_FOR_GENERATEMIPMAP,
};

const uint32_t kCrBugsForEntry193[1] = {
    634519,
};

const int kFeatureListForEntry194[1] = {
    INIT_TWO_CUBE_MAP_LEVELS_BEFORE_COPYTEXIMAGE,
};

const uint32_t kCrBugsForEntry194[1] = {
    648197,
};

const int kFeatureListForEntry195[1] = {
    USE_UNUSED_STANDARD_SHARED_BLOCKS,
};

const uint32_t kCrBugsForEntry195[1] = {
    618464,
};

const int kFeatureListForEntry196[1] = {
    UNPACK_IMAGE_HEIGHT_WORKAROUND_WITH_UNPACK_BUFFER,
};

const uint32_t kCrBugsForEntry196[1] = {
    654258,
};

const int kFeatureListForEntry197[1] = {
    ADJUST_SRC_DST_REGION_FOR_BLITFRAMEBUFFER,
};

const uint32_t kCrBugsForEntry197[1] = {
    644740,
};

const int kFeatureListForEntry198[1] = {
    ADJUST_SRC_DST_REGION_FOR_BLITFRAMEBUFFER,
};

const uint32_t kCrBugsForEntry198[1] = {
    664740,
};

const int kFeatureListForEntry199[1] = {
    ADJUST_SRC_DST_REGION_FOR_BLITFRAMEBUFFER,
};

const uint32_t kCrBugsForEntry199[1] = {
    664740,
};

const int kFeatureListForEntry200[1] = {
    DISABLE_ES3_GL_CONTEXT,
};

const uint32_t kCrBugsForEntry200[1] = {
    657925,
};

const int kFeatureListForEntry201[2] = {
    REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3,
    DONT_REMOVE_INVARIANT_FOR_FRAGMENT_INPUT,
};

const uint32_t kCrBugsForEntry201[2] = {
    659326, 639760,
};

const int kFeatureListForEntry202[1] = {
    REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3,
};

const uint32_t kCrBugsForEntry202[2] = {
    639760, 641129,
};

const int kFeatureListForEntry203[1] = {
    REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3,
};

const uint32_t kCrBugsForEntry203[2] = {
    639760, 641129,
};

const GpuControlList::DriverInfo kDriverInfoForEntry203 = {
    "Mesa",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const GpuControlList::More kMoreForEntry203 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "3.3",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
};

const char* kDisabledExtensionsForEntry205[1] = {
    "GL_EXT_multisampled_render_to_texture",
};

const uint32_t kCrBugsForEntry205[1] = {
    663811,
};

const GpuControlList::GLStrings kGLStringsForEntry205 = {
    nullptr, "Adreno \\(TM\\) 5.*", nullptr, nullptr,
};

const char* kDisabledExtensionsForEntry206[2] = {
    "GL_KHR_blend_equation_advanced", "GL_KHR_blend_equation_advanced_coherent",
};

const uint32_t kCrBugsForEntry206[1] = {
    661715,
};

const int kFeatureListForEntry207[1] = {
    DECODE_ENCODE_SRGB_FOR_GENERATEMIPMAP,
};

const uint32_t kCrBugsForEntry207[1] = {
    634519,
};

const int kFeatureListForEntry208[1] = {
    DECODE_ENCODE_SRGB_FOR_GENERATEMIPMAP,
};

const uint32_t kCrBugsForEntry208[1] = {
    634519,
};

const GpuControlList::GLStrings kGLStringsForEntry208 = {
    nullptr, "ANGLE.*", nullptr, nullptr,
};

const int kFeatureListForEntry209[1] = {
    DECODE_ENCODE_SRGB_FOR_GENERATEMIPMAP,
};

const uint32_t kCrBugsForEntry209[1] = {
    634519,
};

const int kFeatureListForEntry210[1] = {
    DECODE_ENCODE_SRGB_FOR_GENERATEMIPMAP,
};

const uint32_t kCrBugsForEntry210[1] = {
    634519,
};

const int kFeatureListForEntry211[1] = {
    REWRITE_FLOAT_UNARY_MINUS_OPERATOR,
};

const uint32_t kCrBugsForEntry211[1] = {
    672380,
};

const int kFeatureListForEntry212[1] = {
    DISABLE_PROGRAM_CACHING_FOR_TRANSFORM_FEEDBACK,
};

const uint32_t kCrBugsForEntry212[1] = {
    658074,
};

const GpuControlList::GLStrings kGLStringsForEntry212 = {
    nullptr, "Adreno.*", nullptr, nullptr,
};

const int kFeatureListForEntry213[1] = {
    USE_VIRTUALIZED_GL_CONTEXTS,
};

const uint32_t kCrBugsForEntry213[1] = {
    678508,
};

const GpuControlList::GLStrings kGLStringsForEntry213 = {
    "ARM.*", "Mali-G.*", nullptr, nullptr,
};

const int kFeatureListForEntry214[2] = {
    DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE,
    FORCE_UPDATE_SCISSOR_STATE_WHEN_BINDING_FBO0,
};

const uint32_t kCrBugsForEntry214[4] = {
    670607, 696627, 698197, 707839,
};

const GpuControlList::GLStrings kGLStringsForEntry214 = {
    nullptr, "Adreno \\(TM\\) 3.*", nullptr, nullptr,
};

const int kFeatureListForEntry215[1] = {
    USE_GPU_DRIVER_WORKAROUND_FOR_TESTING,
};

const uint32_t kCrBugsForEntry215[1] = {
    682912,
};

const uint32_t kDeviceIDsForEntry215[1] = {
    0xbad9,
};

const int kFeatureListForEntry216[1] = {
    PACK_PARAMETERS_WORKAROUND_WITH_PACK_BUFFER,
};

const uint32_t kCrBugsForEntry216[1] = {
    698926,
};

const GpuControlList::GLStrings kGLStringsForEntry216 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry217[1] = {
    UNPACK_ALIGNMENT_WORKAROUND_WITH_UNPACK_BUFFER,
};

const uint32_t kCrBugsForEntry217[1] = {
    698926,
};

const GpuControlList::GLStrings kGLStringsForEntry217 = {
    "NVIDIA.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForEntry219[1] = {
    DISABLE_DXGI_ZERO_COPY_VIDEO,
};

const uint32_t kCrBugsForEntry219[1] = {
    623029,
};

const int kFeatureListForEntry220[1] = {
    DISABLE_NV12_DXGI_VIDEO,
};

const uint32_t kCrBugsForEntry220[1] = {
    644293,
};

const GpuControlList::DriverInfo kDriverInfoForEntry220 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "21.19.519.2",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForEntry221[1] = {
    DISALLOW_LARGE_INSTANCED_DRAW,
};

const uint32_t kCrBugsForEntry221[1] = {
    701682,
};

const GpuControlList::GLStrings kGLStringsForEntry221 = {
    nullptr, "Adreno \\(TM\\) 3.*", nullptr, nullptr,
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_DRIVER_BUG_LIST_ARRAYS_AND_STRUCTS_AUTOGEN_H_
