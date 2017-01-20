// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2f_apdu_command.h"

namespace device {

scoped_refptr<U2fApduCommand> U2fApduCommand::CreateFromMessage(
    const std::vector<uint8_t>& message) {
  uint16_t data_length = 0;
  size_t index = 0, response_length = 0;
  std::vector<uint8_t> data;

  if (message.size() < kApduMinHeader || message.size() > kApduMaxLength)
    return nullptr;
  uint8_t cla = message[index++];
  uint8_t ins = message[index++];
  uint8_t p1 = message[index++];
  uint8_t p2 = message[index++];

  switch (message.size()) {
    // No data present; no expected response
    case kApduMinHeader:
      break;
    // Invalid encoding sizes
    case kApduMinHeader + 1:
    case kApduMinHeader + 2:
      return nullptr;
    // No data present; response expected
    case kApduMinHeader + 3:
      // Fifth byte must be 0
      if (message[index++] != 0)
        return nullptr;
      response_length = message[index++] << 8;
      response_length |= message[index++];
      // Special case where response length of 0x0000 corresponds to 65536
      // Defined in ISO7816-4
      if (response_length == 0)
        response_length = kApduMaxResponseLength;
      break;
    default:
      // Fifth byte must be 0
      if (message[index++] != 0)
        return nullptr;
      data_length = message[index++] << 8;
      data_length |= message[index++];

      if (message.size() == data_length + index) {
        // No response expected
        data.insert(data.end(), message.begin() + index, message.end());
      } else if (message.size() == data_length + index + 2) {
        // Maximum response size is stored in final 2 bytes
        data.insert(data.end(), message.begin() + index, message.end() - 2);
        index += data_length;
        response_length = message[index++] << 8;
        response_length |= message[index++];
        // Special case where response length of 0x0000 corresponds to 65536
        // Defined in ISO7816-4
        if (response_length == 0)
          response_length = kApduMaxResponseLength;
      } else {
        return nullptr;
      }
      break;
  }

  return make_scoped_refptr(
      new U2fApduCommand(cla, ins, p1, p2, response_length, std::move(data)));
}

// static
scoped_refptr<U2fApduCommand> U2fApduCommand::Create() {
  return make_scoped_refptr(new U2fApduCommand());
}

std::vector<uint8_t> U2fApduCommand::GetEncodedCommand() const {
  std::vector<uint8_t> encoded = {cla_, ins_, p1_, p2_};

  // If data exists, request size (Lc) is encoded in 3 bytes, with the first
  // byte always being null, and the other two bytes being a big-endian
  // representation of the request size. If data length is 0, response size (Le)
  // will be prepended with a null byte.
  if (data_.size() > 0) {
    size_t data_length = data_.size();

    encoded.push_back(0x0);
    if (data_length > kApduMaxDataLength)
      data_length = kApduMaxDataLength;
    encoded.push_back((data_length >> 8) & 0xff);
    encoded.push_back(data_length & 0xff);
    encoded.insert(encoded.end(), data_.begin(), data_.begin() + data_length);
  } else if (response_length_ > 0) {
    encoded.push_back(0x0);
  }

  if (response_length_ > 0) {
    encoded.push_back((response_length_ >> 8) & 0xff);
    encoded.push_back(response_length_ & 0xff);
  }
  return encoded;
}

U2fApduCommand::U2fApduCommand()
    : cla_(0), ins_(0), p1_(0), p2_(0), response_length_(0) {}

U2fApduCommand::U2fApduCommand(uint8_t cla,
                               uint8_t ins,
                               uint8_t p1,
                               uint8_t p2,
                               size_t response_length,
                               std::vector<uint8_t> data)
    : cla_(cla),
      ins_(ins),
      p1_(p1),
      p2_(p2),
      response_length_(response_length),
      data_(std::move(data)) {}

U2fApduCommand::~U2fApduCommand() {}

}  // namespace device
