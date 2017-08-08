// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_CORE_HTTP2_FRAME_DECODER_ADAPTER_H_
#define NET_SPDY_CORE_HTTP2_FRAME_DECODER_ADAPTER_H_

// Provides a SpdyFramerDecoderAdapter that uses Http2FrameDecoder for decoding
// HTTP/2 frames. The adapter does not directly decode HPACK, but instead calls
// SpdyFramer::GetHpackDecoderAdapter() to get the decoder to be used.

#include <stddef.h>

#include <cstdint>
#include <memory>

#include "net/spdy/core/hpack/hpack_header_table.h"
#include "net/spdy/core/spdy_alt_svc_wire_format.h"
#include "net/spdy/core/spdy_framer.h"
#include "net/spdy/core/spdy_headers_handler_interface.h"
#include "net/spdy/core/spdy_protocol.h"
#include "net/spdy/platform/api/spdy_string_piece.h"

namespace net {

// SpdyFramerDecoderAdapter will use the given visitor implementing this
// interface to deliver event callbacks as frames are decoded.
//
// Control frames that contain HTTP2 header blocks (HEADER, and PUSH_PROMISE)
// are processed in fashion that allows the decompressed header block to be
// delivered in chunks to the visitor.
// The following steps are followed:
//   1. OnHeaders, or OnPushPromise is called.
//   2. OnHeaderFrameStart is called; visitor is expected to return an instance
//      of SpdyHeadersHandlerInterface that will receive the header key-value
//      pairs.
//   3. OnHeaderFrameEnd is called, indicating that the full header block has
//      been delivered for the control frame.
// During step 2, if the visitor is not interested in accepting the header data,
// it should return a no-op implementation of SpdyHeadersHandlerInterface.
class SPDY_EXPORT_PRIVATE SpdyFramerVisitorInterface {
 public:
  virtual ~SpdyFramerVisitorInterface() {}

  // Called if an error is detected in the SpdyFrame protocol.
  virtual void OnError(SpdyFramer::SpdyFramerError error) = 0;

  // Called when the common header for a frame is received. Validating the
  // common header occurs in later processing.
  virtual void OnCommonHeader(SpdyStreamId /*stream_id*/,
                              size_t /*length*/,
                              uint8_t /*type*/,
                              uint8_t /*flags*/) {}

  // Called when a data frame header is received. The frame's data
  // payload will be provided via subsequent calls to
  // OnStreamFrameData().
  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) = 0;

  // Called when data is received.
  // |stream_id| The stream receiving data.
  // |data| A buffer containing the data received.
  // |len| The length of the data buffer.
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len) = 0;

  // Called when the other side has finished sending data on this stream.
  // |stream_id| The stream that was receiving data.
  virtual void OnStreamEnd(SpdyStreamId stream_id) = 0;

  // Called when padding is received (padding length field or padding octets).
  // |stream_id| The stream receiving data.
  // |len| The number of padding octets.
  virtual void OnStreamPadding(SpdyStreamId stream_id, size_t len) = 0;

  // Called just before processing the payload of a frame containing header
  // data. Should return an implementation of SpdyHeadersHandlerInterface that
  // will receive headers for stream |stream_id|. The caller will not take
  // ownership of the headers handler. The same instance should remain live
  // and be returned for all header frames comprising a logical header block
  // (i.e. until OnHeaderFrameEnd() is called).
  virtual SpdyHeadersHandlerInterface* OnHeaderFrameStart(
      SpdyStreamId stream_id) = 0;

  // Called after processing the payload of a frame containing header data.
  virtual void OnHeaderFrameEnd(SpdyStreamId stream_id) = 0;

  // Called when a RST_STREAM frame has been parsed.
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyErrorCode error_code) = 0;

  // Called when a SETTINGS frame is received.
  virtual void OnSettings() {}

  // Called when a complete setting within a SETTINGS frame has been parsed and
  // validated.
  virtual void OnSetting(SpdySettingsIds id, uint32_t value) = 0;

  // Called when a SETTINGS frame is received with the ACK flag set.
  virtual void OnSettingsAck() {}

  // Called before and after parsing SETTINGS id and value tuples.
  virtual void OnSettingsEnd() = 0;

  // Called when a PING frame has been parsed.
  virtual void OnPing(SpdyPingId unique_id, bool is_ack) = 0;

  // Called when a GOAWAY frame has been parsed.
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyErrorCode error_code) = 0;

  // Called when a HEADERS frame is received.
  // Note that header block data is not included. See OnHeaderFrameStart().
  // |stream_id| The stream receiving the header.
  // |has_priority| Whether or not the headers frame included a priority value,
  //     and stream dependency info.
  // |weight| If |has_priority| is true, then weight (in the range [1, 256])
  //     for the receiving stream, otherwise 0.
  // |parent_stream_id| If |has_priority| is true the parent stream of the
  //     receiving stream, else 0.
  // |exclusive| If |has_priority| is true the exclusivity of dependence on the
  //     parent stream, else false.
  // |fin| Whether FIN flag is set in frame headers.
  // |end| False if HEADERs frame is to be followed by a CONTINUATION frame,
  //     or true if not.
  virtual void OnHeaders(SpdyStreamId stream_id,
                         bool has_priority,
                         int weight,
                         SpdyStreamId parent_stream_id,
                         bool exclusive,
                         bool fin,
                         bool end) = 0;

  // Called when a WINDOW_UPDATE frame has been parsed.
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              int delta_window_size) = 0;

  // Called when a goaway frame opaque data is available.
  // |goaway_data| A buffer containing the opaque GOAWAY data chunk received.
  // |len| The length of the header data buffer. A length of zero indicates
  //       that the header data block has been completely sent.
  // When this function returns true the visitor indicates that it accepted
  // all of the data. Returning false indicates that that an error has
  // occurred while processing the data. Default implementation returns true.
  virtual bool OnGoAwayFrameData(const char* goaway_data, size_t len);

  // Called when a PUSH_PROMISE frame is received.
  // Note that header block data is not included. See OnHeaderFrameStart().
  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id,
                             bool end) = 0;

  // Called when a CONTINUATION frame is received.
  // Note that header block data is not included. See OnHeaderFrameStart().
  virtual void OnContinuation(SpdyStreamId stream_id, bool end) = 0;

  // Called when an ALTSVC frame has been parsed.
  virtual void OnAltSvc(
      SpdyStreamId /*stream_id*/,
      SpdyStringPiece /*origin*/,
      const SpdyAltSvcWireFormat::AlternativeServiceVector& /*altsvc_vector*/) {
  }

  // Called when a PRIORITY frame is received.
  // |stream_id| The stream to update the priority of.
  // |parent_stream_id| The parent stream of |stream_id|.
  // |weight| Stream weight, in the range [1, 256].
  // |exclusive| Whether |stream_id| should be an only child of
  //     |parent_stream_id|.
  virtual void OnPriority(SpdyStreamId stream_id,
                          SpdyStreamId parent_stream_id,
                          int weight,
                          bool exclusive) = 0;

  // Called when a frame type we don't recognize is received.
  // Return true if this appears to be a valid extension frame, false otherwise.
  // We distinguish between extension frames and nonsense by checking
  // whether the stream id is valid.
  virtual bool OnUnknownFrame(SpdyStreamId stream_id, uint8_t frame_type) = 0;
};

// Abstract base class for an HTTP/2 decoder to be called from SpdyFramer.
class SpdyFramerDecoderAdapter {
 public:
  SpdyFramerDecoderAdapter();
  virtual ~SpdyFramerDecoderAdapter();

  // Set callbacks to be called from the framer.  A visitor must be set, or
  // else the framer will likely crash.  It is acceptable for the visitor
  // to do nothing.  If this is called multiple times, only the last visitor
  // will be used.
  virtual void set_visitor(SpdyFramerVisitorInterface* visitor);
  SpdyFramerVisitorInterface* visitor() const { return visitor_; }

  // Set extension callbacks to be called from the framer or decoder. Optional.
  // If called multiple times, only the last visitor will be used.
  virtual void set_extension_visitor(ExtensionVisitorInterface* visitor) = 0;

  // Set debug callbacks to be called from the framer. The debug visitor is
  // completely optional and need not be set in order for normal operation.
  // If this is called multiple times, only the last visitor will be used.
  virtual void set_debug_visitor(
      SpdyFramerDebugVisitorInterface* debug_visitor);
  SpdyFramerDebugVisitorInterface* debug_visitor() const {
    return debug_visitor_;
  }

  // Set debug callbacks to be called from the HPACK decoder.
  virtual void SetDecoderHeaderTableDebugVisitor(
      std::unique_ptr<HpackHeaderTable::DebugVisitorInterface> visitor) = 0;

  // Sets whether or not ProcessInput returns after finishing a frame, or
  // continues processing additional frames. Normally ProcessInput processes
  // all input, but this method enables the caller (and visitor) to work with
  // a single frame at a time (or that portion of the frame which is provided
  // as input). Reset() does not change the value of this flag.
  virtual void set_process_single_input_frame(bool v);
  bool process_single_input_frame() const {
    return process_single_input_frame_;
  }

  // Decode the |len| bytes of encoded HTTP/2 starting at |*data|. Returns
  // the number of bytes consumed. It is safe to pass more bytes in than
  // may be consumed. Should process (or otherwise buffer) as much as
  // available, unless process_single_input_frame is true.
  virtual size_t ProcessInput(const char* data, size_t len) = 0;

  // Reset the decoder (used just for tests at this time).
  virtual void Reset() = 0;

  // Current state of the decoder.
  virtual SpdyFramer::SpdyState state() const = 0;

  // Current error code (NO_ERROR if state != ERROR).
  virtual SpdyFramer::SpdyFramerError spdy_framer_error() const = 0;

  // Has any frame header looked like the start of an HTTP/1.1 (or earlier)
  // response? Used to detect if a backend/server that we sent a request to
  // has responded with an HTTP/1.1 (or earlier) response.
  virtual bool probable_http_response() const = 0;

  // Returns the estimate of dynamically allocated memory in bytes.
  virtual size_t EstimateMemoryUsage() const = 0;

  // Get (and lazily initialize) the HPACK decoder state.
  virtual HpackDecoderAdapter* GetHpackDecoder() = 0;

 private:
  SpdyFramerVisitorInterface* visitor_ = nullptr;
  SpdyFramerDebugVisitorInterface* debug_visitor_ = nullptr;
  bool process_single_input_frame_ = false;
};

std::unique_ptr<SpdyFramerDecoderAdapter> CreateHttp2FrameDecoderAdapter();

}  // namespace net

#endif  // NET_SPDY_CORE_HTTP2_FRAME_DECODER_ADAPTER_H_
