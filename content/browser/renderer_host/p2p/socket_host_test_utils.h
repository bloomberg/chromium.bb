// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TEST_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TEST_UTILS_H_

#include <vector>

#include "base/sys_byteorder.h"
#include "content/common/p2p_messages.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class MockIPCSender : public IPC::Sender {
 public:
  MockIPCSender();
  virtual ~MockIPCSender();

  MOCK_METHOD1(Send, bool(IPC::Message* msg));
};

MockIPCSender::MockIPCSender() { }
MockIPCSender::~MockIPCSender() { }

class FakeSocket : public net::StreamSocket {
 public:
  FakeSocket(std::string* written_data);
  virtual ~FakeSocket();

  void AppendInputData(const char* data, int data_size);
  int input_pos() const { return input_pos_; }
  bool read_pending() const { return read_pending_; }
  void SetPeerAddress(const net::IPEndPoint& peer_address);
  void SetLocalAddress(const net::IPEndPoint& local_address);

  // net::Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;
  virtual int Connect(const net::CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual const net::BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual bool WasNpnNegotiated() const OVERRIDE;
  virtual net::NextProto GetNegotiatedProtocol() const OVERRIDE;
  virtual bool GetSSLInfo(net::SSLInfo* ssl_info) OVERRIDE;

 private:
  bool read_pending_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionCallback read_callback_;

  std::string* written_data_;
  std::string input_data_;
  int input_pos_;

  net::IPEndPoint peer_address_;
  net::IPEndPoint local_address_;

  net::BoundNetLog net_log_;
};

FakeSocket::FakeSocket(std::string* written_data)
    : read_pending_(false),
      written_data_(written_data),
      input_pos_(0) {
}

FakeSocket::~FakeSocket() { }

void FakeSocket::AppendInputData(const char* data, int data_size) {
  input_data_.insert(input_data_.end(), data, data + data_size);
  // Complete pending read if any.
  if (read_pending_) {
    read_pending_ = false;
    int result = std::min(read_buffer_size_,
                          static_cast<int>(input_data_.size() - input_pos_));
    CHECK(result > 0);
    memcpy(read_buffer_->data(), &input_data_[0] + input_pos_, result);
    input_pos_ += result;
    read_buffer_ = NULL;
    net::CompletionCallback cb = read_callback_;
    read_callback_.Reset();
    cb.Run(result);
  }
}

void FakeSocket::SetPeerAddress(const net::IPEndPoint& peer_address) {
  peer_address_ = peer_address;
}

void FakeSocket::SetLocalAddress(const net::IPEndPoint& local_address) {
  local_address_ = local_address;
}

int FakeSocket::Read(net::IOBuffer* buf, int buf_len,
                     const net::CompletionCallback& callback) {
  DCHECK(buf);
  if (input_pos_ < static_cast<int>(input_data_.size())){
    int result = std::min(buf_len,
                          static_cast<int>(input_data_.size()) - input_pos_);
    memcpy(buf->data(), &(*input_data_.begin()) + input_pos_, result);
    input_pos_ += result;
    return result;
  } else {
    read_pending_ = true;
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }
}

int FakeSocket::Write(net::IOBuffer* buf, int buf_len,
                      const net::CompletionCallback& callback) {
  DCHECK(buf);
  if (written_data_) {
    written_data_->insert(written_data_->end(),
                          buf->data(), buf->data() + buf_len);
  }
  return buf_len;
}


bool FakeSocket::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}
bool FakeSocket::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}

int FakeSocket::Connect(const net::CompletionCallback& callback) {
  return 0;
}

void FakeSocket::Disconnect() {
  NOTREACHED();
}

bool FakeSocket::IsConnected() const {
  return true;
}

bool FakeSocket::IsConnectedAndIdle() const {
  return false;
}

int FakeSocket::GetPeerAddress(net::IPEndPoint* address) const {
  *address = peer_address_;
  return net::OK;
}

int FakeSocket::GetLocalAddress(net::IPEndPoint* address) const {
  *address = local_address_;
  return net::OK;
}

const net::BoundNetLog& FakeSocket::NetLog() const {
  NOTREACHED();
  return net_log_;
}

void FakeSocket::SetSubresourceSpeculation() {
  NOTREACHED();
}

void FakeSocket::SetOmniboxSpeculation() {
  NOTREACHED();
}

bool FakeSocket::WasEverUsed() const {
  return true;
}

bool FakeSocket::UsingTCPFastOpen() const {
  return false;
}

bool FakeSocket::WasNpnNegotiated() const {
  return false;
}

net::NextProto FakeSocket::GetNegotiatedProtocol() const {
  return net::kProtoUnknown;
}

bool FakeSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  return false;
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
  *reinterpret_cast<uint16*>(&*packet->begin()) = base::HostToNet16(type);
  *reinterpret_cast<uint16*>(&*packet->begin() + 2) =
      base::HostToNet16(packet->size() - kStunHeaderSize);
  *reinterpret_cast<uint32*>(&*packet->begin() + 4) =
      base::HostToNet32(kStunMagicCookie);
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

net::IPEndPoint ParseAddress(const std::string ip_str, int port) {
  net::IPAddressNumber ip;
  EXPECT_TRUE(net::ParseIPLiteralToNumber(ip_str, &ip));
  return net::IPEndPoint(ip, port);
}

MATCHER_P(MatchMessage, type, "") {
  return arg->type() == type;
}

MATCHER_P(MatchPacketMessage, packet_content, "") {
  if (arg->type() != P2PMsg_OnDataReceived::ID)
    return false;
  P2PMsg_OnDataReceived::Param params;
  P2PMsg_OnDataReceived::Read(arg, &params);
  return params.c == packet_content;
}

MATCHER_P(MatchIncomingSocketMessage, address, "") {
  if (arg->type() != P2PMsg_OnIncomingTcpConnection::ID)
    return false;
  P2PMsg_OnIncomingTcpConnection::Param params;
  P2PMsg_OnIncomingTcpConnection::Read(
      arg, &params);
  return params.b == address;
}

}  // namespace

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_TEST_UTILS_H_
