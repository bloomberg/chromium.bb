// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2f_apdu_response.h"

namespace device {
scoped_refptr<U2fApduResponse> U2fApduResponse::CreateFromMessage(
    const std::vector<uint8_t>& message) {
  uint16_t status_bytes;
  Status response_status;

  // Invalid message size, data is appended by status byte
  if (message.size() < 2)
    return nullptr;
  status_bytes = message[message.size() - 2] << 8;
  status_bytes |= message[message.size() - 1];
  response_status = static_cast<Status>(status_bytes);
  std::vector<uint8_t> data(message.begin(), message.end() - 2);

  return make_scoped_refptr(
      new U2fApduResponse(std::move(data), response_status));
}

U2fApduResponse::U2fApduResponse(std::vector<uint8_t> message,
                                 Status response_status)
    : response_status_(response_status), data_(std::move(message)) {}

U2fApduResponse::~U2fApduResponse() {}

}  // namespace device
