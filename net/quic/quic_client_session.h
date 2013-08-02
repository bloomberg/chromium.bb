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

#include "base/containers/hash_tables.h"
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
class SSLInfo;

namespace test {
class QuicClientSessionPeer;
}  // namespace test

class NET_EXPORT_PRIVATE QuicClientSession : public QuicSession {
 public:
  // A helper class used to manage a request to create a stream.
  class NET_EXPORT_PRIVATE StreamRequest {
   public:
    StreamRequest();
    ~StreamRequest();

    // Starts a request to create a stream.  If OK is returned, then
    // |stream| will be updated with the newly created stream.  If
    // ERR_IO_PENDING is returned, then when the request is eventuallly
    // complete |callback| will be called.
    int StartRequest(const base::WeakPtr<QuicClientSession> session,
                     QuicReliableClientStream** stream,
                     const CompletionCallback& callback);

    // Cancels any pending stream creation request. May be called
    // repeatedly.
    void CancelRequest();

   private:
    friend class QuicClientSession;

    // Called by |session_| for an asynchronous request when the stream
    // request has finished successfully.
    void OnRequestCompleteSuccess(QuicReliableClientStream* stream);

    // Called by |session_| for an asynchronous request when the stream
    // request has finished with an error. Also called with ERR_ABORTED
    // if |session_| is destroyed while the stream request is still pending.
    void OnRequestCompleteFailure(int rv);

    base::WeakPtr<QuicClientSession> session_;
    CompletionCallback callback_;
    QuicReliableClientStream** stream_;

    DISALLOW_COPY_AND_ASSIGN(StreamRequest);
  };

  // Constructs a new session which will own |connection| and |helper|, but
  // not |stream_factory|, which must outlive this session.
  // TODO(rch): decouple the factory from the session via a Delegate interface.
  QuicClientSession(QuicConnection* connection,
                    DatagramClientSocket* socket,
                    QuicStreamFactory* stream_factory,
                    QuicCryptoClientStreamFactory* crypto_client_stream_factory,
                    const std::string& server_hostname,
                    const QuicConfig& config,
                    QuicCryptoClientConfig* crypto_config,
                    NetLog* net_log);

  virtual ~QuicClientSession();

  // Attempts to create a new stream.  If the stream can be
  // created immediately, returns OK.  If the open stream limit
  // has been reached, returns ERR_IO_PENDING, and |request|
  // will be added to the stream requets queue and will
  // be completed asynchronously.
  // TODO(rch): remove |stream| from this and use setter on |request|
  // and fix in spdy too.
  int TryCreateStream(StreamRequest* request,
                      QuicReliableClientStream** stream);

  // Cancels the pending stream creation request.
  void CancelRequest(StreamRequest* request);

  // QuicSession methods:
  virtual QuicReliableClientStream* CreateOutgoingReliableStream() OVERRIDE;
  virtual QuicCryptoClientStream* GetCryptoStream() OVERRIDE;
  virtual void CloseStream(QuicStreamId stream_id) OVERRIDE;
  virtual void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) OVERRIDE;
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE;

  // QuicConnectionVisitorInterface methods:
  virtual void ConnectionClose(QuicErrorCode error, bool from_peer) OVERRIDE;

  // Performs a crypto handshake with the server.
  int CryptoConnect(const CompletionCallback& callback);

  // Causes the QuicConnectionHelper to start reading from the socket
  // and passing the data along to the QuicConnection.
  void StartReading();

  // Close the session because of |error| and notifies the factory
  // that this session has been closed, which will delete the session.
  void CloseSessionOnError(int error);

  base::Value* GetInfoAsValue(const HostPortPair& pair) const;

  const BoundNetLog& net_log() const { return net_log_; }

  base::WeakPtr<QuicClientSession> GetWeakPtr();

 protected:
  // QuicSession methods:
  virtual ReliableQuicStream* CreateIncomingReliableStream(
      QuicStreamId id) OVERRIDE;

 private:
  friend class test::QuicClientSessionPeer;

  typedef std::list<StreamRequest*> StreamRequestQueue;

  QuicReliableClientStream* CreateOutgoingReliableStreamImpl();
  // A completion callback invoked when a read completes.
  void OnReadComplete(int result);

  void CloseSessionOnErrorInner(int error);

  // Posts a task to notify the factory that this session has been closed.
  void NotifyFactoryOfSessionCloseLater();

  // Notifies the factory that this session has been closed which will
  // delete |this|.
  void NotifyFactoryOfSessionClose();

  base::WeakPtrFactory<QuicClientSession> weak_factory_;
  scoped_ptr<QuicCryptoClientStream> crypto_stream_;
  QuicStreamFactory* stream_factory_;
  scoped_ptr<DatagramClientSocket> socket_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  StreamRequestQueue stream_requests_;
  bool read_pending_;
  CompletionCallback callback_;
  size_t num_total_streams_;
  BoundNetLog net_log_;
  QuicConnectionLogger logger_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientSession);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CLIENT_SESSION_H_
