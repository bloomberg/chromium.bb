// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MOCK_MDNS_SOCKET_FACTORY_H_
#define NET_DNS_MOCK_MDNS_SOCKET_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/datagram_buffer.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/mdns_client_impl.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class IPAddress;
class SocketTag;
struct NetworkTrafficAnnotationTag;

class MockMDnsDatagramServerSocket : public DatagramServerSocket {
 public:
  explicit MockMDnsDatagramServerSocket(AddressFamily address_family);
  ~MockMDnsDatagramServerSocket() override;

  // DatagramServerSocket implementation:
  MOCK_METHOD1(Listen, int(const IPEndPoint& address));

  // GMock cannot handle move-only types like CompletionOnceCallback, so we use
  // delegate methods RecvFromInternal and SendToInternal below as the
  // respectively mocked methods for RecvFrom and SendTo.
  int RecvFrom(IOBuffer* buffer,
               int size,
               IPEndPoint* address,
               CompletionOnceCallback callback) override;

  MOCK_METHOD4(RecvFromInternal,
               int(IOBuffer* buffer,
                   int size,
                   IPEndPoint* address,
                   CompletionOnceCallback* callback));

  int SendTo(IOBuffer* buf,
             int buf_len,
             const IPEndPoint& address,
             CompletionOnceCallback callback) override;

  MOCK_METHOD3(SendToInternal,
               int(const std::string& packet,
                   const std::string address,
                   CompletionOnceCallback* callback));

  MOCK_METHOD1(SetReceiveBufferSize, int(int32_t size));
  MOCK_METHOD1(SetSendBufferSize, int(int32_t size));
  MOCK_METHOD0(SetDoNotFragment, int());
  MOCK_METHOD1(SetMsgConfirm, void(bool confirm));

  MOCK_METHOD0(Close, void());

  MOCK_CONST_METHOD1(GetPeerAddress, int(IPEndPoint* address));
  int GetLocalAddress(IPEndPoint* address) const override;
  MOCK_METHOD0(UseNonBlockingIO, void());
  MOCK_METHOD0(UseWriteBatching, void());
  MOCK_METHOD0(UseMultiCore, void());
  MOCK_METHOD0(UseSendmmsg, void());
  MOCK_CONST_METHOD0(NetLog, const NetLogWithSource&());

  MOCK_METHOD0(AllowAddressReuse, void());
  MOCK_METHOD0(AllowBroadcast, void());
  MOCK_METHOD0(AllowAddressSharingForMulticast, void());

  MOCK_CONST_METHOD1(JoinGroup, int(const IPAddress& group_address));
  MOCK_CONST_METHOD1(LeaveGroup, int(const IPAddress& address));

  MOCK_METHOD1(SetMulticastInterface, int(uint32_t interface_index));
  MOCK_METHOD1(SetMulticastTimeToLive, int(int ttl));
  MOCK_METHOD1(SetMulticastLoopbackMode, int(bool loopback));

  MOCK_METHOD1(SetDiffServCodePoint, int(DiffServCodePoint dscp));

  MOCK_METHOD0(DetachFromThread, void());

  void SetResponsePacket(const std::string& response_packet);

  int HandleRecvNow(IOBuffer* buffer,
                    int size,
                    IPEndPoint* address,
                    CompletionOnceCallback* callback);

  int HandleRecvLater(IOBuffer* buffer,
                      int size,
                      IPEndPoint* address,
                      CompletionOnceCallback* callback);

 private:
  std::string response_packet_;
  IPEndPoint local_address_;
};

class MockMDnsDatagramClientSocket : public DatagramClientSocket {
 public:
  MockMDnsDatagramClientSocket();
  ~MockMDnsDatagramClientSocket() override;

  // DatagramSocket implementation:
  MOCK_METHOD0(Close, void());
  MOCK_CONST_METHOD1(GetPeerAddress, int(IPEndPoint* address));
  MOCK_CONST_METHOD1(GetLocalAddress, int(IPEndPoint* address));
  MOCK_METHOD0(UseNonBlockingIO, void());
  MOCK_METHOD0(SetDoNotFragment, int());
  MOCK_METHOD1(SetMsgConfirm, void(bool confirm));
  MOCK_CONST_METHOD0(NetLog, const NetLogWithSource&());

  // Socket implementation:
  MOCK_METHOD3(Read, int(IOBuffer*, int, CompletionOnceCallback));
  MOCK_METHOD3(ReadIfReady, int(IOBuffer*, int, CompletionOnceCallback));
  MOCK_METHOD0(CancelReadIfReady, int());
  // GMock cannot handle move-only types like CompletionOnceCallback, so we use
  // a delegate method WriteInternal below as the mocked method for Write.
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  MOCK_METHOD3(WriteInternal,
               int(const std::string,
                   CompletionOnceCallback*,
                   const NetworkTrafficAnnotationTag&));
  MOCK_METHOD1(SetReceiveBufferSize, int(int32_t));
  MOCK_METHOD1(SetSendBufferSize, int(int32_t));

  // DatagramClientSocket implementation:
  MOCK_METHOD1(Connect, int(const IPEndPoint& address));
  MOCK_METHOD2(ConnectUsingNetwork,
               int(NetworkChangeNotifier::NetworkHandle, const IPEndPoint&));
  MOCK_METHOD1(ConnectUsingDefaultNetwork, int(const IPEndPoint&));

  MOCK_CONST_METHOD0(GetBoundNetwork, NetworkChangeNotifier::NetworkHandle());
  MOCK_METHOD1(ApplySocketTag, void(const SocketTag&));
  MOCK_METHOD3(WriteAsync,
               int(DatagramBuffers,
                   CompletionOnceCallback,
                   const NetworkTrafficAnnotationTag&));
  MOCK_METHOD4(WriteAsync,
               int(const char*,
                   size_t,
                   CompletionOnceCallback,
                   const NetworkTrafficAnnotationTag&));
  MOCK_METHOD0(GetUnwrittenBuffers, DatagramBuffers());
  MOCK_METHOD1(SetWriteAsyncEnabled, void(bool));
  MOCK_METHOD1(SetMaxPacketSize, void(size_t));
  MOCK_METHOD0(WriteAsyncEnabled, bool());
  MOCK_METHOD1(SetWriteMultiCoreEnabled, void(bool));
  MOCK_METHOD1(SetSendmmsgEnabled, void(bool));
  MOCK_METHOD1(SetWriteBatchingActive, void(bool));
  MOCK_METHOD1(SetMulticastInterface, int(uint32_t));
};

class MockMDnsSocketFactory : public MDnsSocketFactory {
 public:
  MockMDnsSocketFactory();
  ~MockMDnsSocketFactory() override;

  void CreateSocketPairs(
      std::vector<MDnsSendRecvSocketPair>* socket_pairs) override;

  void SimulateReceive(const uint8_t* packet, int size);

  MOCK_METHOD1(OnSendTo, void(const std::string&));

 private:
  // The following two methods are the default implementations of
  // MockMDnsDatagramClientSocket::WriteInternal and
  // MockMDnsDatagramServerSocket::RecvFromInternal respectively.
  int WriteInternal(const std::string& packet,
                    CompletionOnceCallback* callback,
                    const NetworkTrafficAnnotationTag& traffic_annotation);
  // The latest receive callback is always saved, since the MDnsConnection
  // does not care which socket a packet is received on.
  int RecvFromInternal(IOBuffer* buffer,
                       int size,
                       IPEndPoint* address,
                       CompletionOnceCallback* callback);

  void CreateSocketPair(AddressFamily address_family,
                        std::vector<MDnsSendRecvSocketPair>* socket_pairs);

  scoped_refptr<IOBuffer> recv_buffer_;
  int recv_buffer_size_;
  CompletionOnceCallback recv_callback_;
};

}  // namespace net

#endif  // NET_DNS_MOCK_MDNS_SOCKET_FACTORY_H_
