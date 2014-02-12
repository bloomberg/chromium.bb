// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_spdy_decompressor.h"

#include <algorithm>

#include "base/logging.h"

using base::StringPiece;
using std::min;

namespace net {

class SpdyFramerVisitor : public SpdyFramerVisitorInterface {
 public:
  explicit SpdyFramerVisitor(QuicSpdyDecompressor::Visitor* visitor)
      : visitor_(visitor),
        error_(false) {
  }

  virtual void OnError(SpdyFramer* framer) OVERRIDE {
    error_ = true;
  }
  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) OVERRIDE {}
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) OVERRIDE {}
  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE;
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           bool fin,
                           bool unidirectional) OVERRIDE {}
  virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE {}
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE {}
  virtual void OnSetting(SpdySettingsIds id,
                         uint8 flags,
                         uint32 value) OVERRIDE {}
  virtual void OnPing(SpdyPingId unique_id) OVERRIDE {}
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE {}
  virtual void OnHeaders(SpdyStreamId stream_id, bool fin) OVERRIDE {}
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE {}
  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id) OVERRIDE {}
  void set_visitor(QuicSpdyDecompressor::Visitor* visitor) {
    DCHECK(visitor);
    visitor_ = visitor;
  }

 private:
  QuicSpdyDecompressor::Visitor* visitor_;
  bool error_;
};

bool SpdyFramerVisitor::OnControlFrameHeaderData(SpdyStreamId stream_id,
                                                 const char* header_data,
                                                 size_t len) {
  DCHECK(visitor_);
  return visitor_->OnDecompressedData(StringPiece(header_data, len));
}

QuicSpdyDecompressor::QuicSpdyDecompressor()
    : spdy_framer_(SPDY3),
      spdy_visitor_(new SpdyFramerVisitor(NULL)),
      current_header_id_(1),
      has_current_compressed_size_(false),
      current_compressed_size_(0),
      compressed_bytes_consumed_(0) {
  spdy_framer_.set_visitor(spdy_visitor_.get());
}

QuicSpdyDecompressor::~QuicSpdyDecompressor() {
}

size_t QuicSpdyDecompressor::DecompressData(StringPiece data,
                                            Visitor* visitor) {
  spdy_visitor_->set_visitor(visitor);
  size_t bytes_consumed = 0;

  if (!has_current_compressed_size_) {
    const size_t kCompressedBufferSizeSize = sizeof(uint32);
    DCHECK_GT(kCompressedBufferSizeSize, compressed_size_buffer_.length());
    size_t missing_size =
        kCompressedBufferSizeSize - compressed_size_buffer_.length();
    if (data.length() < missing_size) {
      data.AppendToString(&compressed_size_buffer_);
      return data.length();
    }
    bytes_consumed += missing_size;
    data.substr(0, missing_size).AppendToString(&compressed_size_buffer_);
    DCHECK_EQ(kCompressedBufferSizeSize, compressed_size_buffer_.length());
    memcpy(&current_compressed_size_, compressed_size_buffer_.data(),
           kCompressedBufferSizeSize);
    compressed_size_buffer_.clear();
    has_current_compressed_size_ = true;
    data = data.substr(missing_size);
    compressed_bytes_consumed_ = 0;
  }

  size_t bytes_to_consume =
      min(current_compressed_size_ - compressed_bytes_consumed_,
          static_cast<uint32>(data.length()));
  if (bytes_to_consume > 0) {
    if (!spdy_framer_.IncrementallyDecompressControlFrameHeaderData(
            current_header_id_, data.data(), bytes_to_consume)) {
      visitor->OnDecompressionError();
      return bytes_consumed;
    }
    compressed_bytes_consumed_ += bytes_to_consume;
    bytes_consumed += bytes_to_consume;
  }
  if (current_compressed_size_ - compressed_bytes_consumed_ == 0) {
    ResetForNextHeaders();
  }
  return bytes_consumed;
}

void QuicSpdyDecompressor::ResetForNextHeaders() {
  has_current_compressed_size_ = false;
  current_compressed_size_ = 0;
  compressed_bytes_consumed_ = 0;
  ++current_header_id_;
}

}  // namespace net
