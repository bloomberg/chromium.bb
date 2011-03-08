// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_info.h"

GPUInfo::GPUInfo()
    : level(kUninitialized),
      vendor_id(0),
      device_id(0),
      driver_vendor(""),
      driver_version(""),
      driver_date(""),
      pixel_shader_version(0),
      vertex_shader_version(0),
      gl_version(0),
      gl_version_string(""),
      gl_vendor(""),
      gl_renderer(""),
      gl_extensions(""),
      can_lose_context(false),
      collection_error(false) {
}

GPUInfo::~GPUInfo() {
}
