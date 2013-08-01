// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_INFO_H_
#define GPU_CONFIG_GPU_INFO_H_

// Provides access to the GPU information for the system
// on which chrome is currently running.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "gpu/config/dx_diag_node.h"
#include "gpu/config/gpu_performance_stats.h"
#include "gpu/gpu_export.h"

namespace gpu {

struct GPU_EXPORT GPUInfo {
  struct GPU_EXPORT GPUDevice {
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

  // Lenovo dCute is installed. http://crbug.com/181665.
  bool lenovo_dcute;

  // Version of DisplayLink driver installed. Zero if not installed.
  // http://crbug.com/177611.
  Version display_link_version;

  // Primary GPU, for exmaple, the discrete GPU in a dual GPU machine.
  GPUDevice gpu;

  // Secondary GPUs, for example, the integrated GPU in a dual GPU machine.
  std::vector<GPUDevice> secondary_gpus;

  // On Windows, the unique identifier of the adapter the GPU process uses.
  // The default is zero, which makes the browser process create its D3D device
  // on the primary adapter. Note that the primary adapter can change at any
  // time so it is better to specify a particular LUID. Note that valid LUIDs
  // are always non-zero.
  uint64 adapter_luid;

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

  // The machine model identifier with format "name major.minor".
  // Name should not contain any whitespaces.
  std::string machine_model;

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

  // GL window system binding vendor.  "" if not available.
  std::string gl_ws_vendor;

  // GL window system binding version.  "" if not available.
  std::string gl_ws_version;

  // GL window system binding extensions.  "" if not available.
  std::string gl_ws_extensions;

  // GL reset notification strategy as defined by GL_ARB_robustness. 0 if GPU
  // reset detection or notification not available.
  uint32 gl_reset_notification_strategy;

  // The device semantics, i.e. whether the Vista and Windows 7 specific
  // semantics are available.
  bool can_lose_context;

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

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_INFO_H_
