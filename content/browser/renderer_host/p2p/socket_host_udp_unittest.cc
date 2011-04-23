// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>  // for htonl
#else
#include <arpa/inet.h>
#endif

#include <deque>
#include <vector>

#include "content/browser/renderer_host/p2p/socket_host_udp.h"
#include "content/common/p2p_messages.h"
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

const char kTestLocalIpAddress[] = "123.44.22.4";
const char kTestIpAddress1[] = "123.44.22.31";
const int kTestPort1 = 234;
const char kTestIpAddress2[] = "133.11.22.33";
const int kTestPort2 = 543;

const int kStunHeaderSize = 20;
const uint16 kStunBindingRequest = 0x0001;
const uint16 kStunBindingResponse = 0x0102;
const uint16 kStunBindingError = 0x0111;
const uint32 kStunMagicCookie = 0x2112A442;

class FakeDatagramServerSocket : public net::DatagramServerSocket {
 public:
  typedef std::pair<net::IPEndPoint, std::vector<char> > UDPPacket;

  // P2PSocketHostUdp destroyes a socket on errors so sent packets
  // need to be stored outside of this object.
  explicit FakeDatagramServerSocket(std::deque<UDPPacket>* sent_packets)
      : sent_packets_(sent_packets), recv_callback_(NULL) {
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
                       net::CompletionCallback* callback) OVERRIDE {
    CHECK(!recv_callback_);
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
                     net::CompletionCallback* callback) OVERRIDE {
    scoped_refptr<net::IOBuffer> buffer(buf);
    std::vector<char> data_vector(buffer->data(), buffer->data() + buf_len);
    sent_packets_->push_back(UDPPacket(address, data_vector));
    return buf_len;
  }

  void ReceivePacket(const net::IPEndPoint& address, std::vector<char> data) {
    if (recv_callback_) {
      int size = std::min(recv_size_, static_cast<int>(data.size()));
      memcpy(recv_buffer_->data(), &*data.begin(), size);
      *recv_address_ = address;
      net::CompletionCallback* cb = recv_callback_;
      recv_callback_ = NULL;
      recv_buffer_ = NULL;
      cb->Run(size);
    } else {
      incoming_packets_.push_back(UDPPacket(address, data));
    }
  }

 private:
  net::IPEndPoint address_;
  std::deque<UDPPacket>* sent_packets_;
  std::deque<UDPPacket> incoming_packets_;

  scoped_refptr<net::IOBuffer> recv_buffer_;
  net::IPEndPoint* recv_address_;
  int recv_size_;
  net::CompletionCallback* recv_callback_;
};

class MockIPCSender : public IPC::Message::Sender {
 public:
  MOCK_METHOD1(Send, bool(IPC::Message* msg));
};

MATCHER_P(MatchMessage, type, "") {
  return arg->type() == type;
}

}  // namespace

class P2PSocketHostUdpTest : public testing::Test {
 protected:
  void SetUp() OVERRIDE {
    EXPECT_CALL(sender_, Send(
        MatchMessage(static_cast<uint32>(P2PMsg_OnSocketCreated::ID))))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

    socket_host_.reset(new P2PSocketHostUdp(&sender_, 0, 0));
    socket_ = new FakeDatagramServerSocket(&sent_packets_);
    socket_host_->socket_.reset(socket_);

    net::IPAddressNumber local_ip;
    ASSERT_TRUE(net::ParseIPLiteralToNumber(kTestLocalIpAddress, &local_ip));
    local_address_ = net::IPEndPoint(local_ip, kTestPort1);
    socket_host_->Init(local_address_, net::IPEndPoint());

    net::IPAddressNumber ip1;
    ASSERT_TRUE(net::ParseIPLiteralToNumber(kTestIpAddress1, &ip1));
    dest1_ = net::IPEndPoint(ip1, kTestPort1);
    net::IPAddressNumber ip2;
    ASSERT_TRUE(net::ParseIPLiteralToNumber(kTestIpAddress2, &ip2));
    dest2_ = net::IPEndPoint(ip2, kTestPort2);
  }

  void CreateRandomPacket(std::vector<char>* packet) {
    size_t size = kStunHeaderSize + rand() % 1000;
    packet->resize(size);
    for (size_t i = 0; i < size; i++) {
      (*packet)[i] = rand() % 256;
    }
    // Always set the first bit to ensure that generated packet is not
    // valid STUN packet.
    (*packet)[0] = (*packet)[0] | 0x80;
  }

  void CreateStunPacket(std::vector<char>* packet, uint16 type) {
    CreateRandomPacket(packet);
    *reinterpret_cast<uint16*>(&*packet->begin()) = htons(type);
    *reinterpret_cast<uint16*>(&*packet->begin() + 2) =
        htons(packet->size() - kStunHeaderSize);
    *reinterpret_cast<uint32*>(&*packet->begin() + 4) = htonl(kStunMagicCookie);
  }

  void CreateStunRequest(std::vector<char>* packet) {
    CreateStunPacket(packet, kStunBindingRequest);
  }

  void CreateStunResponse(std::vector<char>* packet) {
    CreateStunPacket(packet, kStunBindingResponse);
  }

  void CreateStunError(std::vector<char>* packet) {
    CreateStunPacket(packet, kStunBindingError);
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
  EXPECT_CALL(sender_, Send(
      MatchMessage(static_cast<uint32>(P2PMsg_OnDataReceived::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  // Receive packet from |dest1_|.
  std::vector<char> request_packet;
  CreateStunRequest(&request_packet);
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
  EXPECT_CALL(sender_, Send(
      MatchMessage(static_cast<uint32>(P2PMsg_OnDataReceived::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  // Receive packet from |dest1_|.
  std::vector<char> request_packet;
  CreateStunRequest(&request_packet);
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
  EXPECT_CALL(sender_, Send(
      MatchMessage(static_cast<uint32>(P2PMsg_OnDataReceived::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  // Receive packet from |dest1_|.
  std::vector<char> request_packet;
  CreateStunRequest(&request_packet);
  socket_->ReceivePacket(dest1_, request_packet);

  // Should fail when trying to send the same packet to |dest2_|.
  std::vector<char> packet;
  CreateRandomPacket(&packet);
  EXPECT_CALL(sender_, Send(
      MatchMessage(static_cast<uint32>(P2PMsg_OnError::ID))))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
  socket_host_->Send(dest2_, packet);
}
