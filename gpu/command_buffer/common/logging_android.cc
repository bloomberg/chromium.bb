// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/log.h>
#include <iostream>
#include <sstream>

#include "gpu/command_buffer/common/logging.h"

namespace gpu {

namespace {
std::stringstream* g_log;
const char* kLogTag = "chromium-gpu";
}

std::ostream& Logger::stream() {
  if (!g_log)
    g_log = new std::stringstream();
  return *g_log;
}

Logger::~Logger() {
  if (!condition_) {
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "%s", g_log->str().c_str());
    g_log->str(std::string());
    if (level_ == FATAL)
      assert(false);
  }
}

}  // namespace gpu
