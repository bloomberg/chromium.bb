// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_CLIENT_SOCKET_H_
#define NET_SOCKET_UDP_CLIENT_SOCKET_H_

#include <stdint.h>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/udp_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class NetLog;
struct NetLogSource;

// A client socket that uses UDP as the transport layer.
class NET_EXPORT_PRIVATE UDPClientSocket : public DatagramClientSocket {
 public:
  UDPClientSocket(DatagramSocket::BindType bind_type,
                  net::NetLog* net_log,
                  const net::NetLogSource& source);
  ~UDPClientSocket() override;

  // DatagramClientSocket implementation.
  int Connect(const IPEndPoint& address) override;
  int ConnectUsingNetwork(NetworkChangeNotifier::NetworkHandle network,
                          const IPEndPoint& address) override;
  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override;
  NetworkChangeNotifier::NetworkHandle GetBoundNetwork() const override;
  void ApplySocketTag(const SocketTag& tag) override;
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;

  int WriteAsync(
      const char* buffer,
      size_t buf_len,
      CompletionOnceCallback callback,
      const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int WriteAsync(
      DatagramBuffers buffers,
      CompletionOnceCallback callback,
      const NetworkTrafficAnnotationTag& traffic_annotation) override;

  DatagramBuffers GetUnwrittenBuffers() override;

  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  // Switch to use non-blocking IO. Must be called right after construction and
  // before other calls.
  void UseNonBlockingIO() override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int SetDoNotFragment() override;
  void SetMsgConfirm(bool confirm) override;
  const NetLogWithSource& NetLog() const override;
  void EnableRecvOptimization() override;

  void SetWriteAsyncEnabled(bool enabled) override;
  bool WriteAsyncEnabled() override;
  void SetMaxPacketSize(size_t max_packet_size) override;
  void SetWriteMultiCoreEnabled(bool enabled) override;
  void SetSendmmsgEnabled(bool enabled) override;
  void SetWriteBatchingActive(bool active) override;
  int SetMulticastInterface(uint32_t interface_index) override;

 private:
  UDPSocket socket_;
  NetworkChangeNotifier::NetworkHandle network_;

  DISALLOW_COPY_AND_ASSIGN(UDPClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_UDP_CLIENT_SOCKET_H_
