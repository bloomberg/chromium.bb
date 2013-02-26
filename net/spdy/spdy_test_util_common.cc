// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_test_util_common.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "net/spdy/buffered_spdy_framer.h"

namespace net {

namespace {

class PriorityGetter : public BufferedSpdyFramerVisitorInterface {
 public:
  PriorityGetter() : priority_(0) {}
  virtual ~PriorityGetter() {}

  SpdyPriority priority() const {
    return priority_;
  }

  virtual void OnError(SpdyFramer::SpdyError error_code) OVERRIDE {}
  virtual void OnStreamError(SpdyStreamId stream_id,
                             const std::string& description) OVERRIDE {}
  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           uint8 credential_slot,
                           bool fin,
                           bool unidirectional,
                           const SpdyHeaderBlock& headers) OVERRIDE {
    priority_ = priority;
  }
  virtual void OnSynReply(SpdyStreamId stream_id,
                          bool fin,
                          const SpdyHeaderBlock& headers) OVERRIDE {}
  virtual void OnHeaders(SpdyStreamId stream_id,
                         bool fin,
                         const SpdyHeaderBlock& headers) OVERRIDE {}
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 SpdyDataFlags flags) OVERRIDE {}
  virtual void OnSetting(
      SpdySettingsIds id, uint8 flags, uint32 value) OVERRIDE {}
  virtual void OnPing(uint32 unique_id) OVERRIDE {}
  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE {}
  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE {}
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              int delta_window_size) OVERRIDE {}
  virtual void OnSynStreamCompressed(
      size_t uncompressed_size,
      size_t compressed_size) OVERRIDE {}

 private:
  SpdyPriority priority_;
};

}  // namespace

bool GetSpdyPriority(int version,
                     const SpdyFrame& frame,
                     SpdyPriority* priority) {
  BufferedSpdyFramer framer(version, false);
  PriorityGetter priority_getter;
  framer.set_visitor(&priority_getter);
  size_t frame_size = frame.size();
  if (framer.ProcessInput(frame.data(), frame_size) != frame_size) {
    return false;
  }
  *priority = priority_getter.priority();
  return true;
}

} // namespace net
