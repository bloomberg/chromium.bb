// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purpose of this file is determine what bitrate to use for mirroring.
// Ideally this should be as much as possible, without causing any frames to
// arrive late.

// The current algorithm is to measure how much bandwidth we've been using
// recently. We also keep track of how much data has been queued up for sending
// in a virtual "buffer" (this virtual buffer represents all the buffers between
// the sender and the receiver, including retransmissions and so forth.)
// If we estimate that our virtual buffer is mostly empty, we try to use
// more bandwidth than our recent usage, otherwise we use less.

#include "media/cast/sender/congestion_control.h"

#include "base/logging.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"

namespace media {
namespace cast {

class AdaptiveCongestionControl : public CongestionControl {
 public:
  AdaptiveCongestionControl(base::TickClock* clock,
                            uint32 max_bitrate_configured,
                            uint32 min_bitrate_configured,
                            size_t max_unacked_frames);

  virtual ~AdaptiveCongestionControl() OVERRIDE;

  virtual void UpdateRtt(base::TimeDelta rtt) OVERRIDE;

  // Called when an encoded frame is sent to the transport.
  virtual void SendFrameToTransport(uint32 frame_id,
                                    size_t frame_size,
                                    base::TimeTicks when) OVERRIDE;

  // Called when we receive an ACK for a frame.
  virtual void AckFrame(uint32 frame_id, base::TimeTicks when) OVERRIDE;

  // Returns the bitrate we should use for the next frame.
  virtual uint32 GetBitrate(base::TimeTicks playout_time,
                            base::TimeDelta playout_delay) OVERRIDE;

 private:
  struct FrameStats {
    FrameStats();
    // Time this frame was sent to the transport.
    base::TimeTicks sent_time;
    // Time this frame was acked.
    base::TimeTicks ack_time;
    // Size of encoded frame in bits.
    size_t frame_size;
  };

  // Calculate how much "dead air" (idle time) there is between two frames.
  static base::TimeDelta DeadTime(const FrameStats& a, const FrameStats& b);
  // Get the FrameStats for a given |frame_id|.
  // Note: Older FrameStats will be removed automatically.
  FrameStats* GetFrameStats(uint32 frame_id);
  // Calculate a safe bitrate. This is based on how much we've been
  // sending in the past.
  double CalculateSafeBitrate();

  // For a given frame, calculate when it might be acked.
  // (Or return the time it was acked, if it was.)
  base::TimeTicks EstimatedAckTime(uint32 frame_id, double bitrate);
  // Calculate when we start sending the data for a given frame.
  // This is done by calculating when we were done sending the previous
  // frame, but obviously can't be less than |sent_time| (if known).
  base::TimeTicks EstimatedSendingTime(uint32 frame_id, double bitrate);

  base::TickClock* const clock_;  // Not owned by this class.
  const uint32 max_bitrate_configured_;
  const uint32 min_bitrate_configured_;
  std::deque<FrameStats> frame_stats_;
  uint32 last_frame_stats_;
  uint32 last_acked_frame_;
  uint32 last_encoded_frame_;
  base::TimeDelta rtt_;
  size_t history_size_;
  size_t acked_bits_in_history_;
  base::TimeDelta dead_time_in_history_;

  DISALLOW_COPY_AND_ASSIGN(AdaptiveCongestionControl);
};

class FixedCongestionControl : public CongestionControl {
 public:
  FixedCongestionControl(uint32 bitrate) : bitrate_(bitrate) {}
  virtual ~FixedCongestionControl() OVERRIDE {}

  virtual void UpdateRtt(base::TimeDelta rtt) OVERRIDE {
  }

  // Called when an encoded frame is sent to the transport.
  virtual void SendFrameToTransport(uint32 frame_id,
                                    size_t frame_size,
                                    base::TimeTicks when) OVERRIDE {
  }

  // Called when we receive an ACK for a frame.
  virtual void AckFrame(uint32 frame_id, base::TimeTicks when) OVERRIDE {
  }

  // Returns the bitrate we should use for the next frame.
  virtual uint32 GetBitrate(base::TimeTicks playout_time,
                            base::TimeDelta playout_delay) OVERRIDE {
    return bitrate_;
  }

 private:
  uint32 bitrate_;
  DISALLOW_COPY_AND_ASSIGN(FixedCongestionControl);
};


CongestionControl* NewAdaptiveCongestionControl(
    base::TickClock* clock,
    uint32 max_bitrate_configured,
    uint32 min_bitrate_configured,
    size_t max_unacked_frames) {
  return new AdaptiveCongestionControl(clock,
                                       max_bitrate_configured,
                                       min_bitrate_configured,
                                       max_unacked_frames);
}

CongestionControl* NewFixedCongestionControl(uint32 bitrate) {
  return new FixedCongestionControl(bitrate);
}

// This means that we *try* to keep our buffer 90% empty.
// If it is less full, we increase the bandwidth, if it is more
// we decrease the bandwidth. Making this smaller makes the
// congestion control more aggressive.
static const double kTargetEmptyBufferFraction = 0.9;

// This is the size of our history in frames. Larger values makes the
// congestion control adapt slower.
static const size_t kHistorySize = 100;

AdaptiveCongestionControl::FrameStats::FrameStats() : frame_size(0) {
}

AdaptiveCongestionControl::AdaptiveCongestionControl(
    base::TickClock* clock,
    uint32 max_bitrate_configured,
    uint32 min_bitrate_configured,
    size_t max_unacked_frames)
    : clock_(clock),
      max_bitrate_configured_(max_bitrate_configured),
      min_bitrate_configured_(min_bitrate_configured),
      last_frame_stats_(static_cast<uint32>(-1)),
      last_acked_frame_(static_cast<uint32>(-1)),
      last_encoded_frame_(static_cast<uint32>(-1)),
      history_size_(max_unacked_frames + kHistorySize),
      acked_bits_in_history_(0) {
  DCHECK_GE(max_bitrate_configured, min_bitrate_configured) << "Invalid config";
  frame_stats_.resize(2);
  base::TimeTicks now = clock->NowTicks();
  frame_stats_[0].ack_time = now;
  frame_stats_[0].sent_time = now;
  frame_stats_[1].ack_time = now;
  DCHECK(!frame_stats_[0].ack_time.is_null());
}

CongestionControl::~CongestionControl() {}
AdaptiveCongestionControl::~AdaptiveCongestionControl() {}

void AdaptiveCongestionControl::UpdateRtt(base::TimeDelta rtt) {
  rtt_ = (7 * rtt_ + rtt) / 8;
}

// Calculate how much "dead air" there is between two frames.
base::TimeDelta AdaptiveCongestionControl::DeadTime(const FrameStats& a,
                                                    const FrameStats& b) {
  if (b.sent_time > a.ack_time) {
    return b.sent_time - a.ack_time;
  } else {
    return base::TimeDelta();
  }
}

double AdaptiveCongestionControl::CalculateSafeBitrate() {
  double transmit_time =
      (GetFrameStats(last_acked_frame_)->ack_time -
       frame_stats_.front().sent_time - dead_time_in_history_).InSecondsF();

  if (acked_bits_in_history_ == 0 || transmit_time <= 0.0) {
    return min_bitrate_configured_;
  }
  return acked_bits_in_history_ / std::max(transmit_time, 1E-3);
}

AdaptiveCongestionControl::FrameStats*
AdaptiveCongestionControl::GetFrameStats(uint32 frame_id) {
  int32 offset = static_cast<int32>(frame_id - last_frame_stats_);
  DCHECK_LT(offset, static_cast<int32>(kHistorySize));
  if (offset > 0) {
    frame_stats_.resize(frame_stats_.size() + offset);
    last_frame_stats_ += offset;
    offset = 0;
  }
  while (frame_stats_.size() > history_size_) {
    DCHECK_GT(frame_stats_.size(), 1UL);
    DCHECK(!frame_stats_[0].ack_time.is_null());
    acked_bits_in_history_ -= frame_stats_[0].frame_size;
    dead_time_in_history_ -= DeadTime(frame_stats_[0], frame_stats_[1]);
    DCHECK_GE(acked_bits_in_history_, 0UL);
    VLOG(2) << "DT: " << dead_time_in_history_.InSecondsF();
    DCHECK_GE(dead_time_in_history_.InSecondsF(), 0.0);
    frame_stats_.pop_front();
  }
  offset += frame_stats_.size() - 1;
  if (offset < 0 || offset >= static_cast<int32>(frame_stats_.size())) {
    return NULL;
  }
  return &frame_stats_[offset];
}

void AdaptiveCongestionControl::AckFrame(uint32 frame_id,
                                         base::TimeTicks when) {
  FrameStats* frame_stats = GetFrameStats(last_acked_frame_);
  while (IsNewerFrameId(frame_id, last_acked_frame_)) {
    FrameStats* last_frame_stats = frame_stats;
    frame_stats = GetFrameStats(last_acked_frame_ + 1);
    DCHECK(frame_stats);
    if (frame_stats->sent_time.is_null()) {
      // Can't ack a frame that hasn't been sent yet.
      return;
    }
    last_acked_frame_++;
    if (when < frame_stats->sent_time)
      when = frame_stats->sent_time;

    frame_stats->ack_time = when;
    acked_bits_in_history_ += frame_stats->frame_size;
    dead_time_in_history_ += DeadTime(*last_frame_stats, *frame_stats);
  }
}

void AdaptiveCongestionControl::SendFrameToTransport(uint32 frame_id,
                                                     size_t frame_size,
                                                     base::TimeTicks when) {
  last_encoded_frame_ = frame_id;
  FrameStats* frame_stats = GetFrameStats(frame_id);
  DCHECK(frame_stats);
  frame_stats->frame_size = frame_size;
  frame_stats->sent_time = when;
}

base::TimeTicks AdaptiveCongestionControl::EstimatedAckTime(uint32 frame_id,
                                                            double bitrate) {
  FrameStats* frame_stats = GetFrameStats(frame_id);
  DCHECK(frame_stats);
  if (frame_stats->ack_time.is_null()) {
    DCHECK(frame_stats->frame_size) << "frame_id: " << frame_id;
    base::TimeTicks ret = EstimatedSendingTime(frame_id, bitrate);
    ret += base::TimeDelta::FromSecondsD(frame_stats->frame_size / bitrate);
    ret += rtt_;
    base::TimeTicks now = clock_->NowTicks();
    if (ret < now) {
      // This is a little counter-intuitive, but it seems to work.
      // Basically, when we estimate that the ACK should have already happened,
      // we figure out how long ago it should have happened and guess that the
      // ACK will happen half of that time in the future. This will cause some
      // over-estimation when acks are late, which is actually what we want.
      return now + (now - ret) / 2;
    } else {
      return ret;
    }
  } else {
    return frame_stats->ack_time;
  }
}

base::TimeTicks AdaptiveCongestionControl::EstimatedSendingTime(
    uint32 frame_id,
    double bitrate) {
  FrameStats* frame_stats = GetFrameStats(frame_id);
  DCHECK(frame_stats);
  base::TimeTicks ret = EstimatedAckTime(frame_id - 1, bitrate) - rtt_;
  if (frame_stats->sent_time.is_null()) {
    // Not sent yet, but we can't start sending it in the past.
    return std::max(ret, clock_->NowTicks());
  } else {
    return std::max(ret, frame_stats->sent_time);
  }
}

uint32 AdaptiveCongestionControl::GetBitrate(base::TimeTicks playout_time,
                                             base::TimeDelta playout_delay) {
  double safe_bitrate = CalculateSafeBitrate();
  // Estimate when we might start sending the next frame.
  base::TimeDelta time_to_catch_up =
      playout_time -
      EstimatedSendingTime(last_encoded_frame_ + 1, safe_bitrate);

  double empty_buffer_fraction =
      time_to_catch_up.InSecondsF() / playout_delay.InSecondsF();
  empty_buffer_fraction = std::min(empty_buffer_fraction, 1.0);
  empty_buffer_fraction = std::max(empty_buffer_fraction, 0.0);

  uint32 bits_per_second = static_cast<uint32>(
      safe_bitrate * empty_buffer_fraction / kTargetEmptyBufferFraction);
  VLOG(3) << " FBR:" << (bits_per_second / 1E6)
          << " EBF:" << empty_buffer_fraction
          << " SBR:" << (safe_bitrate / 1E6);
  bits_per_second = std::max(bits_per_second, min_bitrate_configured_);
  bits_per_second = std::min(bits_per_second, max_bitrate_configured_);
  return bits_per_second;
}

}  // namespace cast
}  // namespace media
