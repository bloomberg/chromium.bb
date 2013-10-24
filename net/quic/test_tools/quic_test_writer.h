// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_TEST_WRITER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_TEST_WRITER_H_

#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_packet_writer.h"

namespace net {
namespace test {

// Allows setting a writer for a QuicConnection, to allow
// fine-grained control of writes.
class QuicTestWriter : public QuicPacketWriter {
 public:
  QuicTestWriter();
  virtual ~QuicTestWriter();
  // Takes ownership of |writer|.
  void set_writer(QuicPacketWriter* writer);

 protected:
  QuicPacketWriter* writer();

 private:
  scoped_ptr<QuicPacketWriter> writer_;
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_TEST_WRITER_H_
