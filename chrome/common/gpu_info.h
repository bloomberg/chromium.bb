// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
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

class GPUInfo {
 public:
  GPUInfo();
  ~GPUInfo() {}

  enum Progress {
    kUninitialized,
    kPartial,
    kComplete,
  };

  // Returns whether this GPUInfo has been partially or fully initialized with
  // information.
  Progress progress() const;

  // The amount of time taken to get from the process starting to the message
  // loop being pumped.
  base::TimeDelta initialization_time() const;

  // Return the DWORD (uint32) representing the graphics card vendor id.
  uint32 vendor_id() const;

  // Return the DWORD (uint32) representing the graphics card device id.
  // Device ids are unique to vendor, not to one another.
  uint32 device_id() const;

  // Return the vendor of the graphics driver currently installed.
  std::string driver_vendor() const;

  // Return the version of the graphics driver currently installed.
  std::string driver_version() const;

  // Return the version of the pixel/fragment shader used by the gpu.
  // Major version in the second lowest 8 bits, minor in the lowest 8 bits,
  // eg version 2.5 would be 0x00000205.
  uint32 pixel_shader_version() const;

  // Return the version of the vertex shader used by the gpu.
  // Major version in the second lowest 8 bits, minor in the lowest 8 bits,
  // eg version 2.5 would be 0x00000205.
  uint32 vertex_shader_version() const;

  // Return the version of OpenGL we are using.
  // Major version in the second lowest 8 bits, minor in the lowest 8 bits,
  // eg version 2.5 would be 0x00000205.
  // Returns 0 if we're not using OpenGL, say because we're going through
  // D3D instead.
  // TODO(zmo): should be able to tell if it's GL or GLES.
  uint32 gl_version() const;

  // Return the GL_VERSION string.
  // Return "" if we are not using OpenGL.
  std::string gl_version_string() const;

  // Return the GL_VENDOR string.
  // Return "" if we are not using OpenGL.
  std::string gl_vendor() const;

  // Return the GL_RENDERER string.
  // Return "" if we are not using OpenGL.
  std::string gl_renderer() const;

  // Return the device semantics, i.e. whether the Vista and Windows 7 specific
  // semantics are available.
  bool can_lose_context() const;

  void SetProgress(Progress progress);

  void SetInitializationTime(const base::TimeDelta& initialization_time);

  void SetVideoCardInfo(uint32 vendor_id, uint32 device_id);

  void SetDriverInfo(const std::string& driver_vendor,
                     const std::string& driver_version);

  void SetShaderVersion(uint32 pixel_shader_version,
                        uint32 vertex_shader_version);

  void SetGLVersion(uint32 gl_version);

  void SetGLVersionString(const std::string& gl_vendor_string);

  void SetGLVendor(const std::string& gl_vendor);

  void SetGLRenderer(const std::string& gl_renderer);

  void SetCanLoseContext(bool can_lose_context);

#if defined(OS_WIN)
  // The information returned by the DirectX Diagnostics Tool.
  const DxDiagNode& dx_diagnostics() const;

  void SetDxDiagnostics(const DxDiagNode& dx_diagnostics);
#endif

 private:
  Progress progress_;
  base::TimeDelta initialization_time_;
  uint32 vendor_id_;
  uint32 device_id_;
  std::string driver_vendor_;
  std::string driver_version_;
  uint32 pixel_shader_version_;
  uint32 vertex_shader_version_;
  uint32 gl_version_;
  std::string gl_version_string_;
  std::string gl_vendor_;
  std::string gl_renderer_;
  bool can_lose_context_;

#if defined(OS_WIN)
  DxDiagNode dx_diagnostics_;
#endif
};

#endif  // CHROME_COMMON_GPU_INFO_H__
