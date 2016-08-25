// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/chromium/mock_quic_data.h"

namespace net {
namespace test {

MockQuicData::MockQuicData() : sequence_number_(0) {}

MockQuicData::~MockQuicData() {}

void MockQuicData::AddSynchronousRead(
    std::unique_ptr<QuicEncryptedPacket> packet) {
  reads_.push_back(MockRead(SYNCHRONOUS, packet->data(), packet->length(),
                            sequence_number_++));
  packets_.push_back(std::move(packet));
}

void MockQuicData::AddRead(std::unique_ptr<QuicEncryptedPacket> packet) {
  reads_.push_back(
      MockRead(ASYNC, packet->data(), packet->length(), sequence_number_++));
  packets_.push_back(std::move(packet));
}

void MockQuicData::AddRead(IoMode mode, int rv) {
  reads_.push_back(MockRead(mode, rv, sequence_number_++));
}

void MockQuicData::AddWrite(std::unique_ptr<QuicEncryptedPacket> packet) {
  writes_.push_back(MockWrite(SYNCHRONOUS, packet->data(), packet->length(),
                              sequence_number_++));
  packets_.push_back(std::move(packet));
}

void MockQuicData::AddSocketDataToFactory(MockClientSocketFactory* factory) {
  MockRead* reads = reads_.empty() ? nullptr : &reads_[0];
  MockWrite* writes = writes_.empty() ? nullptr : &writes_[0];
  socket_data_.reset(
      new SequencedSocketData(reads, reads_.size(), writes, writes_.size()));
  factory->AddSocketDataProvider(socket_data_.get());
}

void MockQuicData::Resume() {
  socket_data_->Resume();
}

}  // namespace test
}  // namespace net
