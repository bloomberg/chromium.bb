// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_info.h"

GPUInfo::GPUInfo()
    : vendor_id_(0), device_id_(0), driver_version_(L""),
      pixel_shader_version_(0), vertex_shader_version_(0) {
}

uint32 GPUInfo::vendor_id() const {
  return vendor_id_;
}

uint32 GPUInfo::device_id() const {
  return device_id_;
}

std::wstring GPUInfo::driver_version() const {
  return driver_version_;
}

uint32 GPUInfo::pixel_shader_version() const {
  return pixel_shader_version_;
}

uint32 GPUInfo::vertex_shader_version() const {
  return vertex_shader_version_;
}

void GPUInfo::SetGraphicsInfo(uint32 vendor_id, uint32 device_id,
                              const std::wstring& driver_version,
                              uint32 pixel_shader_version,
                              uint32 vertex_shader_version) {
  vendor_id_ = vendor_id;
  device_id_ = device_id;
  driver_version_ = driver_version;
  pixel_shader_version_ = pixel_shader_version;
  vertex_shader_version_ = vertex_shader_version;
}
