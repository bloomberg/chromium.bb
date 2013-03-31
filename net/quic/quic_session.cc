// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session.h"

#include "base/stl_util.h"
#include "net/quic/quic_connection.h"

using base::StringPiece;
using base::hash_map;
using base::hash_set;
using std::vector;

namespace net {

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

  virtual bool OnPacket(const IPEndPoint& self_address,
                        const IPEndPoint& peer_address,
                        const QuicPacketHeader& header,
                        const vector<QuicStreamFrame>& frame) OVERRIDE {
    bool accepted = session_->OnPacket(self_address, peer_address, header,
                                       frame);
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

  virtual void OnAck(const SequenceNumberSet& acked_packets) OVERRIDE {
    session_->OnAck(acked_packets);
    session_->PostProcessAfterData();
  }

  virtual bool OnCanWrite() OVERRIDE {
    bool rc = session_->OnCanWrite();
    session_->PostProcessAfterData();
    return rc;
  }

  virtual void ConnectionClose(QuicErrorCode error, bool from_peer) OVERRIDE {
    session_->ConnectionClose(error, from_peer);
    // The session will go away, so don't bother with cleanup.
  }

 private:
  QuicSession* session_;
};

QuicSession::QuicSession(QuicConnection* connection, bool is_server)
    : connection_(connection),
      visitor_shim_(new VisitorShim(this)),
      max_open_streams_(kDefaultMaxStreamsPerConnection),
      next_stream_id_(is_server ? 2 : 3),
      is_server_(is_server),
      largest_peer_created_stream_id_(0),
      goaway_received_(false),
      goaway_sent_(false) {
  connection->set_visitor(visitor_shim_.get());
}

QuicSession::~QuicSession() {
  STLDeleteElements(&closed_streams_);
  STLDeleteValues(&stream_map_);
}

bool QuicSession::OnPacket(const IPEndPoint& self_address,
                           const IPEndPoint& peer_address,
                           const QuicPacketHeader& header,
                           const vector<QuicStreamFrame>& frames) {
  if (header.public_header.guid != connection()->guid()) {
    DLOG(INFO) << "Got packet header for invalid GUID: "
               << header.public_header.guid;
    return false;
  }
  for (size_t i = 0; i < frames.size(); ++i) {
    // TODO(rch) deal with the error case of stream id 0
    if (IsClosedStream(frames[i].stream_id)) continue;

    ReliableQuicStream* stream = GetStream(frames[i].stream_id);
    if (stream == NULL) return false;
    if (!stream->WillAcceptStreamFrame(frames[i])) return false;

    // TODO(alyssar) check against existing connection address: if changed, make
    // sure we update the connection.
  }

  for (size_t i = 0; i < frames.size(); ++i) {
    ReliableQuicStream* stream = GetStream(frames[i].stream_id);
    if (stream) {
      stream->OnStreamFrame(frames[i]);
    }
  }
  return true;
}

void QuicSession::OnRstStream(const QuicRstStreamFrame& frame) {
  ReliableQuicStream* stream = GetStream(frame.stream_id);
  if (!stream) {
    return;  // Errors are handled by GetStream.
  }
  stream->OnStreamReset(frame.error_code);
}

void QuicSession::OnGoAway(const QuicGoAwayFrame& frame) {
  DCHECK(frame.last_good_stream_id < next_stream_id_);
  goaway_received_ = true;
}

void QuicSession::ConnectionClose(QuicErrorCode error, bool from_peer) {
  while (stream_map_.size() != 0) {
    ReliableStreamMap::iterator it = stream_map_.begin();
    QuicStreamId id = it->first;
    it->second->ConnectionClose(error, from_peer);
    // The stream should call CloseStream as part of ConnectionClose.
    if (stream_map_.find(id) != stream_map_.end()) {
      LOG(DFATAL) << "Stream failed to close under ConnectionClose";
      CloseStream(id);
    }
  }
}

bool QuicSession::OnCanWrite() {
  // We latch this here rather than doing a traditional loop, because streams
  // may be modifying the list as we loop.
  int remaining_writes = write_blocked_streams_.NumObjects();

  while (!connection_->HasQueuedData() &&
         remaining_writes > 0) {
    DCHECK(!write_blocked_streams_.IsEmpty());
    ReliableQuicStream* stream =
        GetStream(write_blocked_streams_.GetNextBlockedObject());
    if (stream != NULL) {
      // If the stream can't write all bytes, it'll re-add itself to the blocked
      // list.
      stream->OnCanWrite();
    }
    --remaining_writes;
  }

  return write_blocked_streams_.IsEmpty();
}

QuicConsumedData QuicSession::WriteData(QuicStreamId id,
                                        StringPiece data,
                                        QuicStreamOffset offset,
                                        bool fin) {
  return connection_->SendStreamData(id, data, offset, fin);
}

void QuicSession::SendRstStream(QuicStreamId id,
                                QuicRstStreamErrorCode error) {
  connection_->SendRstStream(id, error);
  CloseStream(id);
}

void QuicSession::SendGoAway(QuicErrorCode error_code, const string& reason) {
  goaway_sent_ = true;
  connection_->SendGoAway(error_code, largest_peer_created_stream_id_, reason);
}

void QuicSession::CloseStream(QuicStreamId stream_id) {
  DLOG(INFO) << "Closing stream " << stream_id;

  ReliableStreamMap::iterator it = stream_map_.find(stream_id);
  if (it == stream_map_.end()) {
    DLOG(INFO) << "Stream is already closed: " << stream_id;
    return;
  }
  it->second->OnClose();
  closed_streams_.push_back(it->second);
  stream_map_.erase(it);
}

bool QuicSession::IsCryptoHandshakeComplete() {
  return GetCryptoStream()->handshake_complete();
}

void QuicSession::OnCryptoHandshakeComplete(QuicErrorCode error) {
  // TODO(rch): tear down the connection if error != QUIC_NO_ERROR.
}

void QuicSession::ActivateStream(ReliableQuicStream* stream) {
  DLOG(INFO) << "num_streams: " << stream_map_.size()
            << ". activating " << stream->id();
  DCHECK(stream_map_.count(stream->id()) == 0);
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

  ReliableStreamMap::iterator it = stream_map_.find(stream_id);
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

ReliableQuicStream* QuicSession::GetIncomingReliableStream(
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
    if (largest_peer_created_stream_id_ != 0) {
      for (QuicStreamId id = largest_peer_created_stream_id_ + 2;
           id < stream_id;
           id += 2) {
        implicitly_created_streams_.insert(id);
      }
    }
    largest_peer_created_stream_id_ = stream_id;
  }
  ReliableQuicStream* stream = CreateIncomingReliableStream(stream_id);
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
  if (stream_map_.count(id) != 0) {
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
  return stream_map_.size() + implicitly_created_streams_.size();
}

void QuicSession::MarkWriteBlocked(QuicStreamId id) {
  write_blocked_streams_.AddBlockedObject(id);
}

void QuicSession::PostProcessAfterData() {
  STLDeleteElements(&closed_streams_);
  closed_streams_.clear();
}

}  // namespace net
