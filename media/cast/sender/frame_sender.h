// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the base class for an object that send frames to a receiver.
// TODO(hclam): Refactor such that there is no separate AudioSender vs.
// VideoSender, and the functionality of both is rolled into this class.

#ifndef MEDIA_CAST_SENDER_FRAME_SENDER_H_
#define MEDIA_CAST_SENDER_FRAME_SENDER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/rtcp/rtcp.h"
#include "media/cast/sender/rtp_timestamp_helper.h"

namespace media {
namespace cast {

class FrameSender {
 public:
  FrameSender(scoped_refptr<CastEnvironment> cast_environment,
              CastTransportSender* const transport_sender,
              base::TimeDelta rtcp_interval,
              int frequency,
              uint32 ssrc);
  virtual ~FrameSender();

 protected:
  // Schedule and execute periodic sending of RTCP report.
  void ScheduleNextRtcpReport();
  void SendRtcpReport(bool schedule_future_reports);

  void OnReceivedRtt(base::TimeDelta rtt,
                     base::TimeDelta avg_rtt,
                     base::TimeDelta min_rtt,
                     base::TimeDelta max_rtt);

  bool is_rtt_available() const { return rtt_available_; }

  const scoped_refptr<CastEnvironment> cast_environment_;

  // Sends encoded frames over the configured transport (e.g., UDP).  In
  // Chromium, this could be a proxy that first sends the frames from a renderer
  // process to the browser process over IPC, with the browser process being
  // responsible for "packetizing" the frames and pushing packets into the
  // network layer.
  CastTransportSender* const transport_sender_;

  // Records lip-sync (i.e., mapping of RTP <--> NTP timestamps), and
  // extrapolates this mapping to any other point in time.
  RtpTimestampHelper rtp_timestamp_helper_;

  // RTT information from RTCP.
  bool rtt_available_;
  base::TimeDelta rtt_;
  base::TimeDelta avg_rtt_;
  base::TimeDelta min_rtt_;
  base::TimeDelta max_rtt_;

 private:
  const base::TimeDelta rtcp_interval_;
  const uint32 ssrc_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FrameSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_FRAME_SENDER_H_
