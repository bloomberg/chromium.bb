// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool_test_util.h"

#include <string>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_util.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/udp/datagram_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

IPAddressNumber ParseIP(const std::string& ip) {
  IPAddressNumber number;
  CHECK(ParseIPLiteralToNumber(ip, &number));
  return number;
}

// A StreamSocket which connects synchronously and successfully.
class MockConnectClientSocket : public StreamSocket {
 public:
  MockConnectClientSocket(const AddressList& addrlist, net::NetLog* net_log)
      : connected_(false),
        addrlist_(addrlist),
        net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)),
        use_tcp_fastopen_(false) {}

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE {
    connected_ = true;
    return OK;
  }
  virtual void Disconnect() OVERRIDE { connected_ = false; }
  virtual bool IsConnected() const OVERRIDE { return connected_; }
  virtual bool IsConnectedAndIdle() const OVERRIDE { return connected_; }

  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE {
    *address = addrlist_.front();
    return OK;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE {
    if (!connected_)
      return ERR_SOCKET_NOT_CONNECTED;
    if (addrlist_.front().GetFamily() == ADDRESS_FAMILY_IPV4)
      SetIPv4Address(address);
    else
      SetIPv6Address(address);
    return OK;
  }
  virtual const BoundNetLog& NetLog() const OVERRIDE { return net_log_; }

  virtual void SetSubresourceSpeculation() OVERRIDE {}
  virtual void SetOmniboxSpeculation() OVERRIDE {}
  virtual bool WasEverUsed() const OVERRIDE { return false; }
  virtual void EnableTCPFastOpenIfSupported() OVERRIDE {
    use_tcp_fastopen_ = true;
  }
  virtual bool UsingTCPFastOpen() const OVERRIDE { return use_tcp_fastopen_; }
  virtual bool WasNpnNegotiated() const OVERRIDE { return false; }
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE {
    return kProtoUnknown;
  }
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE { return false; }

  // Socket implementation.
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE { return OK; }
  virtual int SetSendBufferSize(int32 size) OVERRIDE { return OK; }

 private:
  bool connected_;
  const AddressList addrlist_;
  BoundNetLog net_log_;
  bool use_tcp_fastopen_;

  DISALLOW_COPY_AND_ASSIGN(MockConnectClientSocket);
};

class MockFailingClientSocket : public StreamSocket {
 public:
  MockFailingClientSocket(const AddressList& addrlist, net::NetLog* net_log)
      : addrlist_(addrlist),
        net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)),
        use_tcp_fastopen_(false) {}

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE {
    return ERR_CONNECTION_FAILED;
  }

  virtual void Disconnect() OVERRIDE {}

  virtual bool IsConnected() const OVERRIDE { return false; }
  virtual bool IsConnectedAndIdle() const OVERRIDE { return false; }
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE {
    return ERR_UNEXPECTED;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE {
    return ERR_UNEXPECTED;
  }
  virtual const BoundNetLog& NetLog() const OVERRIDE { return net_log_; }

  virtual void SetSubresourceSpeculation() OVERRIDE {}
  virtual void SetOmniboxSpeculation() OVERRIDE {}
  virtual bool WasEverUsed() const OVERRIDE { return false; }
  virtual void EnableTCPFastOpenIfSupported() OVERRIDE {
    use_tcp_fastopen_ = true;
  }
  virtual bool UsingTCPFastOpen() const OVERRIDE { return use_tcp_fastopen_; }
  virtual bool WasNpnNegotiated() const OVERRIDE { return false; }
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE {
    return kProtoUnknown;
  }
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE { return false; }

  // Socket implementation.
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }

  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE { return OK; }
  virtual int SetSendBufferSize(int32 size) OVERRIDE { return OK; }

 private:
  const AddressList addrlist_;
  BoundNetLog net_log_;
  bool use_tcp_fastopen_;

  DISALLOW_COPY_AND_ASSIGN(MockFailingClientSocket);
};

class MockTriggerableClientSocket : public StreamSocket {
 public:
  // |should_connect| indicates whether the socket should successfully complete
  // or fail.
  MockTriggerableClientSocket(const AddressList& addrlist,
                              bool should_connect,
                              net::NetLog* net_log)
      : should_connect_(should_connect),
        is_connected_(false),
        addrlist_(addrlist),
        net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)),
        use_tcp_fastopen_(false),
        weak_factory_(this) {}

  // Call this method to get a closure which will trigger the connect callback
  // when called. The closure can be called even after the socket is deleted; it
  // will safely do nothing.
  base::Closure GetConnectCallback() {
    return base::Bind(&MockTriggerableClientSocket::DoCallback,
                      weak_factory_.GetWeakPtr());
  }

  static scoped_ptr<StreamSocket> MakeMockPendingClientSocket(
      const AddressList& addrlist,
      bool should_connect,
      net::NetLog* net_log) {
    scoped_ptr<MockTriggerableClientSocket> socket(
        new MockTriggerableClientSocket(addrlist, should_connect, net_log));
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           socket->GetConnectCallback());
    return socket.PassAs<StreamSocket>();
  }

  static scoped_ptr<StreamSocket> MakeMockDelayedClientSocket(
      const AddressList& addrlist,
      bool should_connect,
      const base::TimeDelta& delay,
      net::NetLog* net_log) {
    scoped_ptr<MockTriggerableClientSocket> socket(
        new MockTriggerableClientSocket(addrlist, should_connect, net_log));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, socket->GetConnectCallback(), delay);
    return socket.PassAs<StreamSocket>();
  }

  static scoped_ptr<StreamSocket> MakeMockStalledClientSocket(
      const AddressList& addrlist,
      net::NetLog* net_log) {
    scoped_ptr<MockTriggerableClientSocket> socket(
        new MockTriggerableClientSocket(addrlist, true, net_log));
    return socket.PassAs<StreamSocket>();
  }

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE {
    DCHECK(callback_.is_null());
    callback_ = callback;
    return ERR_IO_PENDING;
  }

  virtual void Disconnect() OVERRIDE {}

  virtual bool IsConnected() const OVERRIDE { return is_connected_; }
  virtual bool IsConnectedAndIdle() const OVERRIDE { return is_connected_; }
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE {
    *address = addrlist_.front();
    return OK;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE {
    if (!is_connected_)
      return ERR_SOCKET_NOT_CONNECTED;
    if (addrlist_.front().GetFamily() == ADDRESS_FAMILY_IPV4)
      SetIPv4Address(address);
    else
      SetIPv6Address(address);
    return OK;
  }
  virtual const BoundNetLog& NetLog() const OVERRIDE { return net_log_; }

  virtual void SetSubresourceSpeculation() OVERRIDE {}
  virtual void SetOmniboxSpeculation() OVERRIDE {}
  virtual bool WasEverUsed() const OVERRIDE { return false; }
  virtual void EnableTCPFastOpenIfSupported() OVERRIDE {
    use_tcp_fastopen_ = true;
  }
  virtual bool UsingTCPFastOpen() const OVERRIDE { return use_tcp_fastopen_; }
  virtual bool WasNpnNegotiated() const OVERRIDE { return false; }
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE {
    return kProtoUnknown;
  }
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE { return false; }

  // Socket implementation.
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }

  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return ERR_FAILED;
  }
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE { return OK; }
  virtual int SetSendBufferSize(int32 size) OVERRIDE { return OK; }

 private:
  void DoCallback() {
    is_connected_ = should_connect_;
    callback_.Run(is_connected_ ? OK : ERR_CONNECTION_FAILED);
  }

  bool should_connect_;
  bool is_connected_;
  const AddressList addrlist_;
  BoundNetLog net_log_;
  CompletionCallback callback_;
  bool use_tcp_fastopen_;

  base::WeakPtrFactory<MockTriggerableClientSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockTriggerableClientSocket);
};

}  // namespace

void TestLoadTimingInfoConnectedReused(const ClientSocketHandle& handle) {
  LoadTimingInfo load_timing_info;
  // Only pass true in as |is_reused|, as in general, HttpStream types should
  // have stricter concepts of reuse than socket pools.
  EXPECT_TRUE(handle.GetLoadTimingInfo(true, &load_timing_info));

  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

void TestLoadTimingInfoConnectedNotReused(const ClientSocketHandle& handle) {
  EXPECT_FALSE(handle.is_reused());

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(handle.GetLoadTimingInfo(false, &load_timing_info));

  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              CONNECT_TIMING_HAS_DNS_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);

  TestLoadTimingInfoConnectedReused(handle);
}

void SetIPv4Address(IPEndPoint* address) {
  *address = IPEndPoint(ParseIP("1.1.1.1"), 80);
}

void SetIPv6Address(IPEndPoint* address) {
  *address = IPEndPoint(ParseIP("1:abcd::3:4:ff"), 80);
}

MockTransportClientSocketFactory::MockTransportClientSocketFactory(
    NetLog* net_log)
    : net_log_(net_log),
      allocation_count_(0),
      client_socket_type_(MOCK_CLIENT_SOCKET),
      client_socket_types_(NULL),
      client_socket_index_(0),
      client_socket_index_max_(0),
      delay_(base::TimeDelta::FromMilliseconds(
          ClientSocketPool::kMaxConnectRetryIntervalMs)) {}

MockTransportClientSocketFactory::~MockTransportClientSocketFactory() {}

scoped_ptr<DatagramClientSocket>
MockTransportClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    const RandIntCallback& rand_int_cb,
    NetLog* net_log,
    const NetLog::Source& source) {
  NOTREACHED();
  return scoped_ptr<DatagramClientSocket>();
}

scoped_ptr<StreamSocket>
MockTransportClientSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    NetLog* /* net_log */,
    const NetLog::Source& /* source */) {
  allocation_count_++;

  ClientSocketType type = client_socket_type_;
  if (client_socket_types_ && client_socket_index_ < client_socket_index_max_) {
    type = client_socket_types_[client_socket_index_++];
  }

  switch (type) {
    case MOCK_CLIENT_SOCKET:
      return scoped_ptr<StreamSocket>(
          new MockConnectClientSocket(addresses, net_log_));
    case MOCK_FAILING_CLIENT_SOCKET:
      return scoped_ptr<StreamSocket>(
          new MockFailingClientSocket(addresses, net_log_));
    case MOCK_PENDING_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockPendingClientSocket(
          addresses, true, net_log_);
    case MOCK_PENDING_FAILING_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockPendingClientSocket(
          addresses, false, net_log_);
    case MOCK_DELAYED_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockDelayedClientSocket(
          addresses, true, delay_, net_log_);
    case MOCK_DELAYED_FAILING_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockDelayedClientSocket(
          addresses, false, delay_, net_log_);
    case MOCK_STALLED_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockStalledClientSocket(addresses,
                                                                      net_log_);
    case MOCK_TRIGGERABLE_CLIENT_SOCKET: {
      scoped_ptr<MockTriggerableClientSocket> rv(
          new MockTriggerableClientSocket(addresses, true, net_log_));
      triggerable_sockets_.push(rv->GetConnectCallback());
      // run_loop_quit_closure_ behaves like a condition variable. It will
      // wake up WaitForTriggerableSocketCreation() if it is sleeping. We
      // don't need to worry about atomicity because this code is
      // single-threaded.
      if (!run_loop_quit_closure_.is_null())
        run_loop_quit_closure_.Run();
      return rv.PassAs<StreamSocket>();
    }
    default:
      NOTREACHED();
      return scoped_ptr<StreamSocket>(
          new MockConnectClientSocket(addresses, net_log_));
  }
}

scoped_ptr<SSLClientSocket>
MockTransportClientSocketFactory::CreateSSLClientSocket(
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    const SSLClientSocketContext& context) {
  NOTIMPLEMENTED();
  return scoped_ptr<SSLClientSocket>();
}

void MockTransportClientSocketFactory::ClearSSLSessionCache() {
  NOTIMPLEMENTED();
}

void MockTransportClientSocketFactory::set_client_socket_types(
    ClientSocketType* type_list,
    int num_types) {
  DCHECK_GT(num_types, 0);
  client_socket_types_ = type_list;
  client_socket_index_ = 0;
  client_socket_index_max_ = num_types;
}

base::Closure
MockTransportClientSocketFactory::WaitForTriggerableSocketCreation() {
  while (triggerable_sockets_.empty()) {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_closure_.Reset();
  }
  base::Closure trigger = triggerable_sockets_.front();
  triggerable_sockets_.pop();
  return trigger;
}

}  // namespace net
