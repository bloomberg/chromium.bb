// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_protocol.h"
#include "net/socket/socket_test_util.h"

namespace net {
namespace test {

// Helper class to encapsulate MockReads and MockWrites for QUIC.
// Simplify ownership issues and the interaction with the MockSocketFactory.
class MockQuicData {
 public:
  MockQuicData();
  ~MockQuicData();

  // Adds a synchronous read at the next sequence number which will read
  // |packet|.
  void AddSynchronousRead(std::unique_ptr<QuicEncryptedPacket> packet);

  // Adds an asynchronous read at the next sequence number which will read
  // |packet|.
  void AddRead(std::unique_ptr<QuicEncryptedPacket> packet);

  // Adds a read at the next sequence number which will return |rv| either
  // synchrously or asynchronously based on |mode|.
  void AddRead(IoMode mode, int rv);

  // Adds an asynchronous write at the next sequence number which will write
  // |packet|.
  void AddWrite(std::unique_ptr<QuicEncryptedPacket> packet);

  // Adds the reads and writes to |factory|.
  void AddSocketDataToFactory(MockClientSocketFactory* factory);

  // Resumes I/O after it is paused.
  void Resume();

 private:
  std::vector<std::unique_ptr<QuicEncryptedPacket>> packets_;
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  size_t sequence_number_;
  std::unique_ptr<SequencedSocketData> socket_data_;
};

}  // namespace test
}  // namespace net
