// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_test_writer.h"

namespace net {
namespace test {

QuicTestWriter::QuicTestWriter() {}

QuicTestWriter::~QuicTestWriter() {}

QuicPacketWriter* QuicTestWriter::writer() {
  return writer_.get();
}

void QuicTestWriter::set_writer(QuicPacketWriter* writer) {
  writer_.reset(writer);
}

}  // namespace test
}  // namespace net
