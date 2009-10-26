// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_SSL_SOCKET_ADAPTER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_SSL_SOCKET_ADAPTER_H_

#include "base/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket.h"
#include "net/socket/ssl_client_socket.h"
#include "talk/base/asyncsocket.h"

namespace notifier {

class SSLSocketAdapter;

// This class provides a wrapper to libjingle's talk_base::AsyncSocket that
// implements Chromium's net::ClientSocket interface.  It's used by
// SSLSocketAdapter to enable Chromium's SSL implementation to work over
// libjingle's socket class.
class TransportSocket : public net::ClientSocket, public sigslot::has_slots<> {
 public:
  TransportSocket(talk_base::AsyncSocket* socket,
                  SSLSocketAdapter *ssl_adapter);

  void set_addr(const talk_base::SocketAddress& addr) {
    addr_ = addr;
  }

  // net::ClientSocket implementation

  virtual int Connect(net::CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;

#if defined(OS_LINUX) || defined(OS_MACOSX)
  virtual int GetPeerName(struct sockaddr *name, socklen_t *namelen);
#endif

  // net::Socket implementation

  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  void OnConnectEvent(talk_base::AsyncSocket * socket);
  void OnReadEvent(talk_base::AsyncSocket * socket);
  void OnWriteEvent(talk_base::AsyncSocket * socket);
  void OnCloseEvent(talk_base::AsyncSocket * socket, int err);

  net::CompletionCallback* connect_callback_;
  net::CompletionCallback* read_callback_;
  net::CompletionCallback* write_callback_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_len_;
  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_len_;

  talk_base::AsyncSocket *socket_;
  talk_base::SocketAddress addr_;
  SSLSocketAdapter *ssl_adapter_;

  DISALLOW_COPY_AND_ASSIGN(TransportSocket);
};

// This provides a talk_base::AsyncSocketAdapter interface around Chromium's
// net::SSLClientSocket class.  This allows notifier to use Chromium's SSL
// implementation instead of OpenSSL.
class SSLSocketAdapter : public talk_base::AsyncSocketAdapter {
 public:
  explicit SSLSocketAdapter(talk_base::AsyncSocket* socket);

  bool ignore_bad_cert() const { return ignore_bad_cert_; }
  void set_ignore_bad_cert(bool ignore) { ignore_bad_cert_ = ignore; }

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

  enum State {
    STATE_NONE,
    STATE_READ,
    STATE_READ_COMPLETE,
    STATE_WRITE,
    STATE_WRITE_COMPLETE
  };

  void OnConnected(int result);
  void OnIO(int result);

  bool ignore_bad_cert_;
  scoped_ptr<TransportSocket> socket_;
  scoped_ptr<net::SSLClientSocket> ssl_socket_;
  net::CompletionCallbackImpl<SSLSocketAdapter> connected_callback_;
  net::CompletionCallbackImpl<SSLSocketAdapter> io_callback_;
  bool ssl_connected_;
  State state_;
  scoped_refptr<net::IOBuffer> transport_buf_;
  int data_transferred_;

  DISALLOW_COPY_AND_ASSIGN(SSLSocketAdapter);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_SSL_SOCKET_ADAPTER_H_
