// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This StreamSocket implementation is to be used with servers that
// accept connections on port 443 but don't really use SSL.  For
// example, the Google Talk servers do this to bypass proxies.  (The
// connection is upgraded to TLS as part of the XMPP negotiation, so
// security is preserved.)  A "fake" SSL handshake is done immediately
// after connection to fool proxies into thinking that this is a real
// SSL connection.
//
// NOTE: This StreamSocket implementation does *not* do a real SSL
// handshake nor does it do any encryption!

#ifndef JINGLE_NOTIFIER_BASE_FAKE_SSL_CLIENT_SOCKET_H_
#define JINGLE_NOTIFIER_BASE_FAKE_SSL_CLIENT_SOCKET_H_
#pragma once

#include <cstddef>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace net {
class DrainableIOBuffer;
}  // namespace net

namespace notifier {

class FakeSSLClientSocket : public net::StreamSocket {
 public:
  // Takes ownership of |transport_socket|.
  explicit FakeSSLClientSocket(net::StreamSocket* transport_socket);

  virtual ~FakeSSLClientSocket();

  // Exposed for testing.
  static base::StringPiece GetSslClientHello();
  static base::StringPiece GetSslServerHello();

  // net::StreamSocket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;
  virtual int Connect(net::OldCompletionCallback* callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(net::AddressList* address) const OVERRIDE;
  virtual int GetLocalAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual const net::BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;

 private:
  enum HandshakeState {
    STATE_NONE,
    STATE_CONNECT,
    STATE_SEND_CLIENT_HELLO,
    STATE_VERIFY_SERVER_HELLO,
  };

  int DoHandshakeLoop();
  void RunUserConnectCallback(int status);
  void DoHandshakeLoopWithUserConnectCallback();

  int DoConnect();
  void OnConnectDone(int status);
  void ProcessConnectDone();

  int DoSendClientHello();
  void OnSendClientHelloDone(int status);
  void ProcessSendClientHelloDone(size_t written);

  int DoVerifyServerHello();
  void OnVerifyServerHelloDone(int status);
  net::Error ProcessVerifyServerHelloDone(size_t read);

  // Callbacks passed to |transport_socket_|.
  net::OldCompletionCallbackImpl<FakeSSLClientSocket> connect_callback_;
  net::OldCompletionCallbackImpl<FakeSSLClientSocket>
      send_client_hello_callback_;
  net::OldCompletionCallbackImpl<FakeSSLClientSocket>
      verify_server_hello_callback_;

  scoped_ptr<net::StreamSocket> transport_socket_;

  // During the handshake process, holds a value from HandshakeState.
  // STATE_NONE otherwise.
  HandshakeState next_handshake_state_;

  // True iff we're connected and we've finished the handshake.
  bool handshake_completed_;

  // The callback passed to Connect().
  net::OldCompletionCallback* user_connect_callback_;

  scoped_refptr<net::DrainableIOBuffer> write_buf_;
  scoped_refptr<net::DrainableIOBuffer> read_buf_;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_FAKE_SSL_CLIENT_SOCKET_H_
