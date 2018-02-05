// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <algorithm>

#include "device/fido/u2f_apdu_command.h"
#include "device/fido/u2f_apdu_response.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<uint8_t> input(data, data + size);
  std::unique_ptr<device::U2fApduCommand> cmd =
      device::U2fApduCommand::CreateFromMessage(input);
  std::unique_ptr<device::U2fApduResponse> rsp =
      device::U2fApduResponse::CreateFromMessage(input);
  return 0;
}
