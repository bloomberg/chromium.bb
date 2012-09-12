// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_udp.h"

#include <deque>
#include <vector>

#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "content/browser/renderer_host/p2p/socket_host_test_utils.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/udp/datagram_server_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Return;

namespace {

class FakeDatagramServerSocket : public net::DatagramServerSocket {
 public:
  typedef std::pair<net::IPEndPoint, std::vector<char> > UDPPacket;

  // P2PSocketHostUdp destroyes a socket on errors so sent packets
  // need to be stored outside of this object.
  explicit FakeDatagramServerSocket(std::deque<UDPPacket>* sent_packets)
      : sent_packets_(sent_packets) {
  }

  virtual void Close() OVERRIDE {
  }

  virtual int GetPeerAddress(net::IPEndPoint* address) const OVERRIDE {
    NOTREACHED();
    return net::ERR_SOCKET_NOT_CONNECTED;
  }

  virtual int GetLocalAddress(net::IPEndPoint* address) const OVERRIDE {
    *address = address_;
    return 0;
  }

  virtual int Listen(const net::IPEndPoint& address) OVERRIDE {
    address_ = address;
    return 0;
  }

  virtual int RecvFrom(net::IOBuffer* buf, int buf_len,
                       net::IPEndPoint* address,
                       const net::CompletionCallback& callback) OVERRIDE {
    CHECK(recv_callback_.is_null());
    if (incoming_packets_.size() > 0) {
      scoped_refptr<net::IOBuffer> buffer(buf);
      int size = std::min(
          static_cast<int>(incoming_packets_.front().second.size()), buf_len);
      memcpy(buffer->data(), &*incoming_packets_.front().second.begin(), size);
      *address = incoming_packets_.front().first;
      incoming_packets_.pop_front();
      return size;
    } else {
      recv_callback_ = callback;
      recv_buffer_ = buf;
      recv_size_ = buf_len;
      recv_address_ = address;
      return net::ERR_IO_PENDING;
    }
  }

  virtual int SendTo(net::IOBuffer* buf, int buf_len,
                     const net::IPEndPoint& address,
                     const net::CompletionCallback& callback) OVERRIDE {
    scoped_refptr<net::IOBuffer> buffer(buf);
    std::vector<char> data_vector(buffer->data(), buffer->data() + buf_len);
    sent_packets_->push_back(UDPPacket(address, data_vector));
    return buf_len;
  }

  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE {
    return true;
  }

  virtual bool SetSendBufferSize(int32 size) OVERRIDE {
    return true;
  }

  void ReceivePacket(const net::IPEndPoint& address, std::vector<char> data) {
    if (!recv_callback_.is_null()) {
      int size = std::min(recv_size_, static_cast<int>(data.size()));
      memcpy(recv_buffer_->data(), &*data.begin(), size);
      *recv_address_ = address;
      net::CompletionCallback cb = recv_callback_;
      recv_callback_.Reset();
      recv_buffer_ = NULL;
      cb.Run(size);
    } else {
      incoming_packets_.push_back(UDPPacket(address, data));
    }
  }

  virtual const net::BoundNetLog& NetLog() const {
    return net_log_;
  }

  virtual void AllowAddressReuse() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void AllowBroadcast() OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  net::IPEndPoint address_;
  std::deque<UDPPacket>* sent_packets_;
  std::deque<UDPPacket> incoming_packets_;
  net::BoundNetLog net_log_;

  scoped_refptr<net::IOBuffer> recv_buffer_;
  net::IPEndPoint* recv_address_;
  int recv_size_;
  net::CompletionCallback recv_callback_;
};

}  // namespace

namespace content {

class P2PSocketHostUdpTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(sender_, Send(
        MatchMessage(static_cast<uint32>(P2PMsg_OnSocketCreated::ID))))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

    socket_host_.reset(new P2PSocketHostUdp(&sender_, 0));
    socket_ = new FakeDatagramServerSocket(&sent_packets_);
    socket_host_->socket_.reset(socket_);

    local_address_ = ParseAddress(kTestLocalIpAddress, kTestPort1);
    socket_host_->Init(local_address_, net::IPEndPoint());

    dest1_ = ParseAddress(kTestIpAddress1, kTestPort1);
    dest2_ = ParseAddress(kTestIpAddress2, kTestPort2);
  }

  std::deque<FakeDatagramServerSocket::UDPPacket> sent_packets_;
  FakeDatagramServerSocket* socket_; // Owned by |socket_host_|.
  scoped_ptr<P2PSocketHostUdp> socket_host_;
  MockIPCSender sender_;

  net::IPEndPoint local_address_;

  net::IPEndPoint dest1_;
  net::IPEndPoint dest2_;
};

// Verify that we can send STUN messages before we receive anything
// from the other side.
TEST_F(P2PSocketHostUdpTest, SendStunNoAuth) {
  std::vector<char> packet1;
  CreateStunRequest(&packet1);
  socket_host_->Send(dest1_, packet1);

  std::vector<char> packet2;
  CreateStunResponse(&packet2);
  socket_host_->Send(dest1_, packet2);

  std::vector<char> packet3;
  CreateStunError(&packet3);
  socket_host_->Send(dest1_, packet3);

  ASSERT_EQ(sent_packets_.size(), 3U);
  ASSERT_EQ(sent_packets_[0].second, packet1);
  ASSERT_EQ(sent_packets_[1].second, packet2);
  ASSERT_EQ(sent_packets_[2].second, packet3);
}

// Verify that no data packets can be sent before STUN binding has
// finished.
TEST_F(P2PSocketHostUdpTest, SendDataNoAuth) {
  EXPECT_CALL(sender_, Send(
      MatchMessage(static_cast<uint32>(P2PMsg_OnError::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  std::vector<char> packet;
  CreateRandomPacket(&packet);
  socket_host_->Send(dest1_, packet);

  ASSERT_EQ(sent_packets_.size(), 0U);
}

// Verify that we can send data after we've received STUN request
// from the other side.
TEST_F(P2PSocketHostUdpTest, SendAfterStunRequest) {
  // Receive packet from |dest1_|.
  std::vector<char> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(sender_, Send(MatchPacketMessage(request_packet)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  socket_->ReceivePacket(dest1_, request_packet);

  // Now we should be able to send any data to |dest1_|.
  std::vector<char> packet;
  CreateRandomPacket(&packet);
  socket_host_->Send(dest1_, packet);

  ASSERT_EQ(1U, sent_packets_.size());
  ASSERT_EQ(dest1_, sent_packets_[0].first);
}

// Verify that we can send data after we've received STUN response
// from the other side.
TEST_F(P2PSocketHostUdpTest, SendAfterStunResponse) {
  // Receive packet from |dest1_|.
  std::vector<char> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(sender_, Send(MatchPacketMessage(request_packet)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  socket_->ReceivePacket(dest1_, request_packet);

  // Now we should be able to send any data to |dest1_|.
  std::vector<char> packet;
  CreateRandomPacket(&packet);
  socket_host_->Send(dest1_, packet);

  ASSERT_EQ(1U, sent_packets_.size());
  ASSERT_EQ(dest1_, sent_packets_[0].first);
}

// Verify messages still cannot be sent to an unathorized host after
// successful binding with different host.
TEST_F(P2PSocketHostUdpTest, SendAfterStunResponseDifferentHost) {
  // Receive packet from |dest1_|.
  std::vector<char> request_packet;
  CreateStunRequest(&request_packet);

  EXPECT_CALL(sender_, Send(MatchPacketMessage(request_packet)))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  socket_->ReceivePacket(dest1_, request_packet);

  // Should fail when trying to send the same packet to |dest2_|.
  std::vector<char> packet;
  CreateRandomPacket(&packet);
  EXPECT_CALL(sender_, Send(
      MatchMessage(static_cast<uint32>(P2PMsg_OnError::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  socket_host_->Send(dest2_, packet);
}

}  // namespace content
