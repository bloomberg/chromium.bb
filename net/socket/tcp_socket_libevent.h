// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SOCKET_LIBEVENT_H_
#define NET_SOCKET_TCP_SOCKET_LIBEVENT_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/address_family.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"

namespace net {

class AddressList;
class IOBuffer;
class IPEndPoint;
class SocketLibevent;

class NET_EXPORT TCPSocketLibevent {
 public:
  TCPSocketLibevent(NetLog* net_log, const NetLog::Source& source);
  virtual ~TCPSocketLibevent();

  int Open(AddressFamily family);
  // Takes ownership of |socket_fd|.
  int AdoptConnectedSocket(int socket_fd, const IPEndPoint& peer_address);

  int Bind(const IPEndPoint& address);

  int Listen(int backlog);
  int Accept(scoped_ptr<TCPSocketLibevent>* socket,
             IPEndPoint* address,
             const CompletionCallback& callback);

  int Connect(const IPEndPoint& address, const CompletionCallback& callback);
  bool IsConnected() const;
  bool IsConnectedAndIdle() const;

  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported.
  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback);
  int Write(IOBuffer* buf, int buf_len, const CompletionCallback& callback);

  int GetLocalAddress(IPEndPoint* address) const;
  int GetPeerAddress(IPEndPoint* address) const;

  // Sets various socket options.
  // The commonly used options for server listening sockets:
  // - SetAddressReuse(true).
  int SetDefaultOptionsForServer();
  // The commonly used options for client sockets and accepted sockets:
  // - SetNoDelay(true);
  // - SetKeepAlive(true, 45).
  void SetDefaultOptionsForClient();
  int SetAddressReuse(bool allow);
  int SetReceiveBufferSize(int32 size);
  int SetSendBufferSize(int32 size);
  bool SetKeepAlive(bool enable, int delay);
  bool SetNoDelay(bool no_delay);

  void Close();

  bool UsingTCPFastOpen() const;
  bool IsValid() const;

  // Marks the start/end of a series of connect attempts for logging purpose.
  //
  // TCPClientSocket may attempt to connect to multiple addresses until it
  // succeeds in establishing a connection. The corresponding log will have
  // multiple NetLog::TYPE_TCP_CONNECT_ATTEMPT entries nested within a
  // NetLog::TYPE_TCP_CONNECT. These methods set the start/end of
  // NetLog::TYPE_TCP_CONNECT.
  //
  // TODO(yzshen): Change logging format and let TCPClientSocket log the
  // start/end of a series of connect attempts itself.
  void StartLoggingMultipleConnectAttempts(const AddressList& addresses);
  void EndLoggingMultipleConnectAttempts(int net_error);

  const BoundNetLog& net_log() const { return net_log_; }

 private:
  // States that a fast open socket attempt can result in.
  enum FastOpenStatus {
    FAST_OPEN_STATUS_UNKNOWN,

    // The initial fast open connect attempted returned synchronously,
    // indicating that we had and sent a cookie along with the initial data.
    FAST_OPEN_FAST_CONNECT_RETURN,

    // The initial fast open connect attempted returned asynchronously,
    // indicating that we did not have a cookie for the server.
    FAST_OPEN_SLOW_CONNECT_RETURN,

    // Some other error occurred on connection, so we couldn't tell if
    // fast open would have worked.
    FAST_OPEN_ERROR,

    // An attempt to do a fast open succeeded immediately
    // (FAST_OPEN_FAST_CONNECT_RETURN) and we later confirmed that the server
    // had acked the data we sent.
    FAST_OPEN_SYN_DATA_ACK,

    // An attempt to do a fast open succeeded immediately
    // (FAST_OPEN_FAST_CONNECT_RETURN) and we later confirmed that the server
    // had nacked the data we sent.
    FAST_OPEN_SYN_DATA_NACK,

    // An attempt to do a fast open succeeded immediately
    // (FAST_OPEN_FAST_CONNECT_RETURN) and our probe to determine if the
    // socket was using fast open failed.
    FAST_OPEN_SYN_DATA_FAILED,

    // An attempt to do a fast open failed (FAST_OPEN_SLOW_CONNECT_RETURN)
    // and we later confirmed that the server had acked initial data.  This
    // should never happen (we didn't send data, so it shouldn't have
    // been acked).
    FAST_OPEN_NO_SYN_DATA_ACK,

    // An attempt to do a fast open failed (FAST_OPEN_SLOW_CONNECT_RETURN)
    // and we later discovered that the server had nacked initial data.  This
    // is the expected case results for FAST_OPEN_SLOW_CONNECT_RETURN.
    FAST_OPEN_NO_SYN_DATA_NACK,

    // An attempt to do a fast open failed (FAST_OPEN_SLOW_CONNECT_RETURN)
    // and our later probe for ack/nack state failed.
    FAST_OPEN_NO_SYN_DATA_FAILED,

    FAST_OPEN_MAX_VALUE
  };

  void AcceptCompleted(scoped_ptr<TCPSocketLibevent>* tcp_socket,
                       IPEndPoint* address,
                       const CompletionCallback& callback,
                       int rv);
  int HandleAcceptCompleted(scoped_ptr<TCPSocketLibevent>* tcp_socket,
                            IPEndPoint* address,
                            int rv);
  int BuildTcpSocketLibevent(scoped_ptr<TCPSocketLibevent>* tcp_socket,
                             IPEndPoint* address);

  void ConnectCompleted(const CompletionCallback& callback, int rv) const;
  int HandleConnectCompleted(int rv) const;
  void LogConnectBegin(const AddressList& addresses) const;
  void LogConnectEnd(int net_error) const;

  void ReadCompleted(const scoped_refptr<IOBuffer>& buf,
                     const CompletionCallback& callback,
                     int rv);
  int HandleReadCompleted(IOBuffer* buf, int rv);

  void WriteCompleted(const scoped_refptr<IOBuffer>& buf,
                      const CompletionCallback& callback,
                      int rv) const;
  int HandleWriteCompleted(IOBuffer* buf, int rv) const;
  int TcpFastOpenWrite(IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback);

  // Called when the socket is known to be in a connected state.
  void RecordFastOpenStatus();

  scoped_ptr<SocketLibevent> socket_;
  scoped_ptr<SocketLibevent> accept_socket_;

  // Enables experimental TCP FastOpen option.
  const bool use_tcp_fastopen_;

  // True when TCP FastOpen is in use and we have done the connect.
  bool tcp_fastopen_connected_;

  FastOpenStatus fast_open_status_;

  bool logging_multiple_connect_attempts_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocketLibevent);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SOCKET_LIBEVENT_H_
