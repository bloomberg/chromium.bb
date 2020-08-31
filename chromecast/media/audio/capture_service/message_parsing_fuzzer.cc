// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "chromecast/media/audio/capture_service/message_parsing_utils.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  PacketInfo info;
  chromecast::media::capture_service::ReadHeader(
      reinterpret_cast<const char*>(data), size, &info);
  return 0;
}
