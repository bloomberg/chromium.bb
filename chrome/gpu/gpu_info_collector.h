// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_INFO_COLLECTOR_H__
#define CHROME_GPU_GPU_INFO_COLLECTOR_H__
#pragma once

#include "base/basictypes.h"
#include "build/build_config.h"
#include "content/common/gpu_info.h"

struct IDirect3D9;

namespace gpu_info_collector {

// Populate variables with necessary graphics card information.
// Returns true on success.
bool CollectGraphicsInfo(GPUInfo* gpu_info);

// Similar to CollectGraphicsInfo, only this collects a subset of variables
// without creating a GL/DirectX context (and without the danger of crashing).
// The subset each platform collects may be different.
bool CollectPreliminaryGraphicsInfo(GPUInfo* gpu_info);

#if defined(OS_WIN)
// Windows provides two ways of doing graphics so we need two ways of
// collecting info based on what's on a user's machine.
// The selection between the two methods is done in the cc file.

// A D3D argument is passed in for testing purposes
bool CollectGraphicsInfoD3D(IDirect3D9* d3d, GPUInfo* gpu_info);

// Collects D3D driver version/date through registry lookup.
// Note that this does not require a D3D context.
// device_id here is the raw data in DISPLAY_DEVICE.
bool CollectDriverInfoD3D(const std::wstring& device_id, GPUInfo* gpu_info);

// Collect the DirectX Disagnostics information about the attached displays.
bool GetDxDiagnostics(DxDiagNode* output);
#endif

// All platforms have a GL version for collecting information
bool CollectGraphicsInfoGL(GPUInfo* gpu_info);

// Collect GL and Shading language version information
bool CollectGLVersionInfo(GPUInfo* gpu_info);

// Platform specific method for collecting vendor and device ids
bool CollectVideoCardInfo(GPUInfo* gpu_info);

// Each platform stores the driver version on the GL_VERSION string differently
bool CollectDriverInfoGL(GPUInfo* gpu_info);

}  // namespace gpu_info_collector

#endif  // CHROME_GPU_GPU_INFO_COLLECTOR_H__
