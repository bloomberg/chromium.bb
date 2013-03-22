// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A client specific QuicSession subclass.  This class owns the underlying
// QuicConnection and QuicConnectionHelper objects.  The connection stores
// a non-owning pointer to the helper so this session needs to ensure that
// the helper outlives the connection.

#ifndef NET_QUIC_QUIC_CLIENT_SESSION_H_
#define NET_QUIC_QUIC_CLIENT_SESSION_H_

#include <string>

#include "base/hash_tables.h"
#include "net/base/completion_callback.h"
#include "net/quic/quic_connection_logger.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_reliable_client_stream.h"
#include "net/quic/quic_session.h"

namespace net {

class DatagramClientSocket;
class QuicConnectionHelper;
class QuicCryptoClientStreamFactory;
class QuicStreamFactory;

class NET_EXPORT_PRIVATE QuicClientSession : public QuicSession {
 public:
  // Constructs a new session which will own |connection| and |helper|, but
  // not |stream_factory|, which must outlive this session.
  // TODO(rch): decouple the factory from the session via a Delegate interface.
  QuicClientSession(QuicConnection* connection,
                    DatagramClientSocket* socket,
                    QuicStreamFactory* stream_factory,
                    QuicCryptoClientStreamFactory* crypto_client_stream_factory,
                    const std::string& server_hostname,
                    NetLog* net_log);

  virtual ~QuicClientSession();

  // QuicSession methods:
  virtual QuicReliableClientStream* CreateOutgoingReliableStream() OVERRIDE;
  virtual QuicCryptoClientStream* GetCryptoStream() OVERRIDE;
  virtual void CloseStream(QuicStreamId stream_id) OVERRIDE;
  virtual void OnCryptoHandshakeComplete(QuicErrorCode error) OVERRIDE;

  // Performs a crypto handshake with the server.
  int CryptoConnect(const CompletionCallback& callback);

  // Causes the QuicConnectionHelper to start reading from the socket
  // and passing the data along to the QuicConnection.
  void StartReading();

  // Close the session because of |error|.
  void CloseSessionOnError(int error);

  base::Value* GetInfoAsValue(const HostPortPair& pair) const;

  const BoundNetLog& net_log() const { return net_log_; }

 protected:
  // QuicSession methods:
  virtual ReliableQuicStream* CreateIncomingReliableStream(
      QuicStreamId id) OVERRIDE;

 private:
  // A completion callback invoked when a read completes.
  void OnReadComplete(int result);

  base::WeakPtrFactory<QuicClientSession> weak_factory_;
  scoped_ptr<QuicCryptoClientStream> crypto_stream_;
  QuicStreamFactory* stream_factory_;
  scoped_ptr<DatagramClientSocket> socket_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  bool read_pending_;
  CompletionCallback callback_;
  size_t num_total_streams_;
  BoundNetLog net_log_;
  QuicConnectionLogger logger_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientSession);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CLIENT_SESSION_H_
