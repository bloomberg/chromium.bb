// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session.h"

#include "base/stl_util.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/quic_connection.h"
#include "net/ssl/ssl_info.h"

using base::StringPiece;
using base::hash_map;
using base::hash_set;
using std::make_pair;
using std::vector;

namespace net {

const size_t kMaxPrematurelyClosedStreamsTracked = 20;
const size_t kMaxZombieStreams = 20;

#define ENDPOINT (is_server() ? "Server: " : " Client: ")

// We want to make sure we delete any closed streams in a safe manner.
// To avoid deleting a stream in mid-operation, we have a simple shim between
// us and the stream, so we can delete any streams when we return from
// processing.
//
// We could just override the base methods, but this makes it easier to make
// sure we don't miss any.
class VisitorShim : public QuicConnectionVisitorInterface {
 public:
  explicit VisitorShim(QuicSession* session) : session_(session) {}

  virtual bool OnStreamFrames(const vector<QuicStreamFrame>& frames) OVERRIDE {
    bool accepted = session_->OnStreamFrames(frames);
    session_->PostProcessAfterData();
    return accepted;
  }
  virtual void OnRstStream(const QuicRstStreamFrame& frame) OVERRIDE {
    session_->OnRstStream(frame);
    session_->PostProcessAfterData();
  }

  virtual void OnGoAway(const QuicGoAwayFrame& frame) OVERRIDE {
    session_->OnGoAway(frame);
    session_->PostProcessAfterData();
  }

  virtual bool OnCanWrite() OVERRIDE {
    bool rc = session_->OnCanWrite();
    session_->PostProcessAfterData();
    return rc;
  }

  virtual void OnSuccessfulVersionNegotiation(
      const QuicVersion& version) OVERRIDE {
    session_->OnSuccessfulVersionNegotiation(version);
  }

  virtual void OnConfigNegotiated() OVERRIDE {
    session_->OnConfigNegotiated();
  }

  virtual void OnConnectionClosed(QuicErrorCode error,
                                  bool from_peer) OVERRIDE {
    session_->OnConnectionClosed(error, from_peer);
    // The session will go away, so don't bother with cleanup.
  }

  virtual bool HasPendingHandshake() const OVERRIDE {
    return session_->HasPendingHandshake();
  }

 private:
  QuicSession* session_;
};

QuicSession::QuicSession(QuicConnection* connection,
                         const QuicConfig& config)
    : connection_(connection),
      visitor_shim_(new VisitorShim(this)),
      config_(config),
      max_open_streams_(config_.max_streams_per_connection()),
      next_stream_id_(is_server() ? 2 : 3),
      largest_peer_created_stream_id_(0),
      error_(QUIC_NO_ERROR),
      goaway_received_(false),
      goaway_sent_(false),
      has_pending_handshake_(false) {

  connection_->set_visitor(visitor_shim_.get());
  connection_->SetFromConfig(config_);
  if (connection_->connected()) {
    connection_->SetOverallConnectionTimeout(
        config_.max_time_before_crypto_handshake());
  }
}

QuicSession::~QuicSession() {
  STLDeleteElements(&closed_streams_);
  STLDeleteValues(&stream_map_);
}

bool QuicSession::OnStreamFrames(const vector<QuicStreamFrame>& frames) {
  for (size_t i = 0; i < frames.size(); ++i) {
    // TODO(rch) deal with the error case of stream id 0
    if (IsClosedStream(frames[i].stream_id)) {
      // If we get additional frames for a stream where we didn't process
      // headers, it's highly likely our compression context will end up
      // permanently out of sync with the peer's, so we give up and close the
      // connection.
      if (ContainsKey(prematurely_closed_streams_, frames[i].stream_id)) {
        connection()->SendConnectionClose(
            QUIC_STREAM_RST_BEFORE_HEADERS_DECOMPRESSED);
        return false;
      }
      continue;
    }

    ReliableQuicStream* stream = GetStream(frames[i].stream_id);
    if (stream == NULL) return false;
    if (!stream->WillAcceptStreamFrame(frames[i])) return false;

    // TODO(alyssar) check against existing connection address: if changed, make
    // sure we update the connection.
  }

  for (size_t i = 0; i < frames.size(); ++i) {
    QuicStreamId stream_id = frames[i].stream_id;
    ReliableQuicStream* stream = GetStream(stream_id);
    if (!stream) {
      continue;
    }
    stream->OnStreamFrame(frames[i]);

    // If the stream is a data stream had been prematurely closed, and the
    // headers are now decompressed, then we are finally finished
    // with this stream.
    if (ContainsKey(zombie_streams_, stream_id) &&
        static_cast<QuicDataStream*>(stream)->headers_decompressed()) {
      CloseZombieStream(stream_id);
    }
  }

  while (!decompression_blocked_streams_.empty()) {
    QuicHeaderId header_id = decompression_blocked_streams_.begin()->first;
    if (header_id != decompressor_.current_header_id()) {
      break;
    }
    QuicStreamId stream_id = decompression_blocked_streams_.begin()->second;
    decompression_blocked_streams_.erase(header_id);
    QuicDataStream* stream = GetDataStream(stream_id);
    if (!stream) {
      connection()->SendConnectionClose(
          QUIC_STREAM_RST_BEFORE_HEADERS_DECOMPRESSED);
      return false;
    }
    stream->OnDecompressorAvailable();
  }
  return true;
}

void QuicSession::OnRstStream(const QuicRstStreamFrame& frame) {
  if (frame.stream_id == kCryptoStreamId) {
    connection()->SendConnectionCloseWithDetails(
        QUIC_INVALID_STREAM_ID,
        "Attempt to reset the crypto stream");
    return;
  }
  QuicDataStream* stream = GetDataStream(frame.stream_id);
  if (!stream) {
    return;  // Errors are handled by GetStream.
  }
  if (ContainsKey(zombie_streams_, stream->id())) {
    // If this was a zombie stream then we close it out now.
    CloseZombieStream(stream->id());
    // However, since the headers still have not been decompressed, we want to
    // mark it a prematurely closed so that if we ever receive frames
    // for this stream we can close the connection.
    DCHECK(!stream->headers_decompressed());
    AddPrematurelyClosedStream(frame.stream_id);
    return;
  }
  if (stream->stream_bytes_read() > 0 && !stream->headers_decompressed()) {
    connection()->SendConnectionClose(
        QUIC_STREAM_RST_BEFORE_HEADERS_DECOMPRESSED);
  }
  stream->OnStreamReset(frame.error_code);
}

void QuicSession::OnGoAway(const QuicGoAwayFrame& frame) {
  DCHECK(frame.last_good_stream_id < next_stream_id_);
  goaway_received_ = true;
}

void QuicSession::OnConnectionClosed(QuicErrorCode error, bool from_peer) {
  DCHECK(!connection_->connected());
  if (error_ == QUIC_NO_ERROR) {
    error_ = error;
  }

  while (!stream_map_.empty()) {
    DataStreamMap::iterator it = stream_map_.begin();
    QuicStreamId id = it->first;
    it->second->OnConnectionClosed(error, from_peer);
    // The stream should call CloseStream as part of OnConnectionClosed.
    if (stream_map_.find(id) != stream_map_.end()) {
      LOG(DFATAL) << ENDPOINT
                  << "Stream failed to close under OnConnectionClosed";
      CloseStream(id);
    }
  }
}

bool QuicSession::OnCanWrite() {
  // We latch this here rather than doing a traditional loop, because streams
  // may be modifying the list as we loop.
  int remaining_writes = write_blocked_streams_.NumBlockedStreams();

  while (!connection_->HasQueuedData() &&
         remaining_writes > 0) {
    DCHECK(write_blocked_streams_.HasWriteBlockedStreams());
    if (!write_blocked_streams_.HasWriteBlockedStreams()) {
      LOG(DFATAL) << "WriteBlockedStream is missing";
      connection_->CloseConnection(QUIC_INTERNAL_ERROR, false);
      return true;  // We have no write blocked streams.
    }
    int index = write_blocked_streams_.GetHighestPriorityWriteBlockedList();
    QuicStreamId stream_id = write_blocked_streams_.PopFront(index);
    if (stream_id == kCryptoStreamId) {
      has_pending_handshake_ = false;  // We just popped it.
    }
    ReliableQuicStream* stream = GetStream(stream_id);
    if (stream != NULL) {
      // If the stream can't write all bytes, it'll re-add itself to the blocked
      // list.
      stream->OnCanWrite();
    }
    --remaining_writes;
  }

  return !write_blocked_streams_.HasWriteBlockedStreams();
}

bool QuicSession::HasPendingHandshake() const {
  return has_pending_handshake_;
}

QuicConsumedData QuicSession::WritevData(
    QuicStreamId id,
    const struct iovec* iov,
    int iov_count,
    QuicStreamOffset offset,
    bool fin,
    QuicAckNotifier::DelegateInterface* ack_notifier_delegate) {
  IOVector data;
  data.AppendIovec(iov, iov_count);
  return connection_->SendStreamData(id, data, offset, fin,
                                     ack_notifier_delegate);
}

void QuicSession::SendRstStream(QuicStreamId id,
                                QuicRstStreamErrorCode error) {
  connection_->SendRstStream(id, error);
  CloseStreamInner(id, true);
}

void QuicSession::SendGoAway(QuicErrorCode error_code, const string& reason) {
  goaway_sent_ = true;
  connection_->SendGoAway(error_code, largest_peer_created_stream_id_, reason);
}

void QuicSession::CloseStream(QuicStreamId stream_id) {
  CloseStreamInner(stream_id, false);
}

void QuicSession::CloseStreamInner(QuicStreamId stream_id,
                                   bool locally_reset) {
  DVLOG(1) << ENDPOINT << "Closing stream " << stream_id;

  DataStreamMap::iterator it = stream_map_.find(stream_id);
  if (it == stream_map_.end()) {
    DVLOG(1) << ENDPOINT << "Stream is already closed: " << stream_id;
    return;
  }
  QuicDataStream* stream = it->second;
  if (connection_->connected() && !stream->headers_decompressed()) {
    // If the stream is being closed locally (for example a client cancelling
    // a request before receiving the response) then we need to make sure that
    // we keep the stream alive long enough to process any response or
    // RST_STREAM frames.
    if (locally_reset && !is_server()) {
      AddZombieStream(stream_id);
      return;
    }

    // This stream has been closed before the headers were decompressed.
    // This might cause problems with head of line blocking of headers.
    // If the peer sent headers which were lost but we now close the stream
    // we will never be able to decompress headers for other streams.
    // To deal with this, we keep track of streams which have been closed
    // prematurely.  If we ever receive data frames for this steam, then we
    // know there actually has been a problem and we close the connection.
    AddPrematurelyClosedStream(stream->id());
  }
  closed_streams_.push_back(it->second);
  if (ContainsKey(zombie_streams_, stream->id())) {
    zombie_streams_.erase(stream->id());
  }
  stream_map_.erase(it);
  stream->OnClose();
}

void QuicSession::AddZombieStream(QuicStreamId stream_id) {
  if (zombie_streams_.size() == kMaxZombieStreams) {
    QuicStreamId oldest_zombie_stream_id = zombie_streams_.begin()->first;
    CloseZombieStream(oldest_zombie_stream_id);
    // However, since the headers still have not been decompressed, we want to
    // mark it a prematurely closed so that if we ever receive frames
    // for this stream we can close the connection.
    AddPrematurelyClosedStream(oldest_zombie_stream_id);
  }
  zombie_streams_.insert(make_pair(stream_id, true));
}

void QuicSession::CloseZombieStream(QuicStreamId stream_id) {
  DCHECK(ContainsKey(zombie_streams_, stream_id));
  zombie_streams_.erase(stream_id);
  QuicDataStream* stream = GetDataStream(stream_id);
  if (!stream) {
    return;
  }
  stream_map_.erase(stream_id);
  stream->OnClose();
  closed_streams_.push_back(stream);
}

void QuicSession::AddPrematurelyClosedStream(QuicStreamId stream_id) {
  if (prematurely_closed_streams_.size() ==
      kMaxPrematurelyClosedStreamsTracked) {
    prematurely_closed_streams_.erase(prematurely_closed_streams_.begin());
  }
  prematurely_closed_streams_.insert(make_pair(stream_id, true));
}

bool QuicSession::IsEncryptionEstablished() {
  return GetCryptoStream()->encryption_established();
}

bool QuicSession::IsCryptoHandshakeConfirmed() {
  return GetCryptoStream()->handshake_confirmed();
}

void QuicSession::OnConfigNegotiated() {
  connection_->SetFromConfig(config_);
}

void QuicSession::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  switch (event) {
    // TODO(satyamshekhar): Move the logic of setting the encrypter/decrypter
    // to QuicSession since it is the glue.
    case ENCRYPTION_FIRST_ESTABLISHED:
      break;

    case ENCRYPTION_REESTABLISHED:
      // Retransmit originally packets that were sent, since they can't be
      // decrypted by the peer.
      connection_->RetransmitUnackedPackets(INITIAL_ENCRYPTION_ONLY);
      break;

    case HANDSHAKE_CONFIRMED:
      LOG_IF(DFATAL, !config_.negotiated()) << ENDPOINT
          << "Handshake confirmed without parameter negotiation.";
      connection_->SetOverallConnectionTimeout(QuicTime::Delta::Infinite());
      max_open_streams_ = config_.max_streams_per_connection();
      break;

    default:
      LOG(ERROR) << ENDPOINT << "Got unknown handshake event: " << event;
  }
}

void QuicSession::OnCryptoHandshakeMessageSent(
    const CryptoHandshakeMessage& message) {
}

void QuicSession::OnCryptoHandshakeMessageReceived(
    const CryptoHandshakeMessage& message) {
}

QuicConfig* QuicSession::config() {
  return &config_;
}

void QuicSession::ActivateStream(QuicDataStream* stream) {
  DVLOG(1) << ENDPOINT << "num_streams: " << stream_map_.size()
             << ". activating " << stream->id();
  DCHECK_EQ(stream_map_.count(stream->id()), 0u);
  stream_map_[stream->id()] = stream;
}

QuicStreamId QuicSession::GetNextStreamId() {
  QuicStreamId id = next_stream_id_;
  next_stream_id_ += 2;
  return id;
}

ReliableQuicStream* QuicSession::GetStream(const QuicStreamId stream_id) {
  if (stream_id == kCryptoStreamId) {
    return GetCryptoStream();
  }
  return GetDataStream(stream_id);
}

QuicDataStream* QuicSession::GetDataStream(const QuicStreamId stream_id) {
  if (stream_id == kCryptoStreamId) {
    DLOG(FATAL) << "Attempt to call GetDataStream with the crypto stream id";
    return NULL;
  }

  DataStreamMap::iterator it = stream_map_.find(stream_id);
  if (it != stream_map_.end()) {
    return it->second;
  }

  if (IsClosedStream(stream_id)) {
    return NULL;
  }

  if (stream_id % 2 == next_stream_id_ % 2) {
    // We've received a frame for a locally-created stream that is not
    // currently active.  This is an error.
    connection()->SendConnectionClose(QUIC_PACKET_FOR_NONEXISTENT_STREAM);
    return NULL;
  }

  return GetIncomingReliableStream(stream_id);
}

QuicDataStream* QuicSession::GetIncomingReliableStream(
    QuicStreamId stream_id) {
  if (IsClosedStream(stream_id)) {
    return NULL;
  }

  if (goaway_sent_) {
    // We've already sent a GoAway
    SendRstStream(stream_id, QUIC_STREAM_PEER_GOING_AWAY);
    return NULL;
  }

  implicitly_created_streams_.erase(stream_id);
  if (stream_id > largest_peer_created_stream_id_) {
    // TODO(rch) add unit test for this
    if (stream_id - largest_peer_created_stream_id_ > kMaxStreamIdDelta) {
      connection()->SendConnectionClose(QUIC_INVALID_STREAM_ID);
      return NULL;
    }
    if (largest_peer_created_stream_id_ == 0) {
      largest_peer_created_stream_id_= 1;
    }
    for (QuicStreamId id = largest_peer_created_stream_id_ + 2;
         id < stream_id;
         id += 2) {
      implicitly_created_streams_.insert(id);
    }
    largest_peer_created_stream_id_ = stream_id;
  }
  QuicDataStream* stream = CreateIncomingDataStream(stream_id);
  if (stream == NULL) {
    return NULL;
  }
  ActivateStream(stream);
  return stream;
}

bool QuicSession::IsClosedStream(QuicStreamId id) {
  DCHECK_NE(0u, id);
  if (id == kCryptoStreamId) {
    return false;
  }
  if (ContainsKey(zombie_streams_, id)) {
    return true;
  }
  if (ContainsKey(stream_map_, id)) {
    // Stream is active
    return false;
  }
  if (id % 2 == next_stream_id_ % 2) {
    // Locally created streams are strictly in-order.  If the id is in the
    // range of created streams and it's not active, it must have been closed.
    return id < next_stream_id_;
  }
  // For peer created streams, we also need to consider implicitly created
  // streams.
  return id <= largest_peer_created_stream_id_ &&
      implicitly_created_streams_.count(id) == 0;
}

size_t QuicSession::GetNumOpenStreams() const {
  return stream_map_.size() + implicitly_created_streams_.size() -
      zombie_streams_.size();
}

void QuicSession::MarkWriteBlocked(QuicStreamId id, QuicPriority priority) {
  if (id == kCryptoStreamId) {
    DCHECK(!has_pending_handshake_);
    has_pending_handshake_ = true;
    // TODO(jar): Be sure to use the highest priority for the crypto stream,
    // perhaps by adding a "special" priority for it that is higher than
    // kHighestPriority.
    priority = kHighestPriority;
  }
  write_blocked_streams_.PushBack(id, priority);
}

bool QuicSession::HasQueuedData() const {
  return write_blocked_streams_.NumBlockedStreams() ||
      connection_->HasQueuedData();
}

void QuicSession::MarkDecompressionBlocked(QuicHeaderId header_id,
                                           QuicStreamId stream_id) {
  decompression_blocked_streams_[header_id] = stream_id;
}

bool QuicSession::GetSSLInfo(SSLInfo* ssl_info) {
  NOTIMPLEMENTED();
  return false;
}

void QuicSession::PostProcessAfterData() {
  STLDeleteElements(&closed_streams_);
  closed_streams_.clear();
}

}  // namespace net
