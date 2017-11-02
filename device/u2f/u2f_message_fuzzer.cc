// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <vector>
#include "net/base/io_buffer.h"
#include "u2f_message.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  size_t packet_size = 64;
  size_t remaining_buffer = size;
  const uint8_t* start = data;

  std::vector<uint8_t> buf(start,
                           start + std::min(packet_size, remaining_buffer));
  std::unique_ptr<device::U2fMessage> msg =
      device::U2fMessage::CreateFromSerializedData(buf);

  remaining_buffer -= std::min(remaining_buffer, packet_size);
  start += packet_size;

  while (remaining_buffer > 0) {
    size_t buffer_size = std::min(packet_size, remaining_buffer);
    std::vector<uint8_t> tmp_buf(start, start + buffer_size);
    msg->AddContinuationPacket(tmp_buf);
    remaining_buffer -= std::min(remaining_buffer, buffer_size);
    start += buffer_size;
  }

  return 0;
}
