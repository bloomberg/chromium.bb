// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_info.h"

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

    if (driver_vendor.empty()) {
      changed |= driver_vendor != other.driver_vendor;
      driver_vendor = other.driver_vendor;
    }
    if (driver_version.empty()) {
      changed |= driver_version != other.driver_version;
      driver_version = other.driver_version;
    }
    if (driver_date.empty()) {
      changed |= driver_date != other.driver_date;
      driver_date = other.driver_date;
    }
    if (pixel_shader_version.empty()) {
      changed |= pixel_shader_version != other.pixel_shader_version;
      pixel_shader_version = other.pixel_shader_version;
    }
    if (vertex_shader_version.empty()) {
      changed |= vertex_shader_version != other.vertex_shader_version;
      vertex_shader_version = other.vertex_shader_version;
    }
    if (gl_version.empty()) {
      changed |= gl_version != other.gl_version;
      gl_version = other.gl_version;
    }
    if (gl_version_string.empty()) {
      changed |= gl_version_string != other.gl_version_string;
      gl_version_string = other.gl_version_string;
    }
    if (gl_vendor.empty()) {
      changed |= gl_vendor != other.gl_vendor;
      gl_vendor = other.gl_vendor;
    }
    if (gl_renderer.empty()) {
      changed |= gl_renderer != other.gl_renderer;
      gl_renderer = other.gl_renderer;
    }
    if (gl_extensions.empty()) {
      changed |= gl_extensions != other.gl_extensions;
      gl_extensions = other.gl_extensions;
    }
    can_lose_context = other.can_lose_context;
#if defined(OS_WIN)
    if (dx_diagnostics.values.size() == 0 &&
      dx_diagnostics.children.size() == 0) {
      dx_diagnostics = other.dx_diagnostics;
      changed = true;
    }
#endif
  }
  return changed;
}
