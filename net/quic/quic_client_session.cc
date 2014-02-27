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
#include "net/quic/quic_default_packet_writer.h"
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
    const base::WeakPtr<QuicClientSession>& session,
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
    scoped_ptr<DatagramClientSocket> socket,
    scoped_ptr<QuicDefaultPacketWriter> writer,
    QuicStreamFactory* stream_factory,
    QuicCryptoClientStreamFactory* crypto_client_stream_factory,
    const string& server_hostname,
    const QuicConfig& config,
    QuicCryptoClientConfig* crypto_config,
    NetLog* net_log)
    : QuicSession(connection, config),
      require_confirmation_(false),
      stream_factory_(stream_factory),
      socket_(socket.Pass()),
      writer_(writer.Pass()),
      read_buffer_(new IOBufferWithSize(kMaxPacketSize)),
      read_pending_(false),
      num_total_streams_(0),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_QUIC_SESSION)),
      logger_(net_log_),
      num_packets_read_(0),
      weak_factory_(this) {
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
  // The session must be closed before it is destroyed.
  DCHECK(streams()->empty());
  CloseAllStreams(ERR_UNEXPECTED);
  DCHECK(observers_.empty());
  CloseAllObservers(ERR_UNEXPECTED);

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

  UMA_HISTOGRAM_COUNTS("Net.QuicSession.NumTotalStreams", num_total_streams_);
  UMA_HISTOGRAM_COUNTS("Net.QuicNumSentClientHellos",
                       crypto_stream_->num_sent_client_hellos());
  if (!IsCryptoHandshakeConfirmed())
    return;

  // Sending one client_hello means we had zero handshake-round-trips.
  int round_trip_handshakes = crypto_stream_->num_sent_client_hellos() - 1;

  // Don't bother with these histogram during tests, which mock out
  // num_sent_client_hellos().
  if (round_trip_handshakes < 0 || !stream_factory_)
    return;

  bool port_selected = stream_factory_->enable_port_selection();
  SSLInfo ssl_info;
  if (!crypto_stream_->GetSSLInfo(&ssl_info) || !ssl_info.cert) {
    if (port_selected) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectSelectPortForHTTP",
                                  round_trip_handshakes, 0, 3, 4);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectRandomPortForHTTP",
                                  round_trip_handshakes, 0, 3, 4);
    }
  } else {
    if (port_selected) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectSelectPortForHTTPS",
                                  round_trip_handshakes, 0, 3, 4);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectRandomPortForHTTPS",
                                  round_trip_handshakes, 0, 3, 4);
    }
  }
}

bool QuicClientSession::OnStreamFrames(
    const std::vector<QuicStreamFrame>& frames) {
  // Record total number of stream frames.
  UMA_HISTOGRAM_COUNTS("Net.QuicNumStreamFramesInPacket", frames.size());

  // Record number of frames per stream in packet.
  typedef std::map<QuicStreamId, size_t> FrameCounter;
  FrameCounter frames_per_stream;
  for (size_t i = 0; i < frames.size(); ++i) {
    frames_per_stream[frames[i].stream_id]++;
  }
  for (FrameCounter::const_iterator it = frames_per_stream.begin();
       it != frames_per_stream.end(); ++it) {
    UMA_HISTOGRAM_COUNTS("Net.QuicNumStreamFramesPerStreamInPacket",
                         it->second);
  }

  return QuicSession::OnStreamFrames(frames);
}

void QuicClientSession::AddObserver(Observer* observer) {
  DCHECK(!ContainsKey(observers_, observer));
  observers_.insert(observer);
}

void QuicClientSession::RemoveObserver(Observer* observer) {
  DCHECK(ContainsKey(observers_, observer));
  observers_.erase(observer);
}

int QuicClientSession::TryCreateStream(StreamRequest* request,
                                       QuicReliableClientStream** stream) {
  if (!crypto_stream_->encryption_established()) {
    DLOG(DFATAL) << "Encryption not established.";
    return ERR_CONNECTION_CLOSED;
  }

  if (goaway_received()) {
    DVLOG(1) << "Going away.";
    return ERR_CONNECTION_CLOSED;
  }

  if (!connection()->connected()) {
    DVLOG(1) << "Already closed.";
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

QuicReliableClientStream* QuicClientSession::CreateOutgoingDataStream() {
  if (!crypto_stream_->encryption_established()) {
    DVLOG(1) << "Encryption not active so no outgoing stream created.";
    return NULL;
  }
  if (GetNumOpenStreams() >= get_max_open_streams()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
               << "Already " << GetNumOpenStreams() << " open.";
    return NULL;
  }
  if (goaway_received()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
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
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.NumOpenStreams", GetNumOpenStreams());
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return crypto_stream_.get();
};

bool QuicClientSession::GetSSLInfo(SSLInfo* ssl_info) {
  DCHECK(crypto_stream_.get());
  return crypto_stream_->GetSSLInfo(ssl_info);
}

int QuicClientSession::CryptoConnect(bool require_confirmation,
                                     const CompletionCallback& callback) {
  require_confirmation_ = require_confirmation;
  RecordHandshakeState(STATE_STARTED);
  if (!crypto_stream_->CryptoConnect()) {
    // TODO(wtc): change crypto_stream_.CryptoConnect() to return a
    // QuicErrorCode and map it to a net error code.
    return ERR_CONNECTION_FAILED;
  }

  bool can_notify = require_confirmation_ ?
      IsCryptoHandshakeConfirmed() : IsEncryptionEstablished();
  if (can_notify) {
    return OK;
  }

  callback_ = callback;
  return ERR_IO_PENDING;
}

int QuicClientSession::GetNumSentClientHellos() const {
  return crypto_stream_->num_sent_client_hellos();
}

bool QuicClientSession::CanPool(const std::string& hostname) const {
  // TODO(rch): When QUIC supports channel ID or client certificates, this
  // logic will need to be revised.
  DCHECK(connection()->connected());
  SSLInfo ssl_info;
  bool unused = false;
  DCHECK(crypto_stream_);
  if (!crypto_stream_->GetSSLInfo(&ssl_info) || !ssl_info.cert) {
    // We can always pool with insecure QUIC sessions.
    return true;
  }
  // Only pool secure QUIC sessions if the cert matches the new hostname.
  return ssl_info.cert->VerifyNameMatch(hostname, &unused);
}

QuicDataStream* QuicClientSession::CreateIncomingDataStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return NULL;
}

void QuicClientSession::CloseStream(QuicStreamId stream_id) {
  QuicSession::CloseStream(stream_id);
  OnClosedStream();
}

void QuicClientSession::SendRstStream(QuicStreamId id,
                                      QuicRstStreamErrorCode error,
                                      QuicStreamOffset bytes_written) {
  QuicSession::SendRstStream(id, error, bytes_written);
  OnClosedStream();
}

void QuicClientSession::OnClosedStream() {
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
  if (!callback_.is_null() &&
      (!require_confirmation_ || event == HANDSHAKE_CONFIRMED)) {
    // TODO(rtenneti): Currently for all CryptoHandshakeEvent events, callback_
    // could be called because there are no error events in CryptoHandshakeEvent
    // enum. If error events are added to CryptoHandshakeEvent, then the
    // following code needs to changed.
    base::ResetAndReturn(&callback_).Run(OK);
  }
  if (event == HANDSHAKE_CONFIRMED) {
    ObserverSet::iterator it = observers_.begin();
    while (it != observers_.end()) {
      Observer* observer = *it;
      ++it;
      observer->OnCryptoHandshakeConfirmed();
    }
  }
  QuicSession::OnCryptoHandshakeEvent(event);
}

void QuicClientSession::OnCryptoHandshakeMessageSent(
    const CryptoHandshakeMessage& message) {
  logger_.OnCryptoHandshakeMessageSent(message);
}

void QuicClientSession::OnCryptoHandshakeMessageReceived(
    const CryptoHandshakeMessage& message) {
  logger_.OnCryptoHandshakeMessageReceived(message);
}

void QuicClientSession::OnConnectionClosed(QuicErrorCode error,
                                           bool from_peer) {
  DCHECK(!connection()->connected());
  logger_.OnConnectionClosed(error, from_peer);
  if (from_peer) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.QuicSession.ConnectionCloseErrorCodeServer", error);
  } else {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.QuicSession.ConnectionCloseErrorCodeClient", error);
  }

  if (error == QUIC_CONNECTION_TIMED_OUT) {
    UMA_HISTOGRAM_COUNTS(
        "Net.QuicSession.ConnectionClose.NumOpenStreams.TimedOut",
        GetNumOpenStreams());
    if (!IsCryptoHandshakeConfirmed()) {
      // If there have been any streams created, they were 0-RTT speculative
      // requests that have not be serviced.
      UMA_HISTOGRAM_COUNTS(
          "Net.QuicSession.ConnectionClose.NumTotalStreams.HandshakeTimedOut",
          num_total_streams_);
    }
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.QuicVersion",
                              connection()->version());
  NotifyFactoryOfSessionGoingAway();
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(ERR_QUIC_PROTOCOL_ERROR);
  }
  socket_->Close();
  QuicSession::OnConnectionClosed(error, from_peer);
  DCHECK(streams()->empty());
  CloseAllStreams(ERR_UNEXPECTED);
  CloseAllObservers(ERR_UNEXPECTED);
  NotifyFactoryOfSessionClosedLater();
}

void QuicClientSession::OnSuccessfulVersionNegotiation(
    const QuicVersion& version) {
  logger_.OnSuccessfulVersionNegotiation(version);
  QuicSession::OnSuccessfulVersionNegotiation(version);
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
    num_packets_read_ = 0;
    return;
  }

  if (++num_packets_read_ > 32) {
    num_packets_read_ = 0;
    // Data was read, process it.
    // Schedule the work through the message loop to avoid recursive
    // callbacks.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&QuicClientSession::OnReadComplete,
                   weak_factory_.GetWeakPtr(), rv));
  } else {
    OnReadComplete(rv);
  }
}

void QuicClientSession::CloseSessionOnError(int error) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.CloseSessionOnError", -error);
  CloseSessionOnErrorInner(error, QUIC_INTERNAL_ERROR);
  NotifyFactoryOfSessionClosed();
}

void QuicClientSession::CloseSessionOnErrorInner(int net_error,
                                                 QuicErrorCode quic_error) {
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(net_error);
  }
  CloseAllStreams(net_error);
  CloseAllObservers(net_error);
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CLOSE_ON_ERROR,
      NetLog::IntegerCallback("net_error", net_error));

  connection()->CloseConnection(quic_error, false);
  DCHECK(!connection()->connected());
}

void QuicClientSession::CloseAllStreams(int net_error) {
  while (!streams()->empty()) {
    ReliableQuicStream* stream = streams()->begin()->second;
    QuicStreamId id = stream->id();
    static_cast<QuicReliableClientStream*>(stream)->OnError(net_error);
    CloseStream(id);
  }
}

void QuicClientSession::CloseAllObservers(int net_error) {
  while (!observers_.empty()) {
    Observer* observer = *observers_.begin();
    observers_.erase(observer);
    observer->OnSessionClosed(net_error);
  }
}

base::Value* QuicClientSession::GetInfoAsValue(
    const std::set<HostPortProxyPair>& aliases) const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  // TODO(rch): remove "host_port_pair" when Chrome 34 is stable.
  dict->SetString("host_port_pair", aliases.begin()->first.ToString());
  dict->SetString("version", QuicVersionToString(connection()->version()));
  dict->SetInteger("open_streams", GetNumOpenStreams());
  dict->SetInteger("total_streams", num_total_streams_);
  dict->SetString("peer_address", peer_address().ToString());
  dict->SetString("connection_id", base::Uint64ToString(connection_id()));
  dict->SetBoolean("connected", connection()->connected());

  base::ListValue* alias_list = new base::ListValue();
  for (std::set<HostPortProxyPair>::const_iterator it = aliases.begin();
       it != aliases.end(); it++) {
    alias_list->Append(new base::StringValue(it->first.ToString()));
  }
  dict->Set("aliases", alias_list);

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
    DVLOG(1) << "Closing session on read error: " << result;
    UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.ReadError", -result);
    NotifyFactoryOfSessionGoingAway();
    CloseSessionOnErrorInner(result, QUIC_PACKET_READ_ERROR);
    NotifyFactoryOfSessionClosedLater();
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
    NotifyFactoryOfSessionClosedLater();
    return;
  }
  StartReading();
}

void QuicClientSession::NotifyFactoryOfSessionGoingAway() {
  if (stream_factory_)
    stream_factory_->OnSessionGoingAway(this);
}

void QuicClientSession::NotifyFactoryOfSessionClosedLater() {
  DCHECK_EQ(0u, GetNumOpenStreams());
  DCHECK(!connection()->connected());
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&QuicClientSession::NotifyFactoryOfSessionClosed,
                 weak_factory_.GetWeakPtr()));
}

void QuicClientSession::NotifyFactoryOfSessionClosed() {
  DCHECK_EQ(0u, GetNumOpenStreams());
  // Will delete |this|.
  if (stream_factory_)
    stream_factory_->OnSessionClosed(this);
}

}  // namespace net
