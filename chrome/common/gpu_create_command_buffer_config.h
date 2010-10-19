// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_CREATE_COMMAND_BUFFER_CONFIG_H_
#define CHROME_COMMON_GPU_CREATE_COMMAND_BUFFER_CONFIG_H_
#pragma once

#include <string>
#include <vector>

// Parameters passed when initializing a GPU channel.
struct GPUCreateCommandBufferConfig {
  GPUCreateCommandBufferConfig() { }

  GPUCreateCommandBufferConfig(
      const std::string& _allowed_extensions,
      const std::vector<int>& _attribs)
      : allowed_extensions(_allowed_extensions),
        attribs(_attribs) {
  }

  std::string allowed_extensions;
  std::vector<int> attribs;
};

#endif  // CHROME_COMMON_GPU_CREATE_COMMAND_BUFFER_CONFIG_H_
