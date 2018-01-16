// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "device/u2f/register_response_data.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<uint8_t> input(data, data + size);
  auto response = device::RegisterResponseData::CreateFromU2fRegisterResponse(
      "https://google.com", input);
  return 0;
}
