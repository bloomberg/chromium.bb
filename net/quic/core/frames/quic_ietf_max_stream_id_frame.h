// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CORE_FRAMES_QUIC_IETF_MAX_STREAM_ID_FRAME_H_
#define NET_QUIC_CORE_FRAMES_QUIC_IETF_MAX_STREAM_ID_FRAME_H_

#include <ostream>

#include "net/quic/core/frames/quic_control_frame.h"

namespace net {

// IETF format max-stream id frame.
// This frame is used by the sender to inform the peer of the largest
// stream id that the peer may open and that the sender will accept.
struct QUIC_EXPORT_PRIVATE QuicIetfMaxStreamIdFrame : public QuicControlFrame {
  QuicIetfMaxStreamIdFrame();
  QuicIetfMaxStreamIdFrame(QuicControlFrameId control_frame_id,
                           QuicStreamId max_stream_id);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicIetfMaxStreamIdFrame& frame);

  // The maximum stream id to support.
  QuicStreamId max_stream_id;
};

}  // namespace net

#endif  // NET_QUIC_CORE_FRAMES_QUIC_IETF_MAX_STREAM_ID_FRAME_H_
