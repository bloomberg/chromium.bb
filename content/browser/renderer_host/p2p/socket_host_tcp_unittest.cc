// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_tcp.h"

#include <deque>

#include "base/sys_byteorder.h"
#include "content/browser/renderer_host/p2p/socket_host_test_utils.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Return;

namespace content {

class P2PSocketHostTcpTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(sender_, Send(
        MatchMessage(static_cast<uint32>(P2PMsg_OnSocketCreated::ID))))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

    socket_host_.reset(new P2PSocketHostTcp(&sender_, 0));
    socket_ = new FakeSocket(&sent_data_);
    socket_->SetLocalAddress(ParseAddress(kTestLocalIpAddress, kTestPort1));
    socket_host_->socket_.reset(socket_);

    dest_ = ParseAddress(kTestIpAddress1, kTestPort1);

    local_address_ = ParseAddress(kTestLocalIpAddress, kTestPort1);

    socket_host_->remote_address_ = dest_;
    socket_host_->state_ = P2PSocketHost::STATE_CONNECTING;
    socket_host_->OnConnected(net::OK);
  }

  std::string IntToSize(int size) {
    std::string result;
    uint16 size16 = base::HostToNet16(size);
    result.resize(sizeof(size16));
    memcpy(&result[0], &size16, sizeof(size16));
    return result;
  }

  std::string sent_data_;
  FakeSocket* socket_; // Owned by |socket_host_|.
  scoped_ptr<P2PSocketHostTcp> socket_host_;
  MockIPCSender sender_;

  net::IPEndPoint local_address_;

  net::IPEndPoint dest_;
  net::IPEndPoint dest2_;
};

// Verify that we can send STUN message and that they are formatted
// properly.
TEST_F(P2PSocketHostTcpTest, SendStunNoAuth) {
  std::vector<char> packet1;
  CreateStunRequest(&packet1);
  socket_host_->Send(dest_, packet1);

  std::vector<char> packet2;
  CreateStunResponse(&packet2);
  socket_host_->Send(dest_, packet2);

  std::vector<char> packet3;
  CreateStunError(&packet3);
  socket_host_->Send(dest_, packet3);

  std::string expected_data;
  expected_data.append(IntToSize(packet1.size()));
  expected_data.append(packet1.begin(), packet1.end());
  expected_data.append(IntToSize(packet2.size()));
  expected_data.append(packet2.begin(), packet2.end());
  expected_data.append(IntToSize(packet3.size()));
  expected_data.append(packet3.begin(), packet3.end());

  EXPECT_EQ(expected_data, sent_data_);
}

// Verify that we can receive STUN messages from the socket, and that
// the messages are parsed properly.
TEST_F(P2PSocketHostTcpTest, ReceiveStun) {
  std::vector<char> packet1;
  CreateStunRequest(&packet1);
  socket_host_->Send(dest_, packet1);

  std::vector<char> packet2;
  CreateStunResponse(&packet2);
  socket_host_->Send(dest_, packet2);

  std::vector<char> packet3;
  CreateStunError(&packet3);
  socket_host_->Send(dest_, packet3);

  std::string received_data;
  received_data.append(IntToSize(packet1.size()));
  received_data.append(packet1.begin(), packet1.end());
  received_data.append(IntToSize(packet2.size()));
  received_data.append(packet2.begin(), packet2.end());
  received_data.append(IntToSize(packet3.size()));
  received_data.append(packet3.begin(), packet3.end());

  EXPECT_CALL(sender_, Send(MatchPacketMessage(packet1)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  EXPECT_CALL(sender_, Send(MatchPacketMessage(packet2)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  EXPECT_CALL(sender_, Send(MatchPacketMessage(packet3)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  size_t pos = 0;
  size_t step_sizes[] = {3, 2, 1};
  size_t step = 0;
  while (pos < received_data.size()) {
    size_t step_size = std::min(step_sizes[step], received_data.size() - pos);
    socket_->AppendInputData(&received_data[pos], step_size);
    pos += step_size;
    if (++step >= arraysize(step_sizes))
      step = 0;
  }
}

// Verify that we can't send data before we've received STUN response
// from the other side.
TEST_F(P2PSocketHostTcpTest, SendDataNoAuth) {
  EXPECT_CALL(sender_, Send(
      MatchMessage(static_cast<uint32>(P2PMsg_OnError::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  std::vector<char> packet;
  CreateRandomPacket(&packet);
  socket_host_->Send(dest_, packet);

  EXPECT_EQ(0U, sent_data_.size());
}

// Verify that we can send data after we've received STUN response
// from the other side.
TEST_F(P2PSocketHostTcpTest, SendAfterStunRequest) {
  // Receive packet from |dest_|.
  std::vector<char> request_packet;
  CreateStunRequest(&request_packet);

  std::string received_data;
  received_data.append(IntToSize(request_packet.size()));
  received_data.append(request_packet.begin(), request_packet.end());

  EXPECT_CALL(sender_, Send(MatchPacketMessage(request_packet)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  socket_->AppendInputData(&received_data[0], received_data.size());

  // Now we should be able to send any data to |dest_|.
  std::vector<char> packet;
  CreateRandomPacket(&packet);
  socket_host_->Send(dest_, packet);

  std::string expected_data;
  expected_data.append(IntToSize(packet.size()));
  expected_data.append(packet.begin(), packet.end());

  EXPECT_EQ(expected_data, sent_data_);
}

}  // namespace content
