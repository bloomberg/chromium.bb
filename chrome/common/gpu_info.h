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

  // Return the version of the graphics driver currently installed.
  // This will typically be something
  std::wstring driver_version() const;

  // Return the version of the pixel/fragment shader used by the gpu.
  // This will typically be a number less than 10 so storing as a float
  // should be okay.
  uint32 pixel_shader_version() const;

  // Return the version of the vertex shader used by the gpu.
  // This will typically be a number less than 10 so storing as a float
  // should be okay.
  uint32 vertex_shader_version() const;

  // Return the version of OpenGL we are using.
  // Major version in the high word, minor in the low word, eg version 2.5
  // would be 0x00020005.
  // Returns 0 if we're not using OpenGL, say because we're going through
  // D3D instead.
  uint32 gl_version() const;

  // Return the device semantics, i.e. whether the Vista and Windows 7 specific
  // semantics are available.
  bool can_lose_context() const;

  void SetProgress(Progress progress);

  void SetInitializationTime(const base::TimeDelta& initialization_time);

  // Populate variables with passed in values
  void SetGraphicsInfo(uint32 vendor_id, uint32 device_id,
                       const std::wstring& driver_version,
                       uint32 pixel_shader_version,
                       uint32 vertex_shader_version,
                       uint32 gl_version,
                       bool can_lose_context);

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
  std::wstring driver_version_;
  uint32 pixel_shader_version_;
  uint32 vertex_shader_version_;
  uint32 gl_version_;
  bool can_lose_context_;

#if defined(OS_WIN)
  DxDiagNode dx_diagnostics_;
#endif
};

#endif  // CHROME_COMMON_GPU_INFO_H__
