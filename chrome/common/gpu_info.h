// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_INFO_H__
#define CHROME_COMMON_GPU_INFO_H__

// Provides access to the GPU information for the system
// on which chrome is currently running.

#include <string>

#include "base/basictypes.h"

class GPUInfo {
 public:
  GPUInfo();
  ~GPUInfo() {}

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

  // Populate variables with passed in values
  void SetGraphicsInfo(uint32 vendor_id, uint32 device_id,
                       const std::wstring& driver_version,
                       uint32 pixel_shader_version,
                       uint32 vertex_shader_version);
 private:
  uint32 vendor_id_;
  uint32 device_id_;
  std::wstring driver_version_;
  uint32 pixel_shader_version_;
  uint32 vertex_shader_version_;
};

#endif  // CHROME_COMMON_GPU_INFO_H__
