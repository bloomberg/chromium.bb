// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/logging.h"

#include <iostream>

namespace gpu {

std::ostream& Logger::stream() {
  return std::cerr;
}

Logger::~Logger() {
  if (!condition_) {
    std::cerr << std::endl;
    std::cerr.flush();
    if (level_ == FATAL)
      assert(false);
  }
}

}  // namespace gpu
