// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/core/spdy_framer_decoder_adapter.h"

#include <memory>
#include <utility>

#include "base/format_macros.h"
#include "base/logging.h"
#include "net/spdy/platform/api/spdy_estimate_memory_usage.h"
#include "net/spdy/platform/api/spdy_ptr_util.h"
#include "net/spdy/platform/api/spdy_string_utils.h"

#if defined(COMPILER_GCC)
#define PRETTY_THIS SpdyStringPrintf("%s@%p ", __PRETTY_FUNCTION__, this)
#elif defined(COMPILER_MSVC)
#define PRETTY_THIS SpdyStringPrintf("%s@%p ", __FUNCSIG__, this)
#else
#define PRETTY_THIS SpdyStringPrintf("%s@%p ", __func__, this)
#endif

namespace net {

SpdyFramerDecoderAdapter::SpdyFramerDecoderAdapter() {
  DVLOG(1) << PRETTY_THIS;
}

SpdyFramerDecoderAdapter::~SpdyFramerDecoderAdapter() {
  DVLOG(1) << PRETTY_THIS;
}

void SpdyFramerDecoderAdapter::set_visitor(
    SpdyFramerVisitorInterface* visitor) {
  visitor_ = visitor;
}

void SpdyFramerDecoderAdapter::set_debug_visitor(
    SpdyFramerDebugVisitorInterface* debug_visitor) {
  debug_visitor_ = debug_visitor;
}

void SpdyFramerDecoderAdapter::set_process_single_input_frame(bool v) {
  process_single_input_frame_ = v;
}

void SpdyFramerVisitorAdapter::OnError(SpdyFramer* framer) {
  visitor_->OnError(framer_);
}

void SpdyFramerVisitorAdapter::OnCommonHeader(SpdyStreamId stream_id,
                                              size_t length,
                                              uint8_t type,
                                              uint8_t flags) {
  visitor_->OnCommonHeader(stream_id, length, type, flags);
}

void SpdyFramerVisitorAdapter::OnDataFrameHeader(SpdyStreamId stream_id,
                                                 size_t length,
                                                 bool fin) {
  visitor_->OnDataFrameHeader(stream_id, length, fin);
}

void SpdyFramerVisitorAdapter::OnStreamFrameData(SpdyStreamId stream_id,
                                                 const char* data,
                                                 size_t len) {
  visitor_->OnStreamFrameData(stream_id, data, len);
}

void SpdyFramerVisitorAdapter::OnStreamEnd(SpdyStreamId stream_id) {
  visitor_->OnStreamEnd(stream_id);
}

void SpdyFramerVisitorAdapter::OnStreamPadding(SpdyStreamId stream_id,
                                               size_t len) {
  visitor_->OnStreamPadding(stream_id, len);
}

SpdyHeadersHandlerInterface* SpdyFramerVisitorAdapter::OnHeaderFrameStart(
    SpdyStreamId stream_id) {
  return visitor_->OnHeaderFrameStart(stream_id);
}

void SpdyFramerVisitorAdapter::OnHeaderFrameEnd(SpdyStreamId stream_id,
                                                bool end_headers) {
  visitor_->OnHeaderFrameEnd(stream_id, end_headers);
}

void SpdyFramerVisitorAdapter::OnRstStream(SpdyStreamId stream_id,
                                           SpdyErrorCode error_code) {
  visitor_->OnRstStream(stream_id, error_code);
}

void SpdyFramerVisitorAdapter::OnSetting(SpdySettingsIds id,
                                         uint32_t value) {
  visitor_->OnSetting(id, value);
}

void SpdyFramerVisitorAdapter::OnPing(SpdyPingId unique_id, bool is_ack) {
  visitor_->OnPing(unique_id, is_ack);
}

void SpdyFramerVisitorAdapter::OnSettings(bool clear_persisted) {
  visitor_->OnSettings(clear_persisted);
}

void SpdyFramerVisitorAdapter::OnSettingsAck() {
  visitor_->OnSettingsAck();
}

void SpdyFramerVisitorAdapter::OnSettingsEnd() {
  visitor_->OnSettingsEnd();
}

void SpdyFramerVisitorAdapter::OnGoAway(SpdyStreamId last_accepted_stream_id,
                                        SpdyErrorCode error_code) {
  visitor_->OnGoAway(last_accepted_stream_id, error_code);
}

void SpdyFramerVisitorAdapter::OnHeaders(SpdyStreamId stream_id,
                                         bool has_priority,
                                         int weight,
                                         SpdyStreamId parent_stream_id,
                                         bool exclusive,
                                         bool fin,
                                         bool end) {
  visitor_->OnHeaders(stream_id, has_priority, weight, parent_stream_id,
                      exclusive, fin, end);
}

void SpdyFramerVisitorAdapter::OnWindowUpdate(SpdyStreamId stream_id,
                                              int delta_window_size) {
  visitor_->OnWindowUpdate(stream_id, delta_window_size);
}

bool SpdyFramerVisitorAdapter::OnGoAwayFrameData(const char* goaway_data,
                                                 size_t len) {
  return visitor_->OnGoAwayFrameData(goaway_data, len);
}

void SpdyFramerVisitorAdapter::OnPushPromise(SpdyStreamId stream_id,
                                             SpdyStreamId promised_stream_id,
                                             bool end) {
  visitor_->OnPushPromise(stream_id, promised_stream_id, end);
}

void SpdyFramerVisitorAdapter::OnContinuation(SpdyStreamId stream_id,
                                              bool end) {
  visitor_->OnContinuation(stream_id, end);
}

void SpdyFramerVisitorAdapter::OnPriority(SpdyStreamId stream_id,
                                          SpdyStreamId parent_id,
                                          int weight,
                                          bool exclusive) {
  visitor_->OnPriority(stream_id, parent_id, weight, exclusive);
}

void SpdyFramerVisitorAdapter::OnAltSvc(
    SpdyStreamId stream_id,
    SpdyStringPiece origin,
    const SpdyAltSvcWireFormat::AlternativeServiceVector& altsvc_vector) {
  visitor_->OnAltSvc(stream_id, origin, altsvc_vector);
}

bool SpdyFramerVisitorAdapter::OnUnknownFrame(SpdyStreamId stream_id,
                                              uint8_t frame_type) {
  return visitor_->OnUnknownFrame(stream_id, frame_type);
}

class NestedSpdyFramerDecoder : public SpdyFramerDecoderAdapter {
  typedef SpdyFramer::SpdyState SpdyState;
  typedef SpdyFramer::SpdyFramerError SpdyFramerError;

 public:
  explicit NestedSpdyFramerDecoder(SpdyFramer* outer)
      : framer_(nullptr,
                outer->compression_enabled() ? SpdyFramer::ENABLE_COMPRESSION
                                             : SpdyFramer::DISABLE_COMPRESSION),
        outer_(outer) {
    DVLOG(1) << PRETTY_THIS;
  }
  ~NestedSpdyFramerDecoder() override { DVLOG(1) << PRETTY_THIS; }

  // Wrap the visitor in a SpdyFramerVisitorAdapter so that the correct
  // SpdyFramer instance is passed to OnError. Passes the call on to the
  // base adapter class and wrapped SpdyFramer.
  void set_visitor(SpdyFramerVisitorInterface* visitor) override {
    visitor_adapter_ =
        SpdyMakeUnique<SpdyFramerVisitorAdapter>(visitor, outer_);
    SpdyFramerDecoderAdapter::set_visitor(visitor_adapter_.get());
    framer_.set_visitor(visitor_adapter_.get());
  }

  void set_extension_visitor(ExtensionVisitorInterface* visitor) override {
    framer_.set_extension_visitor(visitor);
  }

  // Passes the call on to the base adapter class and wrapped SpdyFramer.
  void set_debug_visitor(
      SpdyFramerDebugVisitorInterface* debug_visitor) override {
    SpdyFramerDecoderAdapter::set_debug_visitor(debug_visitor);
    framer_.set_debug_visitor(debug_visitor);
  }

  // Passes the call on to the wrapped SpdyFramer.
  void SetDecoderHeaderTableDebugVisitor(
      std::unique_ptr<HpackHeaderTable::DebugVisitorInterface> visitor)
      override {
        framer_.SetDecoderHeaderTableDebugVisitor(std::move(visitor));
  }

  // Passes the call on to the base adapter class and wrapped SpdyFramer.
  void set_process_single_input_frame(bool v) override {
    SpdyFramerDecoderAdapter::set_process_single_input_frame(v);
    framer_.set_process_single_input_frame(v);
  }

  size_t ProcessInput(const char* data, size_t len) override {
    DVLOG(2) << "ProcessInput(data, " << len << ")";
    size_t result = framer_.ProcessInput(data, len);
    DVLOG(2) << "ProcessInput(data, " << len << ")  returning " << result;
    return result;
  }

  void Reset() override { framer_.Reset(); }

  SpdyFramer::SpdyFramerError spdy_framer_error() const override {
    return framer_.spdy_framer_error();
  }
  SpdyFramer::SpdyState state() const override { return framer_.state(); }
  bool probable_http_response() const override {
    return framer_.probable_http_response();
  }
  size_t EstimateMemoryUsage() const override {
    // Skip |visitor_adapter_| because it doesn't allocate.
    return SpdyEstimateMemoryUsage(framer_);
  }

 private:
  SpdyFramer framer_;
  SpdyFramer* const outer_;
  std::unique_ptr<SpdyFramerVisitorAdapter> visitor_adapter_;
};

std::unique_ptr<SpdyFramerDecoderAdapter> CreateNestedSpdyFramerDecoder(
    SpdyFramer* outer) {
  return SpdyMakeUnique<NestedSpdyFramerDecoder>(outer);
}

}  // namespace net
