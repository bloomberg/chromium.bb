// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_spdy_session.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "net/quic/core/quic_headers_stream.h"
#include "net/quic/platform/api/quic_bug_tracker.h"
#include "net/quic/platform/api/quic_flag_utils.h"
#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_logging.h"
#include "net/quic/platform/api/quic_ptr_util.h"
#include "net/quic/platform/api/quic_str_cat.h"
#include "net/quic/platform/api/quic_text_utils.h"
#include "net/spdy/core/http2_frame_decoder_adapter.h"

using std::string;

namespace net {

namespace {

class HeaderTableDebugVisitor
    : public HpackHeaderTable::DebugVisitorInterface {
 public:
  HeaderTableDebugVisitor(const QuicClock* clock,
                          std::unique_ptr<QuicHpackDebugVisitor> visitor)
      : clock_(clock), headers_stream_hpack_visitor_(std::move(visitor)) {}

  int64_t OnNewEntry(const HpackEntry& entry) override {
    QUIC_DVLOG(1) << entry.GetDebugString();
    return (clock_->ApproximateNow() - QuicTime::Zero()).ToMicroseconds();
  }

  void OnUseEntry(const HpackEntry& entry) override {
    const QuicTime::Delta elapsed(
        clock_->ApproximateNow() -
        QuicTime::Delta::FromMicroseconds(entry.time_added()) -
        QuicTime::Zero());
    QUIC_DVLOG(1) << entry.GetDebugString() << " " << elapsed.ToMilliseconds()
                  << " ms";
    headers_stream_hpack_visitor_->OnUseEntry(elapsed);
  }

 private:
  const QuicClock* clock_;
  std::unique_ptr<QuicHpackDebugVisitor> headers_stream_hpack_visitor_;

  DISALLOW_COPY_AND_ASSIGN(HeaderTableDebugVisitor);
};

}  // namespace

// A SpdyFramerVisitor that passes HEADERS frames to the QuicSpdyStream, and
// closes the connection if any unexpected frames are received.
class QuicSpdySession::SpdyFramerVisitor
    : public SpdyFramerVisitorInterface,
      public SpdyFramerDebugVisitorInterface {
 public:
  explicit SpdyFramerVisitor(QuicSpdySession* session) : session_(session) {}

  SpdyHeadersHandlerInterface* OnHeaderFrameStart(
      SpdyStreamId /* stream_id */) override {
    return &header_list_;
  }

  void OnHeaderFrameEnd(SpdyStreamId /* stream_id */) override {
    if (session_->IsConnected()) {
      session_->OnHeaderList(header_list_);
    }
    header_list_.Clear();
  }

  void OnStreamFrameData(SpdyStreamId stream_id,
                         const char* data,
                         size_t len) override {
    CloseConnection("SPDY DATA frame received.",
                    QUIC_INVALID_HEADERS_STREAM_DATA);
  }

  void OnStreamEnd(SpdyStreamId stream_id) override {
    // The framer invokes OnStreamEnd after processing a frame that had the fin
    // bit set.
  }

  void OnStreamPadding(SpdyStreamId stream_id, size_t len) override {
    CloseConnection("SPDY frame padding received.",
                    QUIC_INVALID_HEADERS_STREAM_DATA);
  }

  void OnError(Http2DecoderAdapter::SpdyFramerError error) override {
    QuicErrorCode code = QUIC_INVALID_HEADERS_STREAM_DATA;
    switch (error) {
      case Http2DecoderAdapter::SpdyFramerError::SPDY_DECOMPRESS_FAILURE:
        code = QUIC_HEADERS_STREAM_DATA_DECOMPRESS_FAILURE;
        break;
      default:
        break;
    }
    CloseConnection(
        QuicStrCat("SPDY framing error: ",
                   Http2DecoderAdapter::SpdyFramerErrorToString(error)),
        code);
  }

  void OnDataFrameHeader(SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override {
    CloseConnection("SPDY DATA frame received.",
                    QUIC_INVALID_HEADERS_STREAM_DATA);
  }

  void OnRstStream(SpdyStreamId stream_id, SpdyErrorCode error_code) override {
    CloseConnection("SPDY RST_STREAM frame received.",
                    QUIC_INVALID_HEADERS_STREAM_DATA);
  }

  void OnSetting(SpdySettingsIds id, uint32_t value) override {
    if (!GetQuicReloadableFlag(quic_respect_http2_settings_frame)) {
      CloseConnection("SPDY SETTINGS frame received.",
                      QUIC_INVALID_HEADERS_STREAM_DATA);
      return;
    }
    switch (id) {
      case SETTINGS_HEADER_TABLE_SIZE:
        session_->UpdateHeaderEncoderTableSize(value);
        break;
      case SETTINGS_ENABLE_PUSH:
        if (session_->perspective() == Perspective::IS_SERVER) {
          // See rfc7540, Section 6.5.2.
          if (value > 1) {
            CloseConnection(
                QuicStrCat("Invalid value for SETTINGS_ENABLE_PUSH: ", value),
                QUIC_INVALID_HEADERS_STREAM_DATA);
            return;
          }
          session_->UpdateEnableServerPush(value > 0);
          break;
        } else {
          CloseConnection(
              QuicStrCat("Unsupported field of HTTP/2 SETTINGS frame: ", id),
              QUIC_INVALID_HEADERS_STREAM_DATA);
        }
        break;
      // TODO(fayang): Need to support SETTINGS_MAX_HEADER_LIST_SIZE when
      // clients are actually sending it.
      case SETTINGS_MAX_HEADER_LIST_SIZE:
        if (GetQuicReloadableFlag(quic_send_max_header_list_size)) {
          break;
        }
      default:
        CloseConnection(
            QuicStrCat("Unsupported field of HTTP/2 SETTINGS frame: ", id),
            QUIC_INVALID_HEADERS_STREAM_DATA);
    }
  }

  void OnSettingsAck() override {
    if (!GetQuicReloadableFlag(quic_respect_http2_settings_frame)) {
      CloseConnection("SPDY SETTINGS frame received.",
                      QUIC_INVALID_HEADERS_STREAM_DATA);
    }
  }

  void OnSettingsEnd() override {
    if (!GetQuicReloadableFlag(quic_respect_http2_settings_frame)) {
      CloseConnection("SPDY SETTINGS frame received.",
                      QUIC_INVALID_HEADERS_STREAM_DATA);
    }
  }

  void OnPing(SpdyPingId unique_id, bool is_ack) override {
    CloseConnection("SPDY PING frame received.",
                    QUIC_INVALID_HEADERS_STREAM_DATA);
  }

  void OnGoAway(SpdyStreamId last_accepted_stream_id,
                SpdyErrorCode error_code) override {
    CloseConnection("SPDY GOAWAY frame received.",
                    QUIC_INVALID_HEADERS_STREAM_DATA);
  }

  void OnHeaders(SpdyStreamId stream_id,
                 bool has_priority,
                 int weight,
                 SpdyStreamId /*parent_stream_id*/,
                 bool /*exclusive*/,
                 bool fin,
                 bool end) override {
    if (!session_->IsConnected()) {
      return;
    }

    // TODO(mpw): avoid down-conversion and plumb SpdyStreamPrecedence through
    // QuicHeadersStream.
    SpdyPriority priority =
        has_priority ? Http2WeightToSpdy3Priority(weight) : 0;
    session_->OnHeaders(stream_id, has_priority, priority, fin);
  }

  void OnWindowUpdate(SpdyStreamId stream_id, int delta_window_size) override {
    CloseConnection("SPDY WINDOW_UPDATE frame received.",
                    QUIC_INVALID_HEADERS_STREAM_DATA);
  }

  void OnPushPromise(SpdyStreamId stream_id,
                     SpdyStreamId promised_stream_id,
                     bool end) override {
    if (!session_->supports_push_promise()) {
      CloseConnection("PUSH_PROMISE not supported.",
                      QUIC_INVALID_HEADERS_STREAM_DATA);
      return;
    }
    if (!session_->IsConnected()) {
      return;
    }
    session_->OnPushPromise(stream_id, promised_stream_id, end);
  }

  void OnContinuation(SpdyStreamId stream_id, bool end) override {}

  void OnPriority(SpdyStreamId stream_id,
                  SpdyStreamId parent_id,
                  int weight,
                  bool exclusive) override {
    CloseConnection("SPDY PRIORITY frame received.",
                    QUIC_INVALID_HEADERS_STREAM_DATA);
  }

  bool OnUnknownFrame(SpdyStreamId stream_id, uint8_t frame_type) override {
    CloseConnection("Unknown frame type received.",
                    QUIC_INVALID_HEADERS_STREAM_DATA);
    return false;
  }

  // SpdyFramerDebugVisitorInterface implementation
  void OnSendCompressedFrame(SpdyStreamId stream_id,
                             SpdyFrameType type,
                             size_t payload_len,
                             size_t frame_len) override {
    if (payload_len == 0) {
      QUIC_BUG << "Zero payload length.";
      return;
    }
    int compression_pct = 100 - (100 * frame_len) / payload_len;
    QUIC_DVLOG(1) << "Net.QuicHpackCompressionPercentage: " << compression_pct;
  }

  void OnReceiveCompressedFrame(SpdyStreamId stream_id,
                                SpdyFrameType type,
                                size_t frame_len) override {
    if (session_->IsConnected()) {
      session_->OnCompressedFrameSize(frame_len);
    }
  }

  void set_max_uncompressed_header_bytes(
      size_t set_max_uncompressed_header_bytes) {
    header_list_.set_max_header_list_size(set_max_uncompressed_header_bytes);
  }

 private:
  void CloseConnection(const string& details, QuicErrorCode code) {
    if (session_->IsConnected()) {
      session_->CloseConnectionWithDetails(code, details);
    }
  }

 private:
  QuicSpdySession* session_;
  QuicHeaderList header_list_;

  DISALLOW_COPY_AND_ASSIGN(SpdyFramerVisitor);
};

QuicHpackDebugVisitor::QuicHpackDebugVisitor() {}

QuicHpackDebugVisitor::~QuicHpackDebugVisitor() {}

QuicSpdySession::QuicSpdySession(QuicConnection* connection,
                                 QuicSession::Visitor* visitor,
                                 const QuicConfig& config)
    : QuicSession(connection, visitor, config),
      max_inbound_header_list_size_(kDefaultMaxUncompressedHeaderSize),
      server_push_enabled_(true),
      stream_id_(kInvalidStreamId),
      promised_stream_id_(kInvalidStreamId),
      fin_(false),
      frame_len_(0),
      uncompressed_frame_len_(0),
      supports_push_promise_(perspective() == Perspective::IS_CLIENT),
      cur_max_timestamp_(QuicTime::Zero()),
      prev_max_timestamp_(QuicTime::Zero()),
      use_hq_deframer_(GetQuicReloadableFlag(quic_enable_hq_deframer)),
      spdy_framer_(SpdyFramer::ENABLE_COMPRESSION),
      spdy_framer_visitor_(new SpdyFramerVisitor(this)) {
  if (use_hq_deframer_) {
    QUIC_FLAG_COUNT(quic_reloadable_flag_quic_enable_hq_deframer);
    hq_deframer_.set_visitor(spdy_framer_visitor_.get());
    hq_deframer_.set_debug_visitor(spdy_framer_visitor_.get());
  } else {
    h2_deframer_.set_visitor(spdy_framer_visitor_.get());
    h2_deframer_.set_debug_visitor(spdy_framer_visitor_.get());
  }
}

QuicSpdySession::~QuicSpdySession() {
  // Set the streams' session pointers in closed and dynamic stream lists
  // to null to avoid subsequent use of this session.
  for (auto& stream : *closed_streams()) {
    static_cast<QuicSpdyStream*>(stream.get())->ClearSession();
  }
  for (auto const& kv : zombie_streams()) {
    static_cast<QuicSpdyStream*>(kv.second.get())->ClearSession();
  }
  for (auto const& kv : dynamic_streams()) {
    static_cast<QuicSpdyStream*>(kv.second.get())->ClearSession();
  }
}

void QuicSpdySession::Initialize() {
  QuicSession::Initialize();

  if (perspective() == Perspective::IS_SERVER) {
    set_largest_peer_created_stream_id(kHeadersStreamId);
  } else {
    QuicStreamId headers_stream_id = GetNextOutgoingStreamId();
    DCHECK_EQ(headers_stream_id, kHeadersStreamId);
  }

  headers_stream_.reset(new QuicHeadersStream(this));
  DCHECK_EQ(kHeadersStreamId, headers_stream_->id());
  static_streams()[kHeadersStreamId] = headers_stream_.get();

  set_max_uncompressed_header_bytes(max_inbound_header_list_size_);

  // Limit HPACK buffering to 2x header list size limit.
  set_max_decode_buffer_size_bytes(2 * max_inbound_header_list_size_);
}

void QuicSpdySession::OnStreamHeadersPriority(QuicStreamId stream_id,
                                              SpdyPriority priority) {
  QuicSpdyStream* stream = GetSpdyDataStream(stream_id);
  if (!stream) {
    // It's quite possible to receive headers after a stream has been reset.
    return;
  }
  stream->OnStreamHeadersPriority(priority);
}

void QuicSpdySession::OnStreamHeaderList(QuicStreamId stream_id,
                                         bool fin,
                                         size_t frame_len,
                                         const QuicHeaderList& header_list) {
  QuicSpdyStream* stream = GetSpdyDataStream(stream_id);
  if (stream == nullptr) {
    // The stream no longer exists, but trailing headers may contain the final
    // byte offset necessary for flow control and open stream accounting.
    size_t final_byte_offset = 0;
    for (const auto& header : header_list) {
      const string& header_key = header.first;
      const string& header_value = header.second;
      if (header_key == kFinalOffsetHeaderKey) {
        if (!QuicTextUtils::StringToSizeT(header_value, &final_byte_offset)) {
          connection()->CloseConnection(
              QUIC_INVALID_HEADERS_STREAM_DATA,
              "Trailers are malformed (no final offset)",
              ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
          return;
        }
        DVLOG(1) << "Received final byte offset in trailers for stream "
                 << stream_id << ", which no longer exists.";
        OnFinalByteOffsetReceived(stream_id, final_byte_offset);
      }
    }

    // It's quite possible to receive headers after a stream has been reset.
    return;
  }
  stream->OnStreamHeaderList(fin, frame_len, header_list);
}

size_t QuicSpdySession::ProcessHeaderData(const struct iovec& iov,
                                          QuicTime timestamp) {
  DCHECK(timestamp.IsInitialized());
  UpdateCurMaxTimeStamp(timestamp);
  return use_hq_deframer_ ? hq_deframer_.ProcessInput(
                                static_cast<char*>(iov.iov_base), iov.iov_len)
                          : h2_deframer_.ProcessInput(
                                static_cast<char*>(iov.iov_base), iov.iov_len);
}

size_t QuicSpdySession::WriteHeaders(
    QuicStreamId id,
    SpdyHeaderBlock headers,
    bool fin,
    SpdyPriority priority,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  return WriteHeadersImpl(id, std::move(headers), fin, priority, 0, false,
                          std::move(ack_listener));
}

size_t QuicSpdySession::WriteHeaders(
    QuicStreamId id,
    SpdyHeaderBlock headers,
    bool fin,
    SpdyPriority priority,
    QuicStreamId parent_stream_id,
    bool exclusive,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  return WriteHeadersImpl(id, std::move(headers), fin, priority,
                          parent_stream_id, exclusive, std::move(ack_listener));
}

size_t QuicSpdySession::WriteHeadersImpl(
    QuicStreamId id,
    SpdyHeaderBlock headers,
    bool fin,
    SpdyPriority priority,
    QuicStreamId parent_stream_id,
    bool exclusive,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  SpdyHeadersIR headers_frame(id, std::move(headers));
  headers_frame.set_fin(fin);
  if (perspective() == Perspective::IS_CLIENT) {
    headers_frame.set_has_priority(true);
    headers_frame.set_weight(Spdy3PriorityToHttp2Weight(priority));
    headers_frame.set_parent_stream_id(parent_stream_id);
    headers_frame.set_exclusive(exclusive);
  }
  SpdySerializedFrame frame(spdy_framer_.SerializeFrame(headers_frame));
  headers_stream_->WriteOrBufferData(
      QuicStringPiece(frame.data(), frame.size()), false,
      std::move(ack_listener));
  return frame.size();
}

size_t QuicSpdySession::WritePriority(QuicStreamId id,
                                      QuicStreamId parent_stream_id,
                                      int weight,
                                      bool exclusive) {
  SpdyPriorityIR priority_frame(id, parent_stream_id, weight, exclusive);
  SpdySerializedFrame frame(spdy_framer_.SerializeFrame(priority_frame));
  headers_stream_->WriteOrBufferData(
      QuicStringPiece(frame.data(), frame.size()), false, nullptr);
  return frame.size();
}

size_t QuicSpdySession::WritePushPromise(QuicStreamId original_stream_id,
                                         QuicStreamId promised_stream_id,
                                         SpdyHeaderBlock headers) {
  if (perspective() == Perspective::IS_CLIENT) {
    QUIC_BUG << "Client shouldn't send PUSH_PROMISE";
    return 0;
  }

  SpdyPushPromiseIR push_promise(original_stream_id, promised_stream_id,
                                 std::move(headers));
  // PUSH_PROMISE must not be the last frame sent out, at least followed by
  // response headers.
  push_promise.set_fin(false);

  SpdySerializedFrame frame(spdy_framer_.SerializeFrame(push_promise));
  headers_stream_->WriteOrBufferData(
      QuicStringPiece(frame.data(), frame.size()), false, nullptr);
  return frame.size();
}

size_t QuicSpdySession::SendMaxHeaderListSize(size_t value) {
  SpdySettingsIR settings_frame;
  settings_frame.AddSetting(SETTINGS_MAX_HEADER_LIST_SIZE, value);

  SpdySerializedFrame frame(spdy_framer_.SerializeFrame(settings_frame));
  headers_stream_->WriteOrBufferData(
      QuicStringPiece(frame.data(), frame.size()), false, nullptr);
  return frame.size();
}

void QuicSpdySession::OnHeadersHeadOfLineBlocking(QuicTime::Delta delta) {
  // Implemented in Chromium for stats tracking.
}

void QuicSpdySession::RegisterStreamPriority(QuicStreamId id,
                                             SpdyPriority priority) {
  write_blocked_streams()->RegisterStream(id, priority);
}

void QuicSpdySession::UnregisterStreamPriority(QuicStreamId id) {
  write_blocked_streams()->UnregisterStream(id);
}

void QuicSpdySession::UpdateStreamPriority(QuicStreamId id,
                                           SpdyPriority new_priority) {
  write_blocked_streams()->UpdateStreamPriority(id, new_priority);
}

QuicSpdyStream* QuicSpdySession::GetSpdyDataStream(
    const QuicStreamId stream_id) {
  return static_cast<QuicSpdyStream*>(GetOrCreateDynamicStream(stream_id));
}

void QuicSpdySession::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  QuicSession::OnCryptoHandshakeEvent(event);
  if (GetQuicReloadableFlag(quic_send_max_header_list_size) &&
      event == HANDSHAKE_CONFIRMED && config()->SupportMaxHeaderListSize()) {
    SendMaxHeaderListSize(max_inbound_header_list_size_);
  }
}

void QuicSpdySession::OnPromiseHeaderList(QuicStreamId stream_id,
                                          QuicStreamId promised_stream_id,
                                          size_t frame_len,
                                          const QuicHeaderList& header_list) {
  string error = "OnPromiseHeaderList should be overridden in client code.";
  QUIC_BUG << error;
  connection()->CloseConnection(QUIC_INTERNAL_ERROR, error,
                                ConnectionCloseBehavior::SILENT_CLOSE);
}

void QuicSpdySession::OnConfigNegotiated() {
  QuicSession::OnConfigNegotiated();
  if (config()->HasClientSentConnectionOption(kDHDT, perspective())) {
    DisableHpackDynamicTable();
  }
}

bool QuicSpdySession::ShouldReleaseHeadersStreamSequencerBuffer() {
  return false;
}

void QuicSpdySession::OnHeaders(SpdyStreamId stream_id,
                                bool has_priority,
                                SpdyPriority priority,
                                bool fin) {
  if (has_priority) {
    if (perspective() == Perspective::IS_CLIENT) {
      CloseConnectionWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                 "Server must not send priorities.");
      return;
    }
    OnStreamHeadersPriority(stream_id, priority);
  } else {
    if (perspective() == Perspective::IS_SERVER) {
      CloseConnectionWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                 "Client must send priorities.");
      return;
    }
  }
  DCHECK_EQ(kInvalidStreamId, stream_id_);
  DCHECK_EQ(kInvalidStreamId, promised_stream_id_);
  stream_id_ = stream_id;
  fin_ = fin;
}

void QuicSpdySession::OnPushPromise(SpdyStreamId stream_id,
                                    SpdyStreamId promised_stream_id,
                                    bool end) {
  DCHECK_EQ(kInvalidStreamId, stream_id_);
  DCHECK_EQ(kInvalidStreamId, promised_stream_id_);
  stream_id_ = stream_id;
  promised_stream_id_ = promised_stream_id;
}

void QuicSpdySession::OnHeaderList(const QuicHeaderList& header_list) {
  QUIC_DVLOG(1) << "Received header list for stream " << stream_id_ << ": "
                << header_list.DebugString();
  if (prev_max_timestamp_ > cur_max_timestamp_) {
    // prev_max_timestamp_ > cur_max_timestamp_ implies that
    // headers from lower numbered streams actually came off the
    // wire after headers for the current stream, hence there was
    // HOL blocking.
    QuicTime::Delta delta = prev_max_timestamp_ - cur_max_timestamp_;
    QUIC_DLOG(INFO) << "stream " << stream_id_
                    << ": Net.QuicSession.HeadersHOLBlockedTime "
                    << delta.ToMilliseconds();
    OnHeadersHeadOfLineBlocking(delta);
  }
  prev_max_timestamp_ = std::max(prev_max_timestamp_, cur_max_timestamp_);
  cur_max_timestamp_ = QuicTime::Zero();
  if (promised_stream_id_ == kInvalidStreamId) {
    OnStreamHeaderList(stream_id_, fin_, frame_len_, header_list);
  } else {
    OnPromiseHeaderList(stream_id_, promised_stream_id_, frame_len_,
                        header_list);
  }
  // Reset state for the next frame.
  promised_stream_id_ = kInvalidStreamId;
  stream_id_ = kInvalidStreamId;
  fin_ = false;
  frame_len_ = 0;
  uncompressed_frame_len_ = 0;
}

void QuicSpdySession::OnCompressedFrameSize(size_t frame_len) {
  frame_len_ += frame_len;
}

void QuicSpdySession::DisableHpackDynamicTable() {
  spdy_framer_.UpdateHeaderEncoderTableSize(0);
}

void QuicSpdySession::SetHpackEncoderDebugVisitor(
    std::unique_ptr<QuicHpackDebugVisitor> visitor) {
  spdy_framer_.SetEncoderHeaderTableDebugVisitor(
      std::unique_ptr<HeaderTableDebugVisitor>(new HeaderTableDebugVisitor(
          connection()->helper()->GetClock(), std::move(visitor))));
}

void QuicSpdySession::SetHpackDecoderDebugVisitor(
    std::unique_ptr<QuicHpackDebugVisitor> visitor) {
  if (use_hq_deframer_) {
    hq_deframer_.SetDecoderHeaderTableDebugVisitor(
        std::unique_ptr<HeaderTableDebugVisitor>(new HeaderTableDebugVisitor(
            connection()->helper()->GetClock(), std::move(visitor))));
  } else {
    h2_deframer_.SetDecoderHeaderTableDebugVisitor(
        std::unique_ptr<HeaderTableDebugVisitor>(new HeaderTableDebugVisitor(
            connection()->helper()->GetClock(), std::move(visitor))));
  }
}

void QuicSpdySession::UpdateHeaderEncoderTableSize(uint32_t value) {
  spdy_framer_.UpdateHeaderEncoderTableSize(value);
}

void QuicSpdySession::UpdateEnableServerPush(bool value) {
  set_server_push_enabled(value);
}

void QuicSpdySession::set_max_uncompressed_header_bytes(
    size_t set_max_uncompressed_header_bytes) {
  spdy_framer_visitor_->set_max_uncompressed_header_bytes(
      set_max_uncompressed_header_bytes);
}

void QuicSpdySession::CloseConnectionWithDetails(QuicErrorCode error,
                                                 const string& details) {
  connection()->CloseConnection(
      error, details, ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

}  // namespace net
