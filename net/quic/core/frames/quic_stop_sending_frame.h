// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CORE_FRAMES_QUIC_STOP_SENDING_FRAME_H_
#define NET_QUIC_CORE_FRAMES_QUIC_STOP_SENDING_FRAME_H_

#include <ostream>

#include "net/quic/core/frames/quic_control_frame.h"
#include "net/quic/core/quic_error_codes.h"

namespace net {

struct QUIC_EXPORT_PRIVATE QuicStopSendingFrame : public QuicControlFrame {
  QuicStopSendingFrame();
  QuicStopSendingFrame(QuicControlFrameId control_frame_id,
                       QuicStreamId stream_id,
                       uint16_t application_error_code);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicStopSendingFrame& frame);

  QuicStreamId stream_id;
  QuicApplicationErrorCode application_error_code;
};

}  // namespace net

#endif  // NET_QUIC_CORE_FRAMES_QUIC_STOP_SENDING_FRAME_H_
