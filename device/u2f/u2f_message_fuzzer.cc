// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include "net/base/io_buffer.h"
#include "u2f_message.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  size_t packet_size = 65;
  size_t remaining_buffer = size;
  uint8_t* start = const_cast<uint8_t*>(data);

  scoped_refptr<net::IOBufferWithSize> buf(
      new net::IOBufferWithSize(packet_size));
  memcpy(buf->data(), start, std::min(packet_size, remaining_buffer));
  scoped_refptr<device::U2fMessage> msg =
      device::U2fMessage::CreateFromSerializedData(buf);

  remaining_buffer -= std::min(remaining_buffer, packet_size);
  start += packet_size;

  while (remaining_buffer > 0) {
    size_t buffer_size = std::min(packet_size, remaining_buffer);
    scoped_refptr<net::IOBufferWithSize> tmp_buf(
        new net::IOBufferWithSize(buffer_size));
    memcpy(tmp_buf->data(), start, buffer_size);
    msg->AddContinuationPacket(tmp_buf);
    remaining_buffer -= std::min(remaining_buffer, buffer_size);
    start += buffer_size;
  }

  return 0;
}
