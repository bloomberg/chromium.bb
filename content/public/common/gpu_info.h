// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_GPU_INFO_H_
#define CONTENT_PUBLIC_COMMON_GPU_INFO_H_

// Provides access to the GPU information for the system
// on which chrome is currently running.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/dx_diag_node.h"
#include "content/public/common/gpu_performance_stats.h"

namespace content {

struct CONTENT_EXPORT GPUInfo {
  struct CONTENT_EXPORT GPUDevice {
    GPUDevice();
    ~GPUDevice();

    // The DWORD (uint32) representing the graphics card vendor id.
    uint32 vendor_id;

    // The DWORD (uint32) representing the graphics card device id.
    // Device ids are unique to vendor, not to one another.
    uint32 device_id;

    // The strings that describe the GPU.
    // In Linux these strings are obtained through libpci.
    // In Win/MacOSX, these two strings are not filled at the moment.
    std::string vendor_string;
    std::string device_string;
  };

  GPUInfo();
  ~GPUInfo();

  // Whether more GPUInfo fields might be collected in the future.
  bool finalized;

  // The amount of time taken to get from the process starting to the message
  // loop being pumped.
  base::TimeDelta initialization_time;

  // Computer has NVIDIA Optimus
  bool optimus;

  // Computer has AMD Dynamic Switchable Graphics
  bool amd_switchable;

  // Primary GPU, for exmaple, the discrete GPU in a dual GPU machine.
  GPUDevice gpu;

  // Secondary GPUs, for example, the integrated GPU in a dual GPU machine.
  std::vector<GPUDevice> secondary_gpus;

  // The vendor of the graphics driver currently installed.
  std::string driver_vendor;

  // The version of the graphics driver currently installed.
  std::string driver_version;

  // The date of the graphics driver currently installed.
  std::string driver_date;

  // The version of the pixel/fragment shader used by the gpu.
  std::string pixel_shader_version;

  // The version of the vertex shader used by the gpu.
  std::string vertex_shader_version;

  // The version of OpenGL we are using.
  // TODO(zmo): should be able to tell if it's GL or GLES.
  std::string gl_version;

  // The GL_VERSION string.  "" if we are not using OpenGL.
  std::string gl_version_string;

  // The GL_VENDOR string.  "" if we are not using OpenGL.
  std::string gl_vendor;

  // The GL_RENDERER string.  "" if we are not using OpenGL.
  std::string gl_renderer;

  // The GL_EXTENSIONS string.  "" if we are not using OpenGL.
  std::string gl_extensions;

  // The device semantics, i.e. whether the Vista and Windows 7 specific
  // semantics are available.
  bool can_lose_context;

  // Whether gpu or driver is accessible.
  bool gpu_accessible;

  // By default all values are 0.
  GpuPerformanceStats performance_stats;

  bool software_rendering;

  // Whether the gpu process is running in a sandbox.
  bool sandboxed;

#if defined(OS_WIN)
  // The information returned by the DirectX Diagnostics Tool.
  DxDiagNode dx_diagnostics;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_GPU_INFO_H_
