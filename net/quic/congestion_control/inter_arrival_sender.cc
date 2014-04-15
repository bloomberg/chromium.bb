// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/inter_arrival_sender.h"

#include <algorithm>

#include "base/stl_util.h"
#include "net/quic/congestion_control/rtt_stats.h"

using std::max;
using std::min;

namespace net {

namespace {
const int64 kProbeBitrateKBytesPerSecond = 1200;  // 9.6 Mbit/s
const float kPacketLossBitrateReduction = 0.7f;
const float kUncertainSafetyMargin = 0.7f;
const float kMaxBitrateReduction = 0.9f;
const float kMinBitrateReduction = 0.05f;
const uint64 kMinBitrateKbit = 10;

static const int kHistoryPeriodMs = 5000;
static const int kBitrateSmoothingPeriodMs = 1000;
static const int kMinBitrateSmoothingPeriodMs = 500;

}  // namespace

InterArrivalSender::InterArrivalSender(const QuicClock* clock,
                                       const RttStats* rtt_stats)
    : clock_(clock),
      rtt_stats_(rtt_stats),
      probing_(true),
      max_segment_size_(kDefaultMaxPacketSize),
      current_bandwidth_(QuicBandwidth::Zero()),
      smoothed_rtt_(QuicTime::Delta::Zero()),
      channel_estimator_(new ChannelEstimator()),
      bitrate_ramp_up_(new InterArrivalBitrateRampUp(clock)),
      overuse_detector_(new InterArrivalOveruseDetector()),
      probe_(new InterArrivalProbe(max_segment_size_)),
      state_machine_(new InterArrivalStateMachine(clock)),
      paced_sender_(new PacedSender(QuicBandwidth::FromKBytesPerSecond(
          kProbeBitrateKBytesPerSecond), max_segment_size_)),
      bandwidth_usage_state_(kBandwidthSteady),
      back_down_time_(QuicTime::Zero()),
      back_down_bandwidth_(QuicBandwidth::Zero()),
      back_down_congestion_delay_(QuicTime::Delta::Zero()) {
}

InterArrivalSender::~InterArrivalSender() {
  STLDeleteValues(&packet_history_map_);
}

void InterArrivalSender::SetFromConfig(const QuicConfig& config,
                                       bool is_server) {
}

// TODO(pwestin): this is really inefficient (4% CPU on the GFE loadtest).
// static
QuicBandwidth InterArrivalSender::CalculateSentBandwidth(
    QuicTime feedback_receive_time) const {
  const QuicTime::Delta kBitrateSmoothingPeriod =
      QuicTime::Delta::FromMilliseconds(kBitrateSmoothingPeriodMs);
  const QuicTime::Delta kMinBitrateSmoothingPeriod =
      QuicTime::Delta::FromMilliseconds(kMinBitrateSmoothingPeriodMs);

  QuicByteCount sum_bytes_sent = 0;

  // Sum packet from new until they are kBitrateSmoothingPeriod old.
  SentPacketsMap::const_reverse_iterator history_rit =
      packet_history_map_.rbegin();

  QuicTime::Delta max_diff = QuicTime::Delta::Zero();
  for (; history_rit != packet_history_map_.rend(); ++history_rit) {
    QuicTime::Delta diff =
        feedback_receive_time.Subtract(history_rit->second->send_timestamp());
    if (diff > kBitrateSmoothingPeriod) {
      break;
    }
    sum_bytes_sent += history_rit->second->bytes_sent();
    max_diff = diff;
  }
  if (max_diff < kMinBitrateSmoothingPeriod) {
    // No estimate.
    return QuicBandwidth::Zero();
  }
  return QuicBandwidth::FromBytesAndTimeDelta(sum_bytes_sent, max_diff);
}

void InterArrivalSender::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& feedback,
    QuicTime feedback_receive_time) {
  DCHECK(feedback.type == kInterArrival);

  if (feedback.type != kInterArrival) {
    return;
  }

  QuicBandwidth sent_bandwidth = CalculateSentBandwidth(feedback_receive_time);

  TimeMap::const_iterator received_it;
  for (received_it = feedback.inter_arrival.received_packet_times.begin();
      received_it != feedback.inter_arrival.received_packet_times.end();
      ++received_it) {
    QuicPacketSequenceNumber sequence_number = received_it->first;

    SentPacketsMap::const_iterator sent_it =
        packet_history_map_.find(sequence_number);
    if (sent_it == packet_history_map_.end()) {
      // Too old data; ignore and move forward.
      DVLOG(1) << "Too old feedback move forward, sequence_number:"
               << sequence_number;
      continue;
    }
    QuicTime time_received = received_it->second;
    QuicTime time_sent = sent_it->second->send_timestamp();
    QuicByteCount bytes_sent = sent_it->second->bytes_sent();

    channel_estimator_->OnAcknowledgedPacket(
        sequence_number, bytes_sent, time_sent, time_received);
    if (probing_) {
      probe_->OnIncomingFeedback(
          sequence_number, bytes_sent, time_sent, time_received);
    } else {
      bool last_of_send_time = false;
      SentPacketsMap::const_iterator next_sent_it = ++sent_it;
      if (next_sent_it == packet_history_map_.end()) {
        // No more sent packets; hence this must be the last.
        last_of_send_time = true;
      } else {
        if (time_sent != next_sent_it->second->send_timestamp()) {
          // Next sent packet have a different send time.
          last_of_send_time = true;
        }
      }
      overuse_detector_->OnAcknowledgedPacket(
          sequence_number, time_sent, last_of_send_time, time_received);
    }
  }
  if (probing_) {
    probing_ = ProbingPhase(feedback_receive_time);
    return;
  }

  InterArrivalState state = state_machine_->GetInterArrivalState();

  if (state == kInterArrivalStatePacketLoss ||
      state == kInterArrivalStateCompetingTcpFLow) {
    EstimateNewBandwidth(feedback_receive_time, sent_bandwidth);
    return;
  }
  EstimateDelayBandwidth(feedback_receive_time, sent_bandwidth);
}

bool InterArrivalSender::ProbingPhase(QuicTime feedback_receive_time) {
  QuicBandwidth available_channel_estimate = QuicBandwidth::Zero();
  if (!probe_->GetEstimate(&available_channel_estimate)) {
    // Continue probing phase.
    return true;
  }
  QuicBandwidth channel_estimate = QuicBandwidth::Zero();
  ChannelEstimateState channel_estimator_state =
      channel_estimator_->GetChannelEstimate(&channel_estimate);

  QuicBandwidth new_rate =
      available_channel_estimate.Scale(kUncertainSafetyMargin);

  switch (channel_estimator_state) {
    case kChannelEstimateUnknown:
      channel_estimate = available_channel_estimate;
      break;
    case kChannelEstimateUncertain:
      channel_estimate = channel_estimate.Scale(kUncertainSafetyMargin);
      break;
    case kChannelEstimateGood:
      // Do nothing.
      break;
  }
  new_rate = max(new_rate,
                 QuicBandwidth::FromKBitsPerSecond(kMinBitrateKbit));

  bitrate_ramp_up_->Reset(new_rate, available_channel_estimate,
                          channel_estimate);

  current_bandwidth_ = new_rate;
  paced_sender_->UpdateBandwidthEstimate(feedback_receive_time, new_rate);
  DVLOG(1) << "Probe result; new rate:"
           << new_rate.ToKBitsPerSecond() << " Kbits/s "
           << " available estimate:"
           << available_channel_estimate.ToKBitsPerSecond() << " Kbits/s "
           << " channel estimate:"
           << channel_estimate.ToKBitsPerSecond() << " Kbits/s ";
  return false;
}

void InterArrivalSender::OnPacketAcked(
    QuicPacketSequenceNumber /*acked_sequence_number*/,
    QuicByteCount acked_bytes) {
  if (probing_) {
    probe_->OnAcknowledgedPacket(acked_bytes);
  }
}

void InterArrivalSender::OnPacketLost(
    QuicPacketSequenceNumber /*sequence_number*/,
    QuicTime ack_receive_time) {
  // Packet loss was reported.
  if (!probing_) {
    if (!state_machine_->PacketLossEvent()) {
      // Less than one RTT since last PacketLossEvent.
      return;
    }
    // Calculate new pace rate.
    EstimateBandwidthAfterLossEvent(ack_receive_time);
  }
}

bool InterArrivalSender::OnPacketSent(
    QuicTime sent_time,
    QuicPacketSequenceNumber sequence_number,
    QuicByteCount bytes,
    HasRetransmittableData /*has_retransmittable_data*/) {
  if (probing_) {
    probe_->OnPacketSent(bytes);
  }
  paced_sender_->OnPacketSent(sent_time, bytes);

  packet_history_map_[sequence_number] = new SentPacket(bytes, sent_time);
  CleanupPacketHistory();
  return true;
}

void InterArrivalSender::OnRetransmissionTimeout(
    bool /*packets_retransmitted*/) {
  // TODO(ianswett): Decrease the available bandwidth.
  if (probing_) {
    probe_->OnRetransmissionTimeout();
  }
}

void InterArrivalSender::OnPacketAbandoned(
    QuicPacketSequenceNumber /*sequence_number*/,
    QuicByteCount abandoned_bytes) {
  // TODO(pwestin): use for out outer_congestion_window_ logic.
  if (probing_) {
    probe_->OnAcknowledgedPacket(abandoned_bytes);
  }
}

QuicTime::Delta InterArrivalSender::TimeUntilSend(
    QuicTime now,
    HasRetransmittableData has_retransmittable_data) {
  // TODO(pwestin): implement outer_congestion_window_ logic.
  QuicTime::Delta outer_window = QuicTime::Delta::Zero();

  if (probing_) {
    if (has_retransmittable_data == HAS_RETRANSMITTABLE_DATA &&
        probe_->GetAvailableCongestionWindow() == 0) {
      outer_window = QuicTime::Delta::Infinite();
    }
  }
  return paced_sender_->TimeUntilSend(now, outer_window);
}

void InterArrivalSender::EstimateDelayBandwidth(QuicTime feedback_receive_time,
                                                QuicBandwidth sent_bandwidth) {
  QuicTime::Delta estimated_congestion_delay = QuicTime::Delta::Zero();
  BandwidthUsage new_bandwidth_usage_state =
      overuse_detector_->GetState(&estimated_congestion_delay);

  switch (new_bandwidth_usage_state) {
    case kBandwidthDraining:
    case kBandwidthUnderUsing:
      // Hold our current bitrate.
      break;
    case kBandwidthOverUsing:
      if (!state_machine_->IncreasingDelayEvent()) {
        // Less than one RTT since last IncreasingDelayEvent.
        return;
      }
      EstimateBandwidthAfterDelayEvent(feedback_receive_time,
                                       estimated_congestion_delay);
      break;
    case kBandwidthSteady:
      // Calculate new pace rate.
      if (bandwidth_usage_state_ == kBandwidthDraining ||
          bandwidth_usage_state_ == kBandwidthOverUsing) {
        EstimateNewBandwidthAfterDraining(feedback_receive_time,
                                          estimated_congestion_delay);
      } else {
        EstimateNewBandwidth(feedback_receive_time, sent_bandwidth);
      }
      break;
  }
  bandwidth_usage_state_ = new_bandwidth_usage_state;
}

QuicBandwidth InterArrivalSender::BandwidthEstimate() const {
  return current_bandwidth_;
}

void InterArrivalSender::UpdateRtt(QuicTime::Delta rtt) {
  state_machine_->set_rtt(rtt_stats_->SmoothedRtt());
}

QuicTime::Delta InterArrivalSender::RetransmissionDelay() const {
  // TODO(pwestin): Calculate and return retransmission delay.
  // Use 2 * the smoothed RTT for now.
  return rtt_stats_->SmoothedRtt().Multiply(2);
}

QuicByteCount InterArrivalSender::GetCongestionWindow() const {
  // Return an estimate based on the bandwidth and rtt.
  return BandwidthEstimate().ToBytesPerPeriod(rtt_stats_->SmoothedRtt());
}

void InterArrivalSender::EstimateNewBandwidth(QuicTime feedback_receive_time,
                                              QuicBandwidth sent_bandwidth) {
  QuicBandwidth new_bandwidth = bitrate_ramp_up_->GetNewBitrate(sent_bandwidth);
  if (current_bandwidth_ == new_bandwidth) {
    return;
  }
  current_bandwidth_ = new_bandwidth;
  state_machine_->IncreaseBitrateDecision();

  QuicBandwidth channel_estimate = QuicBandwidth::Zero();
  ChannelEstimateState channel_estimator_state =
      channel_estimator_->GetChannelEstimate(&channel_estimate);

  if (channel_estimator_state == kChannelEstimateGood) {
    bitrate_ramp_up_->UpdateChannelEstimate(channel_estimate);
  }
  paced_sender_->UpdateBandwidthEstimate(feedback_receive_time,
                                         current_bandwidth_);
  DVLOG(1) << "New bandwidth estimate in steady state:"
           << current_bandwidth_.ToKBitsPerSecond()
           << " Kbits/s";
}

// Did we drain the network buffers in our expected pace?
void InterArrivalSender::EstimateNewBandwidthAfterDraining(
    QuicTime feedback_receive_time,
    QuicTime::Delta estimated_congestion_delay) {
  if (current_bandwidth_ > back_down_bandwidth_) {
    // Do nothing, our current bandwidth is higher than our bandwidth at the
    // previous back down.
    DVLOG(1) << "Current bandwidth estimate is higher than before draining";
    return;
  }
  if (estimated_congestion_delay >= back_down_congestion_delay_) {
    // Do nothing, our estimated delay have increased.
    DVLOG(1) << "Current delay estimate is higher than before draining";
    return;
  }
  DCHECK(back_down_time_.IsInitialized());
  QuicTime::Delta buffer_reduction =
      back_down_congestion_delay_.Subtract(estimated_congestion_delay);
  QuicTime::Delta elapsed_time =
      feedback_receive_time.Subtract(back_down_time_).Subtract(
          rtt_stats_->SmoothedRtt());

  QuicBandwidth new_estimate = QuicBandwidth::Zero();
  if (buffer_reduction >= elapsed_time) {
    // We have drained more than the elapsed time... go back to our old rate.
    new_estimate = back_down_bandwidth_;
  } else {
    float fraction_of_rate =
        static_cast<float>(buffer_reduction.ToMicroseconds()) /
            elapsed_time.ToMicroseconds();  // < 1.0

    QuicBandwidth draining_rate = back_down_bandwidth_.Scale(fraction_of_rate);
    QuicBandwidth max_estimated_draining_rate =
        back_down_bandwidth_.Subtract(current_bandwidth_);
    if (draining_rate > max_estimated_draining_rate) {
      // We drained faster than our old send rate, go back to our old rate.
      new_estimate = back_down_bandwidth_;
    } else {
      // Use our drain rate and our kMinBitrateReduction to go to our
      // new estimate.
      new_estimate = max(current_bandwidth_,
                         current_bandwidth_.Add(draining_rate).Scale(
                             1.0f - kMinBitrateReduction));
      DVLOG(1) << "Draining calculation; current rate:"
               << current_bandwidth_.ToKBitsPerSecond() << " Kbits/s "
               << "draining rate:"
               << draining_rate.ToKBitsPerSecond() << " Kbits/s "
               << "new estimate:"
               << new_estimate.ToKBitsPerSecond() << " Kbits/s "
               << " buffer reduction:"
               << buffer_reduction.ToMicroseconds() << " us "
               << " elapsed time:"
               << elapsed_time.ToMicroseconds()  << " us ";
    }
  }
  if (new_estimate == current_bandwidth_) {
    return;
  }

  QuicBandwidth channel_estimate = QuicBandwidth::Zero();
  ChannelEstimateState channel_estimator_state =
      channel_estimator_->GetChannelEstimate(&channel_estimate);

  // TODO(pwestin): we need to analyze channel_estimate too.
  switch (channel_estimator_state) {
    case kChannelEstimateUnknown:
      channel_estimate = current_bandwidth_;
      break;
    case kChannelEstimateUncertain:
      channel_estimate = channel_estimate.Scale(kUncertainSafetyMargin);
      break;
    case kChannelEstimateGood:
      // Do nothing, estimate is accurate.
      break;
  }
  bitrate_ramp_up_->Reset(new_estimate, back_down_bandwidth_, channel_estimate);
  state_machine_->IncreaseBitrateDecision();
  paced_sender_->UpdateBandwidthEstimate(feedback_receive_time, new_estimate);
  current_bandwidth_ = new_estimate;
  DVLOG(1) << "New bandwidth estimate after draining:"
           << new_estimate.ToKBitsPerSecond() << " Kbits/s";
}

void InterArrivalSender::EstimateBandwidthAfterDelayEvent(
    QuicTime feedback_receive_time,
    QuicTime::Delta estimated_congestion_delay) {
  QuicByteCount estimated_byte_buildup =
      current_bandwidth_.ToBytesPerPeriod(estimated_congestion_delay);

  // To drain all build up buffer within one RTT we need to reduce the
  // bitrate with the following.
  // TODO(pwestin): this is a crude first implementation.
  int64 draining_rate_per_rtt = (estimated_byte_buildup *
      kNumMicrosPerSecond) / rtt_stats_->SmoothedRtt().ToMicroseconds();

  float decrease_factor =
      draining_rate_per_rtt / current_bandwidth_.ToBytesPerSecond();

  decrease_factor = max(decrease_factor, kMinBitrateReduction);
  decrease_factor = min(decrease_factor, kMaxBitrateReduction);
  back_down_congestion_delay_ = estimated_congestion_delay;
  QuicBandwidth new_target_bitrate =
      current_bandwidth_.Scale(1.0f - decrease_factor);

  // While in delay sensing mode send at least one packet per RTT.
  QuicBandwidth min_delay_bitrate =
      QuicBandwidth::FromBytesAndTimeDelta(max_segment_size_,
                                           rtt_stats_->SmoothedRtt());
  new_target_bitrate = max(new_target_bitrate, min_delay_bitrate);

  ResetCurrentBandwidth(feedback_receive_time, new_target_bitrate);

  DVLOG(1) << "New bandwidth estimate after delay event:"
      << current_bandwidth_.ToKBitsPerSecond() << " Kbits/s min delay bitrate:"
      << min_delay_bitrate.ToKBitsPerSecond() << " Kbits/s RTT:"
      << rtt_stats_->SmoothedRtt().ToMicroseconds() << " us";
}

void InterArrivalSender::EstimateBandwidthAfterLossEvent(
    QuicTime feedback_receive_time) {
  ResetCurrentBandwidth(feedback_receive_time,
                        current_bandwidth_.Scale(kPacketLossBitrateReduction));
  DVLOG(1) << "New bandwidth estimate after loss event:"
           << current_bandwidth_.ToKBitsPerSecond() << " Kbits/s";
}

void InterArrivalSender::ResetCurrentBandwidth(QuicTime feedback_receive_time,
                                               QuicBandwidth new_rate) {
  new_rate = max(new_rate,
                 QuicBandwidth::FromKBitsPerSecond(kMinBitrateKbit));
  QuicBandwidth channel_estimate = QuicBandwidth::Zero();
  ChannelEstimateState channel_estimator_state =
      channel_estimator_->GetChannelEstimate(&channel_estimate);

  switch (channel_estimator_state) {
    case kChannelEstimateUnknown:
      channel_estimate = current_bandwidth_;
      break;
    case kChannelEstimateUncertain:
      channel_estimate = channel_estimate.Scale(kUncertainSafetyMargin);
      break;
    case kChannelEstimateGood:
      // Do nothing.
      break;
  }
  back_down_time_ = feedback_receive_time;
  back_down_bandwidth_ = current_bandwidth_;
  bitrate_ramp_up_->Reset(new_rate, current_bandwidth_, channel_estimate);
  if (new_rate != current_bandwidth_) {
    current_bandwidth_ = new_rate;
    paced_sender_->UpdateBandwidthEstimate(feedback_receive_time,
                                           current_bandwidth_);
    state_machine_->DecreaseBitrateDecision();
  }
}

void InterArrivalSender::CleanupPacketHistory() {
  const QuicTime::Delta kHistoryPeriod =
      QuicTime::Delta::FromMilliseconds(kHistoryPeriodMs);
  QuicTime now = clock_->ApproximateNow();

  SentPacketsMap::iterator history_it = packet_history_map_.begin();
  for (; history_it != packet_history_map_.end(); ++history_it) {
    if (now.Subtract(history_it->second->send_timestamp()) <= kHistoryPeriod) {
      return;
    }
    delete history_it->second;
    packet_history_map_.erase(history_it);
    history_it = packet_history_map_.begin();
  }
}

}  // namespace net
