// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_INFO_COLLECTOR_H_
#define CONTENT_GPU_GPU_INFO_COLLECTOR_H_
#pragma once

#include "base/basictypes.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/gpu_info.h"

struct IDirect3D9;

namespace gpu_info_collector {

// Populate variables with necessary graphics card information.
// Returns true on success.
bool CollectGraphicsInfo(content::GPUInfo* gpu_info);

// Similar to CollectGraphicsInfo, only this collects a subset of variables
// without creating a GL/DirectX context (and without the danger of crashing).
// The subset each platform collects may be different.
CONTENT_EXPORT bool CollectPreliminaryGraphicsInfo(
    content::GPUInfo* gpu_info);

#if defined(OS_WIN)
// Windows provides two ways of doing graphics so we need two ways of
// collecting info based on what's on a user's machine.
// The selection between the two methods is done in the cc file.

// A D3D argument is passed in for testing purposes
CONTENT_EXPORT bool CollectGraphicsInfoD3D(IDirect3D9* d3d,
                                           content::GPUInfo* gpu_info);

// Collects D3D driver version/date through registry lookup.
// Note that this does not require a D3D context.
// device_id here is the raw data in DISPLAY_DEVICE.
CONTENT_EXPORT bool CollectDriverInfoD3D(const std::wstring& device_id,
                                         content::GPUInfo* gpu_info);

// Collect the DirectX Disagnostics information about the attached displays.
bool GetDxDiagnostics(content::DxDiagNode* output);
#endif

// All platforms have a GL version for collecting information
CONTENT_EXPORT bool CollectGraphicsInfoGL(content::GPUInfo* gpu_info);

// Collect GL and Shading language version information
bool CollectGLVersionInfo(content::GPUInfo* gpu_info);

// Platform specific method for collecting vendor and device ids
bool CollectVideoCardInfo(content::GPUInfo* gpu_info);

// Each platform stores the driver version on the GL_VERSION string differently
bool CollectDriverInfoGL(content::GPUInfo* gpu_info);

}  // namespace gpu_info_collector

#endif  // CONTENT_GPU_GPU_INFO_COLLECTOR_H_
