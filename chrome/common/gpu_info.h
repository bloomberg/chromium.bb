// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_INFO_H__
#define CHROME_COMMON_GPU_INFO_H__
#pragma once

// Provides access to the GPU information for the system
// on which chrome is currently running.

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
#include "chrome/common/dx_diag_node.h"

struct GPUInfo {
  GPUInfo();
  ~GPUInfo();

  enum Level {
    kUninitialized,
    kPreliminary,
    kPartial,
    kCompleting,
    kComplete,
  };

  // Whether this GPUInfo has been partially or fully initialized with
  // information.
  Level level;

  // The amount of time taken to get from the process starting to the message
  // loop being pumped.
  base::TimeDelta initialization_time;

  // The DWORD (uint32) representing the graphics card vendor id.
  uint32 vendor_id;

  // The DWORD (uint32) representing the graphics card device id.  Device ids
  // are unique to vendor, not to one another.
  uint32 device_id;

  // The vendor of the graphics driver currently installed.
  std::string driver_vendor;

  // The version of the graphics driver currently installed.
  std::string driver_version;

  // The date of the graphics driver currently installed.
  std::string driver_date;

  // The version of the pixel/fragment shader used by the gpu.  Major version in
  // the second lowest 8 bits, minor in the lowest 8 bits, eg version 2.5 would
  // be 0x00000205.
  uint32 pixel_shader_version;

  // The version of the vertex shader used by the gpu.  Major version in the
  // second lowest 8 bits, minor in the lowest 8 bits, eg version 2.5 would be
  // 0x00000205.
  uint32 vertex_shader_version;

  // The version of OpenGL we are using.
  // Major version in the second lowest 8 bits, minor in the lowest 8 bits,
  // eg version 2.5 would be 0x00000205.
  // Returns 0 if we're not using OpenGL, say because we're going through
  // D3D instead.
  // TODO(zmo): should be able to tell if it's GL or GLES.
  uint32 gl_version;

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

  // True if there was an error at any stage of collecting GPUInfo data.
  // If there was an error, then the GPUInfo fields may be incomplete or set
  // to default values such as 0 or empty string.
  bool collection_error;

#if defined(OS_WIN)
  // The information returned by the DirectX Diagnostics Tool.
  DxDiagNode dx_diagnostics;
#endif
};

#endif  // CHROME_COMMON_GPU_INFO_H__
