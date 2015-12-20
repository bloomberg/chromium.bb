// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/test_common.h"

#include <string>

#include "base/sys_byteorder.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/common.h"
#include "net/base/io_buffer.h"

namespace blimp {

MockStreamSocket::MockStreamSocket() {}

MockStreamSocket::~MockStreamSocket() {}

MockTransport::MockTransport() {}

MockTransport::~MockTransport() {}

scoped_ptr<BlimpConnection> MockTransport::TakeConnection() {
  return make_scoped_ptr(TakeConnectionPtr());
}

const std::string MockTransport::GetName() const {
  return "mock";
}

MockConnectionHandler::MockConnectionHandler() {}

MockConnectionHandler::~MockConnectionHandler() {}

void MockConnectionHandler::HandleConnection(
    scoped_ptr<BlimpConnection> connection) {
  HandleConnectionPtr(connection.get());
}

MockPacketReader::MockPacketReader() {}

MockPacketReader::~MockPacketReader() {}

MockPacketWriter::MockPacketWriter() {}

MockPacketWriter::~MockPacketWriter() {}

MockBlimpConnection::MockBlimpConnection() {}

MockBlimpConnection::~MockBlimpConnection() {}

MockConnectionErrorObserver::MockConnectionErrorObserver() {}

MockConnectionErrorObserver::~MockConnectionErrorObserver() {}

MockBlimpMessageProcessor::MockBlimpMessageProcessor() {}

MockBlimpMessageProcessor::~MockBlimpMessageProcessor() {}

void MockBlimpMessageProcessor::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  MockableProcessMessage(*message, callback);
}

std::string EncodeHeader(size_t size) {
  scoped_ptr<char[]> serialized(new char[kPacketHeaderSizeBytes]);
  uint32_t net_size = base::HostToNet32(size);
  memcpy(serialized.get(), &net_size, sizeof(net_size));
  return std::string(serialized.get(), kPacketHeaderSizeBytes);
}

bool BufferStartsWith(net::GrowableIOBuffer* buf, const std::string& str) {
  return (static_cast<size_t>(buf->capacity()) > str.size() &&
          str == std::string(buf->StartOfBuffer(), str.size()));
}

}  // namespace blimp
