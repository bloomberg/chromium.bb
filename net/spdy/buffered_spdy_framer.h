// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_BUFFERED_SPDY_FRAMER_H_
#define NET_SPDY_BUFFERED_SPDY_FRAMER_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE BufferedSpdyFramerVisitorInterface {
 public:
  BufferedSpdyFramerVisitorInterface() {}

  // Called if an error is detected in the SpdyFrame protocol.
  virtual void OnError(SpdyFramer::SpdyError error_code) = 0;

  // Called if an error is detected in a SPDY stream.
  virtual void OnStreamError(SpdyStreamId stream_id,
                             const std::string& description) = 0;

  // Called after all the header data for SYN_STREAM control frame is received.
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           uint8 credential_slot,
                           bool fin,
                           bool unidirectional,
                           const SpdyHeaderBlock& headers) = 0;

  // Called after all the header data for SYN_REPLY control frame is received.
  virtual void OnSynReply(SpdyStreamId stream_id,
                          bool fin,
                          const SpdyHeaderBlock& headers) = 0;

  // Called after all the header data for HEADERS control frame is received.
  virtual void OnHeaders(SpdyStreamId stream_id,
                         bool fin,
                         const SpdyHeaderBlock& headers) = 0;

  // Called when data is received.
  // |stream_id| The stream receiving data.
  // |data| A buffer containing the data received.
  // |len| The length of the data buffer.
  // When the other side has finished sending data on this stream,
  // this method will be called with a zero-length buffer.
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 SpdyDataFlags flags) = 0;

  // Called when an individual setting within a SETTINGS frame has been parsed
  // and validated.
  virtual void OnSetting(SpdySettingsIds id, uint8 flags, uint32 value) = 0;

  // Called when a PING frame has been parsed.
  virtual void OnPing(uint32 unique_id) = 0;

  // Called when a RST_STREAM frame has been parsed.
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) = 0;

  // Called when a GOAWAY frame has been parsed.
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) = 0;

  // Called when a WINDOW_UPDATE frame has been parsed.
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) = 0;

  // Called after a control frame has been compressed to allow the visitor
  // to record compression statistics.
  virtual void OnSynStreamCompressed(
      size_t uncompressed_size,
      size_t compressed_size) = 0;

 protected:
  virtual ~BufferedSpdyFramerVisitorInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedSpdyFramerVisitorInterface);
};

class NET_EXPORT_PRIVATE BufferedSpdyFramer
    : public SpdyFramerVisitorInterface {
 public:
  BufferedSpdyFramer(int version,
                     bool enable_compression);
  virtual ~BufferedSpdyFramer();

  // Sets callbacks to be called from the buffered spdy framer.  A visitor must
  // be set, or else the framer will likely crash.  It is acceptable for the
  // visitor to do nothing.  If this is called multiple times, only the last
  // visitor will be used.
  void set_visitor(BufferedSpdyFramerVisitorInterface* visitor);

  // SpdyFramerVisitorInterface
  virtual void OnError(SpdyFramer* spdy_framer) OVERRIDE;
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           uint8 credential_slot,
                           bool fin,
                           bool unidirectional) OVERRIDE;
  virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE;
  virtual void OnHeaders(SpdyStreamId stream_id, bool fin) OVERRIDE;
  virtual bool OnCredentialFrameData(const char* frame_data,
                                     size_t len) OVERRIDE;
  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE;
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 SpdyDataFlags flags) OVERRIDE;
  virtual void OnSetting(
      SpdySettingsIds id, uint8 flags, uint32 value) OVERRIDE;
  virtual void OnPing(uint32 unique_id) OVERRIDE;
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE;
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE;
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE;
  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) OVERRIDE;

  // Called after a syn stream control frame has been compressed to
  // allow the visitor to record compression statistics.
  virtual void OnSynStreamCompressed(
      size_t uncompressed_size,
      size_t compressed_size) OVERRIDE;

  // SpdyFramer methods.
  size_t ProcessInput(const char* data, size_t len);
  int protocol_version();
  void Reset();
  SpdyFramer::SpdyError error_code() const;
  SpdyFramer::SpdyState state() const;
  bool MessageFullyRead();
  bool HasError();
  SpdyFrame* CreateSynStream(SpdyStreamId stream_id,
                             SpdyStreamId associated_stream_id,
                             SpdyPriority priority,
                             uint8 credential_slot,
                             SpdyControlFlags flags,
                             bool compressed,
                             const SpdyHeaderBlock* headers);
  SpdyFrame* CreateSynReply(SpdyStreamId stream_id,
                            SpdyControlFlags flags,
                            bool compressed,
                            const SpdyHeaderBlock* headers);
  SpdyFrame* CreateRstStream(SpdyStreamId stream_id,
                             SpdyRstStreamStatus status) const;
  SpdyFrame* CreateSettings(const SettingsMap& values) const;
  SpdyFrame* CreatePingFrame(uint32 unique_id) const;
  SpdyFrame* CreateGoAway(
      SpdyStreamId last_accepted_stream_id,
      SpdyGoAwayStatus status) const;
  SpdyFrame* CreateHeaders(SpdyStreamId stream_id,
                           SpdyControlFlags flags,
                           bool compressed,
                           const SpdyHeaderBlock* headers);
  SpdyFrame* CreateWindowUpdate(
      SpdyStreamId stream_id,
      uint32 delta_window_size) const;
  SpdyFrame* CreateCredentialFrame(
      const SpdyCredential& credential) const;
  SpdyFrame* CreateDataFrame(SpdyStreamId stream_id,
                             const char* data,
                             uint32 len,
                             SpdyDataFlags flags);
  SpdyPriority GetHighestPriority() const;

  // Returns the (minimum) size of control frames (sans variable-length
  // portions).
  size_t GetControlFrameHeaderSize() const {
    return spdy_framer_.GetControlFrameHeaderSize();
  }

  int frames_received() const { return frames_received_; }

 private:
  // The size of the header_buffer_.
  enum { kHeaderBufferSize = 32 * 1024 };

  void InitHeaderStreaming(SpdyStreamId stream_id);

  SpdyFramer spdy_framer_;
  BufferedSpdyFramerVisitorInterface* visitor_;

  // Header block streaming state:
  char header_buffer_[kHeaderBufferSize];
  size_t header_buffer_used_;
  bool header_buffer_valid_;
  SpdyStreamId header_stream_id_;
  int frames_received_;

  // Collection of fields from control frames that we need to
  // buffer up from the spdy framer.
  struct ControlFrameFields {
    SpdyControlType type;
    SpdyStreamId stream_id;
    SpdyStreamId associated_stream_id;
    SpdyPriority priority;
    uint8 credential_slot;
    bool fin;
    bool unidirectional;
  };
  scoped_ptr<ControlFrameFields> control_frame_fields_;

  DISALLOW_COPY_AND_ASSIGN(BufferedSpdyFramer);
};

}  // namespace net

#endif  // NET_SPDY_BUFFERED_SPDY_FRAMER_H_
