// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_create_command_buffer_config.h"

GPUCreateCommandBufferConfig::GPUCreateCommandBufferConfig() {}

GPUCreateCommandBufferConfig::GPUCreateCommandBufferConfig(
    const std::string& _allowed_extensions,
    const std::vector<int>& _attribs)
    : allowed_extensions(_allowed_extensions),
      attribs(_attribs) {
}

GPUCreateCommandBufferConfig::~GPUCreateCommandBufferConfig() {}
