// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// State machine for congestion control. The state is updated by calls from
// other modules as they detect events, or decide on taking specific actions.
// Events include things like packet loss, or growing delay, while decisions
// include decisions to increase or decrease bitrates.
// This class should be called for every event and decision made by the
// congestion control, this class will coalesce all calls relative to the
// smoothed RTT.

#ifndef NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_STATE_MACHINE_H_
#define NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_STATE_MACHINE_H_

#include "net/base/net_export.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_time.h"

namespace net {

//  State transition diagram.
//
//                     kInterArrivalStatePacketLoss
//                                   |
//                       kInterArrivalStateStable
//                                   |
//                       kInterArrivalStateDelay
//                           |              |
//  kInterArrivalStateCompetingFlow -> kInterArrivalStateCompetingTcpFLow

enum NET_EXPORT_PRIVATE InterArrivalState {
  // We are on a network with a delay that is too small to be reliably detected,
  // such as a local ethernet.
  kInterArrivalStatePacketLoss = 1,
  // We are on an underutilized network operating with low delay and low loss.
  kInterArrivalStateStable = 2,
  // We are on a network where we can detect delay changes and suffer only
  // low loss. Nothing indicates that we are competing with another flow.
  kInterArrivalStateDelay = 3,
  // We are on a network where we can detect delay changes and suffer only
  // low loss. We have indications that we are compete with another flow.
  kInterArrivalStateCompetingFlow = 4,
  // We are on a network where we can detect delay changes, however we suffer
  // packet loss due to sharing the bottleneck link with another flow, which is
  // most likely a TCP flow.
  kInterArrivalStateCompetingTcpFLow = 5,
};

class NET_EXPORT_PRIVATE InterArrivalStateMachine {
 public:
  explicit InterArrivalStateMachine(const QuicClock* clock);

  InterArrivalState GetInterArrivalState();

  // Inter arrival congestion control decided to increase bitrate.
  void IncreaseBitrateDecision();

  // Inter arrival congestion control decided to decrease bitrate.
  void DecreaseBitrateDecision();

  // Estimated smoothed round trip time.
  // This should be called whenever the smoothed RTT estimate is updated.
  void set_rtt(QuicTime::Delta rtt);

  // This method is called when a packet loss was reported.
  bool PacketLossEvent();

  // This method is called when we believe that packet transit delay is
  // increasing, presumably due to a growing queue along the path.
  bool IncreasingDelayEvent();

 private:
  const QuicClock* clock_;
  InterArrivalState current_state_;
  QuicTime::Delta smoothed_rtt_;

  int decrease_event_count_;
  QuicTime last_decrease_event_;

  int increase_event_count_;
  QuicTime last_increase_event_;

  int loss_event_count_;
  QuicTime last_loss_event_;

  int delay_event_count_;
  QuicTime last_delay_event_;

  DISALLOW_COPY_AND_ASSIGN(InterArrivalStateMachine);
};

}  // namespace net
#endif  // NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_STATE_MACHINE_H_
