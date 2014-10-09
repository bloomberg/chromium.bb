// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CLIENT_SOCKET_H_
#define NET_SOCKET_TCP_CLIENT_SOCKET_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_socket.h"

namespace net {

// A client socket that uses TCP as the transport layer.
class NET_EXPORT TCPClientSocket : public StreamSocket {
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  TCPClientSocket(const AddressList& addresses,
                  net::NetLog* net_log,
                  const net::NetLog::Source& source);

  // Adopts the given, connected socket and then acts as if Connect() had been
  // called. This function is used by TCPServerSocket and for testing.
  TCPClientSocket(scoped_ptr<TCPSocket> connected_socket,
                  const IPEndPoint& peer_address);

  virtual ~TCPClientSocket();

  // Binds the socket to a local IP address and port.
  int Bind(const IPEndPoint& address);

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) override;
  virtual void Disconnect() override;
  virtual bool IsConnected() const override;
  virtual bool IsConnectedAndIdle() const override;
  virtual int GetPeerAddress(IPEndPoint* address) const override;
  virtual int GetLocalAddress(IPEndPoint* address) const override;
  virtual const BoundNetLog& NetLog() const override;
  virtual void SetSubresourceSpeculation() override;
  virtual void SetOmniboxSpeculation() override;
  virtual bool WasEverUsed() const override;
  virtual bool UsingTCPFastOpen() const override;
  virtual void EnableTCPFastOpenIfSupported() override;
  virtual bool WasNpnNegotiated() const override;
  virtual NextProto GetNegotiatedProtocol() const override;
  virtual bool GetSSLInfo(SSLInfo* ssl_info) override;

  // Socket implementation.
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported.
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) override;
  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback) override;
  virtual int SetReceiveBufferSize(int32 size) override;
  virtual int SetSendBufferSize(int32 size) override;

  virtual bool SetKeepAlive(bool enable, int delay);
  virtual bool SetNoDelay(bool no_delay);

 private:
  // State machine for connecting the socket.
  enum ConnectState {
    CONNECT_STATE_CONNECT,
    CONNECT_STATE_CONNECT_COMPLETE,
    CONNECT_STATE_NONE,
  };

  // State machine used by Connect().
  int DoConnectLoop(int result);
  int DoConnect();
  int DoConnectComplete(int result);

  // Helper used by Disconnect(), which disconnects minus resetting
  // current_address_index_ and bind_address_.
  void DoDisconnect();

  void DidCompleteConnect(int result);
  void DidCompleteReadWrite(const CompletionCallback& callback, int result);

  int OpenSocket(AddressFamily family);

  scoped_ptr<TCPSocket> socket_;

  // Local IP address and port we are bound to. Set to NULL if Bind()
  // wasn't called (in that case OS chooses address/port).
  scoped_ptr<IPEndPoint> bind_address_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list. Set to -1 if uninitialized.
  int current_address_index_;

  // External callback; called when connect is complete.
  CompletionCallback connect_callback_;

  // The next state for the Connect() state machine.
  ConnectState next_connect_state_;

  // This socket was previously disconnected and has not been re-connected.
  bool previously_disconnected_;

  // Record of connectivity and transmissions, for use in speculative connection
  // histograms.
  UseHistory use_history_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CLIENT_SOCKET_H_
