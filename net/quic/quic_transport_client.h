// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_TRANSPORT_CLIENT_H_
#define NET_QUIC_QUIC_TRANSPORT_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_context.h"
#include "net/socket/client_socket_factory.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quic/quic_transport/quic_transport_client_session.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class ProxyResolutionRequest;
class QuicChromiumAlarmFactory;
class URLRequestContext;

struct NET_EXPORT QuicTransportError {
  // |net_error| is always set to a meaningful value.
  int net_error = OK;

  // |quic_error| is set to a QUIC error, or to quic::QUIC_NO_ERROR if the error
  // originates non-QUIC parts of the stack.
  quic::QuicErrorCode quic_error = quic::QUIC_NO_ERROR;

  // Human-readable error summary.
  std::string details;

  // QuicTransport requires that the connection errors have to be
  // undistinguishable until the peer is confirmed to be a QuicTransport
  // endpoint.  See https://wicg.github.io/web-transport/#protocol-security
  bool safe_to_report_details = false;
};

NET_EXPORT
std::string QuicTransportErrorToString(const QuicTransportError& error);

NET_EXPORT
std::ostream& operator<<(std::ostream& os, const QuicTransportError& error);

// QuicTransportClient is the top-level API for QuicTransport in //net.
class NET_EXPORT QuicTransportClient
    : public quic::QuicTransportClientSession::ClientVisitor,
      public QuicChromiumPacketReader::Visitor,
      public QuicChromiumPacketWriter::Delegate,
      public quic::QuicSession::Visitor {
 public:
  //
  // Diagram of allowed state transitions:
  //
  //    NEW -> CONNECTING -> CONNECTED -> CLOSED
  //              |                |
  //              |                |
  //              +---> FAILED <---+
  //
  enum State {
    // The client object has been created but Connect() has not been called.
    NEW,
    // Connection establishment is in progress.  No application data can be sent
    // or received at this point.
    CONNECTING,
    // The connection has been established and application data can be sent and
    // received.
    CONNECTED,
    // The connection has been closed gracefully by either endpoint.
    CLOSED,
    // The connection has been closed abruptly.
    FAILED,
  };

  class NET_EXPORT Visitor {
   public:
    virtual ~Visitor();

    // State change notifiers.
    virtual void OnConnected() = 0;         // CONNECTING -> CONNECTED
    virtual void OnConnectionFailed() = 0;  // CONNECTING -> FAILED
    virtual void OnClosed() = 0;            // CONNECTED -> CLOSED
    virtual void OnError() = 0;             // CONNECTED -> FAILED

    virtual void OnIncomingBidirectionalStreamAvailable() = 0;
    virtual void OnIncomingUnidirectionalStreamAvailable() = 0;
    virtual void OnDatagramReceived(base::StringPiece datagram) = 0;
    virtual void OnCanCreateNewOutgoingBidirectionalStream() = 0;
    virtual void OnCanCreateNewOutgoingUnidirectionalStream() = 0;
  };

  // QUIC protocol version that is used in the origin trial.
  static constexpr quic::ParsedQuicVersion kQuicVersionForOriginTrial =
      quic::ParsedQuicVersion(quic::PROTOCOL_TLS1_3,
                              quic::QUIC_VERSION_IETF_DRAFT_27);

  // |visitor| and |context| must outlive this object.
  QuicTransportClient(const GURL& url,
                      const url::Origin& origin,
                      Visitor* visitor,
                      const NetworkIsolationKey& isolation_key,
                      URLRequestContext* context);
  ~QuicTransportClient() override;

  State state() const { return state_; }
  const QuicTransportError& error() const { return error_; }

  // Connect() is an asynchronous operation.  Once the operation is finished,
  // OnConnected() or OnConnectionFailed() is called on the Visitor.
  void Connect();

  quic::QuicTransportClientSession* session();

  // QuicTransportClientSession::ClientVisitor methods.
  void OnSessionReady() override;
  void OnIncomingBidirectionalStreamAvailable() override;
  void OnIncomingUnidirectionalStreamAvailable() override;
  void OnDatagramReceived(quiche::QuicheStringPiece datagram) override;
  void OnCanCreateNewOutgoingBidirectionalStream() override;
  void OnCanCreateNewOutgoingUnidirectionalStream() override;

  // QuicChromiumPacketReader::Visitor methods.
  void OnReadError(int result, const DatagramClientSocket* socket) override;
  bool OnPacket(const quic::QuicReceivedPacket& packet,
                const quic::QuicSocketAddress& local_address,
                const quic::QuicSocketAddress& peer_address) override;

  // QuicChromiumPacketWriter::Delegate methods.
  int HandleWriteError(int error_code,
                       scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer>
                           last_packet) override;
  void OnWriteError(int error_code) override;
  void OnWriteUnblocked() override;

  // QuicSession::Visitor methods.
  void OnConnectionClosed(quic::QuicConnectionId server_connection_id,
                          quic::QuicErrorCode error,
                          const std::string& error_details,
                          quic::ConnectionCloseSource source) override;
  void OnWriteBlocked(
      quic::QuicBlockedWriterInterface* /*blocked_writer*/) override {}
  void OnRstStreamReceived(const quic::QuicRstStreamFrame& /*frame*/) override {
  }
  void OnStopSendingReceived(
      const quic::QuicStopSendingFrame& /*frame*/) override {}

 private:
  // State of the connection establishment process.
  enum ConnectState {
    CONNECT_STATE_NONE,
    CONNECT_STATE_INIT,
    CONNECT_STATE_CHECK_PROXY,
    CONNECT_STATE_CHECK_PROXY_COMPLETE,
    CONNECT_STATE_RESOLVE_HOST,
    CONNECT_STATE_RESOLVE_HOST_COMPLETE,
    CONNECT_STATE_CONNECT,
    CONNECT_STATE_CONFIRM_CONNECTION,
  };

  // DoLoop processing the Connect() call.
  void DoLoop(int rv);
  // Verifies the basic preconditions for setting up the connection.
  int DoInit();
  // Verifies that there is no mandatory proxy configured for the specified URL.
  int DoCheckProxy();
  int DoCheckProxyComplete(int rv);
  // Resolves the hostname in the URL.
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  // Establishes the QUIC connection.
  int DoConnect();
  // Verifies that the connection has succeeded.
  int DoConfirmConnection();

  void TransitionToState(State next_state);

  const GURL url_;
  const url::Origin origin_;
  const NetworkIsolationKey isolation_key_;
  URLRequestContext* const context_;  // Unowned.
  Visitor* const visitor_;            // Unowned.

  ClientSocketFactory* const client_socket_factory_;
  QuicContext* const quic_context_;
  NetLogWithSource net_log_;
  base::SequencedTaskRunner* task_runner_;

  quic::ParsedQuicVersionVector supported_versions_;
  // TODO(vasilvv): move some of those into QuicContext.
  std::unique_ptr<QuicChromiumAlarmFactory> alarm_factory_;
  quic::QuicCryptoClientConfig crypto_config_;

  State state_ = NEW;
  ConnectState next_connect_state_ = CONNECT_STATE_NONE;
  QuicTransportError error_;

  ProxyInfo proxy_info_;
  std::unique_ptr<ProxyResolutionRequest> proxy_resolution_request_;
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;

  std::unique_ptr<DatagramClientSocket> socket_;
  std::unique_ptr<quic::QuicConnection> connection_;
  std::unique_ptr<quic::QuicTransportClientSession> session_;
  std::unique_ptr<QuicChromiumPacketReader> packet_reader_;

  base::WeakPtrFactory<QuicTransportClient> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_TRANSPORT_CLIENT_H_
