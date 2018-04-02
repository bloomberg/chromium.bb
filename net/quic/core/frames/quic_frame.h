// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CORE_FRAMES_QUIC_FRAME_H_
#define NET_QUIC_CORE_FRAMES_QUIC_FRAME_H_

#include <ostream>
#include <vector>

#include "net/quic/core/frames/quic_ack_frame.h"
#include "net/quic/core/frames/quic_blocked_frame.h"
#include "net/quic/core/frames/quic_connection_close_frame.h"
#include "net/quic/core/frames/quic_goaway_frame.h"
#include "net/quic/core/frames/quic_ietf_blocked_frame.h"
#include "net/quic/core/frames/quic_ietf_max_stream_id_frame.h"
#include "net/quic/core/frames/quic_ietf_stream_id_blocked_frame.h"
#include "net/quic/core/frames/quic_mtu_discovery_frame.h"
#include "net/quic/core/frames/quic_padding_frame.h"
#include "net/quic/core/frames/quic_path_challenge_frame.h"
#include "net/quic/core/frames/quic_path_response_frame.h"
#include "net/quic/core/frames/quic_ping_frame.h"
#include "net/quic/core/frames/quic_rst_stream_frame.h"
#include "net/quic/core/frames/quic_stop_sending_frame.h"
#include "net/quic/core/frames/quic_stop_waiting_frame.h"
#include "net/quic/core/frames/quic_stream_frame.h"
#include "net/quic/core/frames/quic_window_update_frame.h"
#include "net/quic/core/quic_types.h"
#include "net/quic/platform/api/quic_export.h"

namespace net {

struct QUIC_EXPORT_PRIVATE QuicFrame {
  QuicFrame();
  explicit QuicFrame(QuicPaddingFrame padding_frame);
  explicit QuicFrame(QuicMtuDiscoveryFrame frame);
  explicit QuicFrame(QuicPingFrame frame);

  explicit QuicFrame(QuicStreamFrame* stream_frame);
  explicit QuicFrame(QuicAckFrame* frame);
  explicit QuicFrame(QuicRstStreamFrame* frame);
  explicit QuicFrame(QuicConnectionCloseFrame* frame);
  explicit QuicFrame(QuicStopWaitingFrame* frame);
  explicit QuicFrame(QuicGoAwayFrame* frame);
  explicit QuicFrame(QuicWindowUpdateFrame* frame);
  explicit QuicFrame(QuicBlockedFrame* frame);

  QUIC_EXPORT_PRIVATE friend std::ostream& operator<<(std::ostream& os,
                                                      const QuicFrame& frame);

  QuicFrameType type;
  union {
    // Frames smaller than a pointer are inline.
    QuicPaddingFrame padding_frame;
    QuicMtuDiscoveryFrame mtu_discovery_frame;
    QuicPingFrame ping_frame;

    // Frames larger than a pointer.
    QuicStreamFrame* stream_frame;
    QuicAckFrame* ack_frame;
    QuicStopWaitingFrame* stop_waiting_frame;
    QuicRstStreamFrame* rst_stream_frame;
    QuicConnectionCloseFrame* connection_close_frame;
    QuicGoAwayFrame* goaway_frame;
    QuicWindowUpdateFrame* window_update_frame;
    QuicBlockedFrame* blocked_frame;
  };
};
// QuicFrameType consumes 8 bytes with padding.
static_assert(sizeof(QuicFrame) <= 16,
              "Frames larger than 8 bytes should be referenced by pointer.");

typedef std::vector<QuicFrame> QuicFrames;

// Deletes all the sub-frames contained in |frames|.
QUIC_EXPORT_PRIVATE void DeleteFrames(QuicFrames* frames);

// Delete the sub-frame contained in |frame|.
QUIC_EXPORT_PRIVATE void DeleteFrame(QuicFrame* frame);

// Deletes all the QuicStreamFrames for the specified |stream_id|.
QUIC_EXPORT_PRIVATE void RemoveFramesForStream(QuicFrames* frames,
                                               QuicStreamId stream_id);

// Returns true if |type| is a retransmittable control frame.
QUIC_EXPORT_PRIVATE bool IsControlFrame(QuicFrameType type);

// Returns control_frame_id of |frame|. Returns kInvalidControlFrameId if
// |frame| does not have a valid control_frame_id.
QUIC_EXPORT_PRIVATE QuicControlFrameId
GetControlFrameId(const QuicFrame& frame);

// Sets control_frame_id of |frame| to |control_frame_id|.
QUIC_EXPORT_PRIVATE void SetControlFrameId(QuicControlFrameId control_frame_id,
                                           QuicFrame* frame);

// Returns a copy of |frame|.
QUIC_EXPORT_PRIVATE QuicFrame
CopyRetransmittableControlFrame(const QuicFrame& frame);

}  // namespace net

#endif  // NET_QUIC_CORE_FRAMES_QUIC_FRAME_H_
