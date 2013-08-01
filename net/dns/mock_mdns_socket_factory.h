// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MOCK_MDNS_SOCKET_FACTORY_H_
#define NET_DNS_MOCK_MDNS_SOCKET_FACTORY_H_

#include <string>

#include "net/dns/mdns_client_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class MockMDnsDatagramServerSocket : public DatagramServerSocket {
 public:
  MockMDnsDatagramServerSocket();
  ~MockMDnsDatagramServerSocket();

  // DatagramServerSocket implementation:
  int Listen(const IPEndPoint& address);

  MOCK_METHOD1(ListenInternal, int(const std::string& address));

  MOCK_METHOD4(RecvFrom, int(IOBuffer* buffer, int size,
                             IPEndPoint* address,
                             const CompletionCallback& callback));

  int SendTo(IOBuffer* buf, int buf_len, const IPEndPoint& address,
             const CompletionCallback& callback);

  MOCK_METHOD3(SendToInternal, int(const std::string& packet,
                                   const std::string address,
                                   const CompletionCallback& callback));

  MOCK_METHOD1(SetReceiveBufferSize, bool(int32 size));
  MOCK_METHOD1(SetSendBufferSize, bool(int32 size));

  MOCK_METHOD0(Close, void());

  MOCK_CONST_METHOD1(GetPeerAddress, int(IPEndPoint* address));
  MOCK_CONST_METHOD1(GetLocalAddress, int(IPEndPoint* address));
  MOCK_CONST_METHOD0(NetLog, const BoundNetLog&());

  MOCK_METHOD0(AllowAddressReuse, void());
  MOCK_METHOD0(AllowBroadcast, void());

  int JoinGroup(const IPAddressNumber& group_address) const;

  MOCK_CONST_METHOD1(JoinGroupInternal, int(const std::string& group));

  int LeaveGroup(const IPAddressNumber& group_address) const;

  MOCK_CONST_METHOD1(LeaveGroupInternal, int(const std::string& group));

  MOCK_METHOD1(SetMulticastTimeToLive, int(int ttl));

  MOCK_METHOD1(SetMulticastLoopbackMode, int(bool loopback));

  void SetResponsePacket(std::string response_packet);

  int HandleRecvNow(IOBuffer* buffer, int size, IPEndPoint* address,
                    const CompletionCallback& callback);

  int HandleRecvLater(IOBuffer* buffer, int size, IPEndPoint* address,
                      const CompletionCallback& callback);

 private:
  std::string response_packet_;
};

class MockMDnsSocketFactory : public MDnsConnection::SocketFactory {
 public:
  MockMDnsSocketFactory();

  virtual ~MockMDnsSocketFactory();

  virtual scoped_ptr<DatagramServerSocket> CreateSocket() OVERRIDE;

  void SimulateReceive(const uint8* packet, int size);

  MOCK_METHOD1(OnSendTo, void(const std::string&));

 private:
  int SendToInternal(const std::string& packet, const std::string& address,
                     const CompletionCallback& callback);

  // The latest receive callback is always saved, since the MDnsConnection
  // does not care which socket a packet is received on.
  int RecvFromInternal(IOBuffer* buffer, int size,
                       IPEndPoint* address,
                       const CompletionCallback& callback);

  scoped_refptr<IOBuffer> recv_buffer_;
  int recv_buffer_size_;
  CompletionCallback recv_callback_;
};

}  // namespace net

#endif  // NET_DNS_MOCK_MDNS_SOCKET_FACTORY_H_
