// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/common.h"

#include <iostream>

#include "blimp/common/proto/blimp_message.pb.h"
#include "net/base/ip_address_number.h"

namespace blimp {

const size_t kMaxPacketPayloadSizeBytes = 1 << 16;  // 64KB
const size_t kPacketHeaderSizeBytes = 4;

std::ostream& operator<<(std::ostream& out, const BlimpMessage& message) {
  // TODO(kmarshall): Look into including type-specific fields in the output.
  out << "<BlimpMessage type=" << message.type()
      << ", size=" << message.ByteSize() << ">";
  return out;
}

}  // namespace blimp
