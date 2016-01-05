// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SERVER_SOCKET_OPENSSL_H_
#define NET_SOCKET_SSL_SERVER_SOCKET_OPENSSL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/log/net_log.h"
#include "net/socket/ssl_server_socket.h"
#include "net/ssl/ssl_server_config.h"

// Avoid including misc OpenSSL headers, i.e.:
// <openssl/bio.h>
typedef struct bio_st BIO;
// <openssl/ssl.h>
typedef struct ssl_st SSL;

namespace net {

class SSLInfo;

class SSLServerSocketOpenSSL : public SSLServerSocket {
 public:
  // See comments on CreateSSLServerSocket for details of how these
  // parameters are used.
  SSLServerSocketOpenSSL(scoped_ptr<StreamSocket> socket,
                         scoped_refptr<X509Certificate> certificate,
                         const crypto::RSAPrivateKey& key,
                         const SSLServerConfig& ssl_config);
  ~SSLServerSocketOpenSSL() override;

  // SSLServerSocket interface.
  int Handshake(const CompletionCallback& callback) override;

  // SSLSocket interface.
  int ExportKeyingMaterial(const base::StringPiece& label,
                           bool has_context,
                           const base::StringPiece& context,
                           unsigned char* out,
                           unsigned int outlen) override;
  int GetTLSUniqueChannelBinding(std::string* out) override;

  // Socket interface (via StreamSocket).
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  // StreamSocket implementation.
  int Connect(const CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const BoundNetLog& NetLog() const override;
  void SetSubresourceSpeculation() override;
  void SetOmniboxSpeculation() override;
  bool WasEverUsed() const override;
  bool UsingTCPFastOpen() const override;
  bool WasNpnNegotiated() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {}
  int64_t GetTotalReceivedBytes() const override;

 private:
  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
  };

  void OnSendComplete(int result);
  void OnRecvComplete(int result);
  void OnHandshakeIOComplete(int result);

  int BufferSend();
  void BufferSendComplete(int result);
  void TransportWriteComplete(int result);
  int BufferRecv();
  void BufferRecvComplete(int result);
  int TransportReadComplete(int result);
  bool DoTransportIO();
  int DoPayloadRead();
  int DoPayloadWrite();

  int DoHandshakeLoop(int last_io_result);
  int DoReadLoop(int result);
  int DoWriteLoop(int result);
  int DoHandshake();
  void DoHandshakeCallback(int result);
  void DoReadCallback(int result);
  void DoWriteCallback(int result);

  int Init();

  // Members used to send and receive buffer.
  bool transport_send_busy_;
  bool transport_recv_busy_;
  bool transport_recv_eof_;

  scoped_refptr<DrainableIOBuffer> send_buffer_;
  scoped_refptr<IOBuffer> recv_buffer_;

  BoundNetLog net_log_;

  CompletionCallback user_handshake_callback_;
  CompletionCallback user_read_callback_;
  CompletionCallback user_write_callback_;

  // Used by Read function.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // Used by Write function.
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;

  // Used by TransportWriteComplete() and TransportReadComplete() to signify an
  // error writing to the transport socket. A value of OK indicates no error.
  int transport_write_error_;

  // OpenSSL stuff
  SSL* ssl_;
  BIO* transport_bio_;

  // StreamSocket for sending and receiving data.
  scoped_ptr<StreamSocket> transport_socket_;

  // Options for the SSL socket.
  SSLServerConfig ssl_config_;

  // Certificate for the server.
  scoped_refptr<X509Certificate> cert_;

  // Private key used by the server.
  scoped_ptr<crypto::RSAPrivateKey> key_;

  State next_handshake_state_;
  bool completed_handshake_;

  DISALLOW_COPY_AND_ASSIGN(SSLServerSocketOpenSSL);
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SERVER_SOCKET_OPENSSL_H_
