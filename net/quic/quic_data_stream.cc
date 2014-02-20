// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_data_stream.h"

#include "base/logging.h"
#include "net/quic/quic_session.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/quic/quic_utils.h"
#include "net/quic/quic_write_blocked_list.h"

using base::StringPiece;
using std::min;

namespace net {

#define ENDPOINT (session()->is_server() ? "Server: " : " Client: ")

namespace {

// This is somewhat arbitrary.  It's possible, but unlikely, we will either fail
// to set a priority client-side, or cancel a stream before stripping the
// priority from the wire server-side.  In either case, start out with a
// priority in the middle.
QuicPriority kDefaultPriority = 3;

// Appends bytes from data into partial_data_buffer.  Once partial_data_buffer
// reaches 4 bytes, copies the data into 'result' and clears
// partial_data_buffer.
// Returns the number of bytes consumed.
uint32 StripUint32(const char* data, uint32 data_len,
                   string* partial_data_buffer,
                   uint32* result) {
  DCHECK_GT(4u, partial_data_buffer->length());
  size_t missing_size = 4 - partial_data_buffer->length();
  if (data_len < missing_size) {
    StringPiece(data, data_len).AppendToString(partial_data_buffer);
    return data_len;
  }
  StringPiece(data, missing_size).AppendToString(partial_data_buffer);
  DCHECK_EQ(4u, partial_data_buffer->length());
  memcpy(result, partial_data_buffer->data(), 4);
  partial_data_buffer->clear();
  return missing_size;
}

}  // namespace

QuicDataStream::QuicDataStream(QuicStreamId id,
                               QuicSession* session)
    : ReliableQuicStream(id, session),
      visitor_(NULL),
      headers_decompressed_(false),
      priority_(kDefaultPriority),
      headers_id_(0),
      decompression_failed_(false),
      priority_parsed_(false) {
  DCHECK_NE(kCryptoStreamId, id);
  if (version() > QUIC_VERSION_12) {
    // Don't receive any callbacks from the sequencer until headers
    // are complete.
    sequencer()->SetBlockedUntilFlush();
  }
}

QuicDataStream::~QuicDataStream() {
}

size_t QuicDataStream::WriteHeaders(const SpdyHeaderBlock& header_block,
                                    bool fin) {
  size_t bytes_written = session()->WriteHeaders(id(), header_block, fin);
  if (fin) {
    // TODO(rch): Add test to ensure fin_sent_ is set whenever a fin is sent.
    set_fin_sent(true);
    CloseWriteSide();
  }
  return bytes_written;
}

size_t QuicDataStream::Readv(const struct iovec* iov, size_t iov_len) {
  if (FinishedReadingHeaders()) {
    // If the headers have been read, simply delegate to the sequencer's
    // Readv method.
    return sequencer()->Readv(iov, iov_len);
  }
  // Otherwise, copy decompressed header data into |iov|.
  size_t bytes_consumed = 0;
  size_t iov_index = 0;
  while (iov_index < iov_len &&
         decompressed_headers_.length() > bytes_consumed) {
    size_t bytes_to_read = min(iov[iov_index].iov_len,
                               decompressed_headers_.length() - bytes_consumed);
    char* iov_ptr = static_cast<char*>(iov[iov_index].iov_base);
    memcpy(iov_ptr,
           decompressed_headers_.data() + bytes_consumed, bytes_to_read);
    bytes_consumed += bytes_to_read;
    ++iov_index;
  }
  decompressed_headers_.erase(0, bytes_consumed);
  if (FinishedReadingHeaders()) {
    sequencer()->FlushBufferedFrames();
  }
  return bytes_consumed;
}

int QuicDataStream::GetReadableRegions(iovec* iov, size_t iov_len) {
  if (FinishedReadingHeaders()) {
    return sequencer()->GetReadableRegions(iov, iov_len);
  }
  if (iov_len == 0) {
    return 0;
  }
  iov[0].iov_base = static_cast<void*>(
      const_cast<char*>(decompressed_headers_.data()));
  iov[0].iov_len = decompressed_headers_.length();
  return 1;
}

bool QuicDataStream::IsDoneReading() const {
  if (!headers_decompressed_ || !decompressed_headers_.empty()) {
    return false;
  }
  return sequencer()->IsClosed();
}

bool QuicDataStream::HasBytesToRead() const {
  return !decompressed_headers_.empty() || sequencer()->HasBytesToRead();
}

void QuicDataStream::set_priority(QuicPriority priority) {
  DCHECK_EQ(0u, stream_bytes_written());
  priority_ = priority;
}

QuicPriority QuicDataStream::EffectivePriority() const {
  return priority();
}

uint32 QuicDataStream::ProcessRawData(const char* data, uint32 data_len) {
  if (version() <= QUIC_VERSION_12) {
    return ProcessRawData12(data, data_len);
  }

  if (!FinishedReadingHeaders()) {
    LOG(DFATAL) << "ProcessRawData called before headers have been finished";
    return 0;
  }
  return ProcessData(data, data_len);
}

uint32 QuicDataStream::ProcessRawData12(const char* data, uint32 data_len) {
  DCHECK_NE(0u, data_len);

  uint32 total_bytes_consumed = 0;
  if (headers_id_ == 0u) {
    total_bytes_consumed += StripPriorityAndHeaderId(data, data_len);
    data += total_bytes_consumed;
    data_len -= total_bytes_consumed;
    if (data_len == 0 || total_bytes_consumed == 0) {
      return total_bytes_consumed;
    }
  }
  DCHECK_NE(0u, headers_id_);

  // Once the headers are finished, we simply pass the data through.
  if (headers_decompressed_) {
    // Some buffered header data remains.
    if (!decompressed_headers_.empty()) {
      ProcessHeaderData();
    }
    if (decompressed_headers_.empty()) {
      DVLOG(1) << "Delegating procesing to ProcessData";
      total_bytes_consumed += ProcessData(data, data_len);
    }
    return total_bytes_consumed;
  }

  QuicHeaderId current_header_id =
      session()->decompressor()->current_header_id();
  // Ensure that this header id looks sane.
  if (headers_id_ < current_header_id ||
      headers_id_ > kMaxHeaderIdDelta + current_header_id) {
    DVLOG(1) << ENDPOINT
             << "Invalid headers for stream: " << id()
             << " header_id: " << headers_id_
             << " current_header_id: " << current_header_id;
    session()->connection()->SendConnectionClose(QUIC_INVALID_HEADER_ID);
    return total_bytes_consumed;
  }

  // If we are head-of-line blocked on decompression, then back up.
  if (current_header_id != headers_id_) {
    session()->MarkDecompressionBlocked(headers_id_, id());
    DVLOG(1) << ENDPOINT
             << "Unable to decompress header data for stream: " << id()
             << " header_id: " << headers_id_;
    return total_bytes_consumed;
  }

  // Decompressed data will be delivered to decompressed_headers_.
  size_t bytes_consumed = session()->decompressor()->DecompressData(
      StringPiece(data, data_len), this);
  DCHECK_NE(0u, bytes_consumed);
  if (bytes_consumed > data_len) {
    DCHECK(false) << "DecompressData returned illegal value";
    OnDecompressionError();
    return total_bytes_consumed;
  }
  total_bytes_consumed += bytes_consumed;
  data += bytes_consumed;
  data_len -= bytes_consumed;

  if (decompression_failed_) {
    // The session will have been closed in OnDecompressionError.
    return total_bytes_consumed;
  }

  // Headers are complete if the decompressor has moved on to the
  // next stream.
  headers_decompressed_ =
      session()->decompressor()->current_header_id() != headers_id_;
  if (!headers_decompressed_) {
    DCHECK_EQ(0u, data_len);
  }

  ProcessHeaderData();

  if (!headers_decompressed_ || !decompressed_headers_.empty()) {
    return total_bytes_consumed;
  }

  // We have processed all of the decompressed data but we might
  // have some more raw data to process.
  if (data_len > 0) {
    total_bytes_consumed += ProcessData(data, data_len);
  }

  // The sequencer will push any additional buffered frames if this data
  // has been completely consumed.
  return total_bytes_consumed;
}

const IPEndPoint& QuicDataStream::GetPeerAddress() {
  return session()->peer_address();
}

QuicSpdyCompressor* QuicDataStream::compressor() {
  return session()->compressor();
}

bool QuicDataStream::GetSSLInfo(SSLInfo* ssl_info) {
  return session()->GetSSLInfo(ssl_info);
}

uint32 QuicDataStream::ProcessHeaderData() {
  if (decompressed_headers_.empty()) {
    return 0;
  }

  size_t bytes_processed = ProcessData(decompressed_headers_.data(),
                                       decompressed_headers_.length());
  if (bytes_processed == decompressed_headers_.length()) {
    decompressed_headers_.clear();
  } else {
    decompressed_headers_ = decompressed_headers_.erase(0, bytes_processed);
  }
  return bytes_processed;
}

void QuicDataStream::OnDecompressorAvailable() {
  DCHECK_LE(QUIC_VERSION_12, version());
  DCHECK_EQ(headers_id_,
            session()->decompressor()->current_header_id());
  DCHECK(!headers_decompressed_);
  DCHECK(!decompression_failed_);
  DCHECK_EQ(0u, decompressed_headers_.length());

  while (!headers_decompressed_) {
    struct iovec iovec;
    if (sequencer()->GetReadableRegions(&iovec, 1) == 0) {
      return;
    }

    size_t bytes_consumed = session()->decompressor()->DecompressData(
        StringPiece(static_cast<char*>(iovec.iov_base),
                    iovec.iov_len),
        this);
    DCHECK_LE(bytes_consumed, iovec.iov_len);
    if (decompression_failed_) {
      return;
    }
    sequencer()->MarkConsumed(bytes_consumed);

    headers_decompressed_ =
        session()->decompressor()->current_header_id() != headers_id_;
  }

  // Either the headers are complete, or the all data as been consumed.
  ProcessHeaderData();  // Unprocessed headers remain in decompressed_headers_.
  if (IsDoneReading()) {
    OnFinRead();
  } else if (FinishedReadingHeaders()) {
    sequencer()->FlushBufferedFrames();
  }
}

bool QuicDataStream::OnDecompressedData(StringPiece data) {
  DCHECK_GE(QUIC_VERSION_12, version());
  data.AppendToString(&decompressed_headers_);
  return true;
}

void QuicDataStream::OnDecompressionError() {
  DCHECK_LE(QUIC_VERSION_12, version());
  DCHECK(!decompression_failed_);
  decompression_failed_ = true;
  session()->connection()->SendConnectionClose(QUIC_DECOMPRESSION_FAILURE);
}

void QuicDataStream::OnStreamHeaders(StringPiece headers_data) {
  DCHECK_LT(QUIC_VERSION_12, version());
  headers_data.AppendToString(&decompressed_headers_);
  ProcessHeaderData();
}

void QuicDataStream::OnStreamHeadersPriority(QuicPriority priority) {
  DCHECK(session()->connection()->is_server());
  set_priority(priority);
}

void QuicDataStream::OnStreamHeadersComplete(bool fin, size_t frame_len) {
  DCHECK_LT(QUIC_VERSION_12, version());
  headers_decompressed_ = true;
  if (fin) {
    sequencer()->OnStreamFrame(QuicStreamFrame(id(), fin, 0, IOVector()));
  }
  ProcessHeaderData();
  if (FinishedReadingHeaders()) {
    sequencer()->FlushBufferedFrames();
  }
}

void QuicDataStream::OnClose() {
  ReliableQuicStream::OnClose();

  if (visitor_) {
    Visitor* visitor = visitor_;
    // Calling Visitor::OnClose() may result the destruction of the visitor,
    // so we need to ensure we don't call it again.
    visitor_ = NULL;
    visitor->OnClose(this);
  }
}

uint32 QuicDataStream::StripPriorityAndHeaderId(
    const char* data, uint32 data_len) {
  uint32 total_bytes_parsed = 0;

  if (!priority_parsed_ && session()->connection()->is_server()) {
    QuicPriority temporary_priority = priority_;
    total_bytes_parsed = StripUint32(
        data, data_len, &headers_id_and_priority_buffer_, &temporary_priority);
    if (total_bytes_parsed > 0 && headers_id_and_priority_buffer_.size() == 0) {
      priority_parsed_ = true;

      // Spdy priorities are inverted, so the highest numerical value is the
      // lowest legal priority.
      if (temporary_priority > QuicUtils::LowestPriority()) {
        session()->connection()->SendConnectionClose(QUIC_INVALID_PRIORITY);
        return 0;
      }
      priority_ = temporary_priority;
    }
    data += total_bytes_parsed;
    data_len -= total_bytes_parsed;
  }
  if (data_len > 0 && headers_id_ == 0u) {
    // The headers ID has not yet been read.  Strip it from the beginning of
    // the data stream.
    total_bytes_parsed += StripUint32(
        data, data_len, &headers_id_and_priority_buffer_, &headers_id_);
  }
  return total_bytes_parsed;
}

bool QuicDataStream::FinishedReadingHeaders() {
  return (headers_id_ != 0 || version() > QUIC_VERSION_12) &&
      headers_decompressed_ && decompressed_headers_.empty();
}

void QuicDataStream::OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) {
  DVLOG(1) << "Received WindowUpdateFrame for stream: " << id()
           << ", with byte offset: " << frame.byte_offset;
  // TODO(rjshade): Adjust flow control window.
}

}  // namespace net
