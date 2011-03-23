// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu_info.h"

GPUInfo::GPUInfo()
    : finalized(false),
      vendor_id(0),
      device_id(0),
      can_lose_context(false) {
}

GPUInfo::~GPUInfo() { }

bool GPUInfo::Merge(const GPUInfo& other) {
  if (device_id != other.device_id || vendor_id != other.vendor_id) {
    *this = other;
    return true;
  }

  bool changed = false;
  if (!finalized) {
    finalized = other.finalized;
    initialization_time = other.initialization_time;
    if (driver_vendor.empty() && !other.driver_vendor.empty()) {
      driver_vendor = other.driver_vendor;
      changed = true;
    }
    if (driver_version.empty() && !other.driver_version.empty()) {
      driver_version = other.driver_version;
      changed = true;
    }
    if (driver_date.empty() && !other.driver_date.empty()) {
      driver_date = other.driver_date;
      changed = true;
    }
    if (pixel_shader_version.empty())
      pixel_shader_version = other.pixel_shader_version;
    if (vertex_shader_version.empty())
      vertex_shader_version = other.vertex_shader_version;
    if (gl_version.empty())
      gl_version = other.gl_version;
    if (gl_version_string.empty())
      gl_version_string = other.gl_version_string;
    if (gl_vendor.empty())
      gl_vendor = other.gl_vendor;
    if (gl_renderer.empty() && !other.gl_renderer.empty()) {
      gl_renderer = other.gl_renderer;
      changed = true;
    }
    if (gl_extensions.empty())
      gl_extensions = other.gl_extensions;
    can_lose_context = other.can_lose_context;
#if defined(OS_WIN)
    if (dx_diagnostics.values.size() == 0 &&
        dx_diagnostics.children.size() == 0)
      dx_diagnostics = other.dx_diagnostics;
#endif
  }
  return changed;
}

