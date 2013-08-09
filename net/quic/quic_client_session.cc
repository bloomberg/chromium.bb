// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session.h"

#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_stream_factory.h"
#include "net/ssl/ssl_info.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

namespace {

// Note: these values must be kept in sync with the corresponding values in:
// tools/metrics/histograms/histograms.xml
enum HandshakeState {
  STATE_STARTED = 0,
  STATE_ENCRYPTION_ESTABLISHED = 1,
  STATE_HANDSHAKE_CONFIRMED = 2,
  STATE_FAILED = 3,
  NUM_HANDSHAKE_STATES = 4
};

void RecordHandshakeState(HandshakeState state) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicHandshakeState", state,
                            NUM_HANDSHAKE_STATES);
}

}  // namespace

QuicClientSession::StreamRequest::StreamRequest() : stream_(NULL) {}

QuicClientSession::StreamRequest::~StreamRequest() {
  CancelRequest();
}

int QuicClientSession::StreamRequest::StartRequest(
    const base::WeakPtr<QuicClientSession> session,
    QuicReliableClientStream** stream,
    const CompletionCallback& callback) {
  session_ = session;
  stream_ = stream;
  int rv = session_->TryCreateStream(this, stream_);
  if (rv == ERR_IO_PENDING) {
    callback_ = callback;
  }

  return rv;
}

void QuicClientSession::StreamRequest::CancelRequest() {
  if (session_)
    session_->CancelRequest(this);
  session_.reset();
  callback_.Reset();
}

void QuicClientSession::StreamRequest::OnRequestCompleteSuccess(
    QuicReliableClientStream* stream) {
  session_.reset();
  *stream_ = stream;
  ResetAndReturn(&callback_).Run(OK);
}

void QuicClientSession::StreamRequest::OnRequestCompleteFailure(int rv) {
  session_.reset();
  ResetAndReturn(&callback_).Run(rv);
}

QuicClientSession::QuicClientSession(
    QuicConnection* connection,
    DatagramClientSocket* socket,
    QuicStreamFactory* stream_factory,
    QuicCryptoClientStreamFactory* crypto_client_stream_factory,
    const string& server_hostname,
    const QuicConfig& config,
    QuicCryptoClientConfig* crypto_config,
    NetLog* net_log)
    : QuicSession(connection, config, false),
      weak_factory_(this),
      stream_factory_(stream_factory),
      socket_(socket),
      read_buffer_(new IOBufferWithSize(kMaxPacketSize)),
      read_pending_(false),
      num_total_streams_(0),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_QUIC_SESSION)),
      logger_(net_log_) {
  crypto_stream_.reset(
      crypto_client_stream_factory ?
          crypto_client_stream_factory->CreateQuicCryptoClientStream(
              server_hostname, this, crypto_config) :
          new QuicCryptoClientStream(server_hostname, this, crypto_config));

  connection->set_debug_visitor(&logger_);
  // TODO(rch): pass in full host port proxy pair
  net_log_.BeginEvent(
      NetLog::TYPE_QUIC_SESSION,
      NetLog::StringCallback("host", &server_hostname));
}

QuicClientSession::~QuicClientSession() {
  connection()->set_debug_visitor(NULL);
  net_log_.EndEvent(NetLog::TYPE_QUIC_SESSION);

  while (!stream_requests_.empty()) {
    StreamRequest* request = stream_requests_.front();
    stream_requests_.pop_front();
    request->OnRequestCompleteFailure(ERR_ABORTED);
  }

  if (IsEncryptionEstablished())
    RecordHandshakeState(STATE_ENCRYPTION_ESTABLISHED);
  if (IsCryptoHandshakeConfirmed())
    RecordHandshakeState(STATE_HANDSHAKE_CONFIRMED);
  else
    RecordHandshakeState(STATE_FAILED);

  UMA_HISTOGRAM_COUNTS("Net.QuicNumSentClientHellos",
                       crypto_stream_->num_sent_client_hellos());
  if (IsCryptoHandshakeConfirmed()) {
    UMA_HISTOGRAM_COUNTS("Net.QuicNumSentClientHellosCryptoHandshakeConfirmed",
                         crypto_stream_->num_sent_client_hellos());
  }
}

int QuicClientSession::TryCreateStream(StreamRequest* request,
                                       QuicReliableClientStream** stream) {
  if (!crypto_stream_->encryption_established()) {
    DLOG(DFATAL) << "Encryption not established.";
    return ERR_CONNECTION_CLOSED;
  }

  if (goaway_received()) {
    DLOG(INFO) << "Going away.";
    return ERR_CONNECTION_CLOSED;
  }

  if (!connection()->connected()) {
    DLOG(INFO) << "Already closed.";
    return ERR_CONNECTION_CLOSED;
  }

  if (GetNumOpenStreams() < get_max_open_streams()) {
    *stream = CreateOutgoingReliableStreamImpl();
    return OK;
  }

  stream_requests_.push_back(request);
  return ERR_IO_PENDING;
}

void QuicClientSession::CancelRequest(StreamRequest* request) {
  // Remove |request| from the queue while preserving the order of the
  // other elements.
  StreamRequestQueue::iterator it =
      std::find(stream_requests_.begin(), stream_requests_.end(), request);
  if (it != stream_requests_.end()) {
    it = stream_requests_.erase(it);
  }
}

QuicReliableClientStream* QuicClientSession::CreateOutgoingReliableStream() {
  if (!crypto_stream_->encryption_established()) {
    DLOG(INFO) << "Encryption not active so no outgoing stream created.";
    return NULL;
  }
  if (GetNumOpenStreams() >= get_max_open_streams()) {
    DLOG(INFO) << "Failed to create a new outgoing stream. "
               << "Already " << GetNumOpenStreams() << " open.";
    return NULL;
  }
  if (goaway_received()) {
    DLOG(INFO) << "Failed to create a new outgoing stream. "
               << "Already received goaway.";
    return NULL;
  }

  return CreateOutgoingReliableStreamImpl();
}

QuicReliableClientStream*
QuicClientSession::CreateOutgoingReliableStreamImpl() {
  DCHECK(connection()->connected());
  QuicReliableClientStream* stream =
      new QuicReliableClientStream(GetNextStreamId(), this, net_log_);
  ActivateStream(stream);
  ++num_total_streams_;
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return crypto_stream_.get();
};

bool QuicClientSession::GetSSLInfo(SSLInfo* ssl_info) {
  DCHECK(crypto_stream_.get());
  return crypto_stream_->GetSSLInfo(ssl_info);
}

int QuicClientSession::CryptoConnect(const CompletionCallback& callback) {
  RecordHandshakeState(STATE_STARTED);
  if (!crypto_stream_->CryptoConnect()) {
    // TODO(wtc): change crypto_stream_.CryptoConnect() to return a
    // QuicErrorCode and map it to a net error code.
    return ERR_CONNECTION_FAILED;
  }

  if (IsEncryptionEstablished()) {
    return OK;
  }

  callback_ = callback;
  return ERR_IO_PENDING;
}

ReliableQuicStream* QuicClientSession::CreateIncomingReliableStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return NULL;
}

void QuicClientSession::CloseStream(QuicStreamId stream_id) {
  QuicSession::CloseStream(stream_id);

  if (GetNumOpenStreams() < get_max_open_streams() &&
      !stream_requests_.empty() &&
      crypto_stream_->encryption_established() &&
      !goaway_received() &&
      connection()->connected()) {
    StreamRequest* request = stream_requests_.front();
    stream_requests_.pop_front();
    request->OnRequestCompleteSuccess(CreateOutgoingReliableStreamImpl());
  }

  if (GetNumOpenStreams() == 0) {
    stream_factory_->OnIdleSession(this);
  }
}

void QuicClientSession::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  if (!callback_.is_null()) {
    // TODO(rtenneti): Currently for all CryptoHandshakeEvent events, callback_
    // could be called because there are no error events in CryptoHandshakeEvent
    // enum. If error events are added to CryptoHandshakeEvent, then the
    // following code needs to changed.
    base::ResetAndReturn(&callback_).Run(OK);
  }
  QuicSession::OnCryptoHandshakeEvent(event);
}

void QuicClientSession::ConnectionClose(QuicErrorCode error, bool from_peer) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.ConnectionCloseErrorCode",
                              error);
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.QuicVersion",
                              connection()->version());
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(ERR_QUIC_PROTOCOL_ERROR);
  }
  QuicSession::ConnectionClose(error, from_peer);
  NotifyFactoryOfSessionCloseLater();
}

void QuicClientSession::StartReading() {
  if (read_pending_) {
    return;
  }
  read_pending_ = true;
  int rv = socket_->Read(read_buffer_.get(),
                         read_buffer_->size(),
                         base::Bind(&QuicClientSession::OnReadComplete,
                                    weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING) {
    return;
  }

  // Data was read, process it.
  // Schedule the work through the message loop to avoid recursive
  // callbacks.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&QuicClientSession::OnReadComplete,
                 weak_factory_.GetWeakPtr(), rv));
}

void QuicClientSession::CloseSessionOnError(int error) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.CloseSessionOnError", -error);
  CloseSessionOnErrorInner(error);
  NotifyFactoryOfSessionClose();
}

void QuicClientSession::CloseSessionOnErrorInner(int error) {
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(error);
  }
  while (!streams()->empty()) {
    ReliableQuicStream* stream = streams()->begin()->second;
    QuicStreamId id = stream->id();
    static_cast<QuicReliableClientStream*>(stream)->OnError(error);
    CloseStream(id);
  }
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CLOSE_ON_ERROR,
      NetLog::IntegerCallback("net_error", error));

  connection()->CloseConnection(QUIC_INTERNAL_ERROR, false);
  DCHECK(!connection()->connected());
}

base::Value* QuicClientSession::GetInfoAsValue(const HostPortPair& pair) const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("host_port_pair", pair.ToString());
  dict->SetString("version", QuicVersionToString(connection()->version()));
  dict->SetInteger("open_streams", GetNumOpenStreams());
  dict->SetInteger("total_streams", num_total_streams_);
  dict->SetString("peer_address", peer_address().ToString());
  dict->SetString("guid", base::Uint64ToString(guid()));
  return dict;
}

base::WeakPtr<QuicClientSession> QuicClientSession::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void QuicClientSession::OnReadComplete(int result) {
  read_pending_ = false;
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result < 0) {
    DLOG(INFO) << "Closing session on read error: " << result;
    CloseSessionOnErrorInner(result);
    NotifyFactoryOfSessionCloseLater();
    return;
  }

  scoped_refptr<IOBufferWithSize> buffer(read_buffer_);
  read_buffer_ = new IOBufferWithSize(kMaxPacketSize);
  QuicEncryptedPacket packet(buffer->data(), result);
  IPEndPoint local_address;
  IPEndPoint peer_address;
  socket_->GetLocalAddress(&local_address);
  socket_->GetPeerAddress(&peer_address);
  // ProcessUdpPacket might result in |this| being deleted, so we
  // use a weak pointer to be safe.
  connection()->ProcessUdpPacket(local_address, peer_address, packet);
  if (!connection()->connected()) {
    stream_factory_->OnSessionClose(this);
    return;
  }
  StartReading();
}

void QuicClientSession::NotifyFactoryOfSessionCloseLater() {
  DCHECK_EQ(0u, GetNumOpenStreams());
  DCHECK(!connection()->connected());
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&QuicClientSession::NotifyFactoryOfSessionClose,
                 weak_factory_.GetWeakPtr()));
}

void QuicClientSession::NotifyFactoryOfSessionClose() {
  DCHECK_EQ(0u, GetNumOpenStreams());
  DCHECK(stream_factory_);
  // Will delete |this|.
  stream_factory_->OnSessionClose(this);
}

}  // namespace net
