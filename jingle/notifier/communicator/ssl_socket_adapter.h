// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_SSL_SOCKET_ADAPTER_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_SSL_SOCKET_ADAPTER_H_

#include "base/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/socket/client_socket.h"
#include "net/socket/ssl_client_socket.h"
#include "talk/base/asyncsocket.h"
#include "talk/base/ssladapter.h"

namespace notifier {

class SSLSocketAdapter;

// TODO(sergeyu): Write unittests for this code!

// This class provides a wrapper to libjingle's talk_base::AsyncSocket that
// implements Chromium's net::ClientSocket interface.  It's used by
// SSLSocketAdapter to enable Chromium's SSL implementation to work over
// libjingle's socket class.
class TransportSocket : public net::ClientSocket, public sigslot::has_slots<> {
 public:
  TransportSocket(talk_base::AsyncSocket* socket,
                  SSLSocketAdapter *ssl_adapter);
  ~TransportSocket();

  void set_addr(const talk_base::SocketAddress& addr) {
    addr_ = addr;
  }

  // net::ClientSocket implementation

  virtual int Connect(net::CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual int GetPeerAddress(net::AddressList* address) const;
  virtual const net::BoundNetLog& NetLog() const { return net_log_; }
  virtual void SetSubresourceSpeculation();
  virtual void SetOmniboxSpeculation();

  // net::Socket implementation

  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  friend class SSLSocketAdapter;

  void OnReadEvent(talk_base::AsyncSocket* socket);
  void OnWriteEvent(talk_base::AsyncSocket* socket);

  net::CompletionCallback* read_callback_;
  net::CompletionCallback* write_callback_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_len_;
  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_len_;

  net::BoundNetLog net_log_;

  talk_base::AsyncSocket *socket_;
  talk_base::SocketAddress addr_;

  DISALLOW_COPY_AND_ASSIGN(TransportSocket);
};

// This provides a talk_base::AsyncSocketAdapter interface around Chromium's
// net::SSLClientSocket class.  This allows notifier to use Chromium's SSL
// implementation instead of OpenSSL.
class SSLSocketAdapter : public talk_base::SSLAdapter {
 public:
  explicit SSLSocketAdapter(talk_base::AsyncSocket* socket);
  ~SSLSocketAdapter();

  // StartSSL returns 0 if successful, or non-zero on failure.
  // If StartSSL is called while the socket is closed or connecting, the SSL
  // negotiation will begin as soon as the socket connects.
  //
  // restartable is not implemented, and must be set to false.
  virtual int StartSSL(const char* hostname, bool restartable);

  // Create the default SSL adapter for this platform.
  static SSLSocketAdapter* Create(AsyncSocket* socket);

  virtual int Send(const void* pv, size_t cb);
  virtual int Recv(void* pv, size_t cb);

 private:
  friend class TransportSocket;

  enum SSLState {
    SSLSTATE_NONE,
    SSLSTATE_WAIT,
    SSLSTATE_CONNECTED,
  };

  enum IOState {
    IOSTATE_NONE,
    IOSTATE_PENDING,
    IOSTATE_COMPLETE,
  };

  void OnConnected(int result);
  void OnRead(int result);
  void OnWrite(int result);

  virtual void OnConnectEvent(talk_base::AsyncSocket* socket);

  int BeginSSL();

  bool ignore_bad_cert_;
  std::string hostname_;
  TransportSocket* transport_socket_;
  scoped_ptr<net::SSLClientSocket> ssl_socket_;
  net::CompletionCallbackImpl<SSLSocketAdapter> connected_callback_;
  net::CompletionCallbackImpl<SSLSocketAdapter> read_callback_;
  net::CompletionCallbackImpl<SSLSocketAdapter> write_callback_;
  SSLState ssl_state_;
  IOState read_state_;
  IOState write_state_;
  scoped_refptr<net::IOBuffer> transport_buf_;
  int data_transferred_;

  DISALLOW_COPY_AND_ASSIGN(SSLSocketAdapter);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_SSL_SOCKET_ADAPTER_H_
