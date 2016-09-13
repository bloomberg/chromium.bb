// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_sender.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "media/cast/constants.h"

namespace {

// Global map for looking-up CastRemotingSender instances by their
// |remoting_stream_id_|.
using CastRemotingSenderMap = std::map<int32_t, cast::CastRemotingSender*>;
base::LazyInstance<CastRemotingSenderMap>::Leaky g_sender_map =
    LAZY_INSTANCE_INITIALIZER;

constexpr base::TimeDelta kMinSchedulingDelay =
    base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kMaxAckDelay = base::TimeDelta::FromMilliseconds(800);
constexpr base::TimeDelta kMinAckDelay = base::TimeDelta::FromMilliseconds(400);
constexpr base::TimeDelta kReceiverProcessTime =
    base::TimeDelta::FromMilliseconds(250);

}  // namespace

namespace cast {

class CastRemotingSender::RemotingRtcpClient final
    : public media::cast::RtcpObserver {
 public:
  explicit RemotingRtcpClient(base::WeakPtr<CastRemotingSender> remoting_sender)
      : remoting_sender_(remoting_sender) {}

  ~RemotingRtcpClient() final {}

  void OnReceivedCastMessage(
      const media::cast::RtcpCastMessage& cast_message) final {
    if (remoting_sender_)
      remoting_sender_->OnReceivedCastMessage(cast_message);
  }

  void OnReceivedRtt(base::TimeDelta round_trip_time) final {
    if (remoting_sender_)
      remoting_sender_->OnReceivedRtt(round_trip_time);
  }

  void OnReceivedPli() final {
    // Receiving PLI messages on remoting streams is ignored.
  }

 private:
  const base::WeakPtr<CastRemotingSender> remoting_sender_;

  DISALLOW_COPY_AND_ASSIGN(RemotingRtcpClient);
};

// Convenience macro used in logging statements throughout this file.
#define SENDER_SSRC (is_audio_ ? "AUDIO[" : "VIDEO[") << ssrc_ << "] "

CastRemotingSender::CastRemotingSender(
    scoped_refptr<media::cast::CastEnvironment> cast_environment,
    media::cast::CastTransport* transport,
    const media::cast::CastTransportRtpConfig& config)
    : remoting_stream_id_(config.rtp_stream_id),
      cast_environment_(std::move(cast_environment)),
      transport_(transport),
      ssrc_(config.ssrc),
      is_audio_(config.rtp_payload_type ==
                media::cast::RtpPayloadType::REMOTE_AUDIO),
      max_ack_delay_(kMaxAckDelay),
      last_sent_frame_id_(media::cast::FrameId::first() - 1),
      latest_acked_frame_id_(media::cast::FrameId::first() - 1),
      duplicate_ack_counter_(0),
      last_frame_was_canceled_(false),
      weak_factory_(this) {
  DCHECK(cast_environment_->CurrentlyOn(media::cast::CastEnvironment::MAIN));

  CastRemotingSender*& pointer_in_map = g_sender_map.Get()[remoting_stream_id_];
  DCHECK(!pointer_in_map);
  pointer_in_map = this;
  transport_->InitializeStream(
      config, base::MakeUnique<RemotingRtcpClient>(weak_factory_.GetWeakPtr()));
}

CastRemotingSender::~CastRemotingSender() {
  DCHECK(cast_environment_->CurrentlyOn(media::cast::CastEnvironment::MAIN));

  g_sender_map.Pointer()->erase(remoting_stream_id_);
}

void CastRemotingSender::OnReceivedRtt(base::TimeDelta round_trip_time) {
  DCHECK_GT(round_trip_time, base::TimeDelta());
  current_round_trip_time_ = round_trip_time;
  max_ack_delay_ = 2 * current_round_trip_time_ + kReceiverProcessTime;
  max_ack_delay_ =
      std::max(std::min(max_ack_delay_, kMaxAckDelay), kMinAckDelay);
}

void CastRemotingSender::ResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(media::cast::CastEnvironment::MAIN));

  DCHECK(!last_send_time_.is_null());
  const base::TimeDelta time_since_last_send =
      cast_environment_->Clock()->NowTicks() - last_send_time_;
  if (time_since_last_send > max_ack_delay_) {
    if (latest_acked_frame_id_ == last_sent_frame_id_) {
      // Last frame acked, no point in doing anything.
    } else {
      VLOG(1) << SENDER_SSRC
              << "ACK timeout; last acked frame: " << latest_acked_frame_id_;
      ResendForKickstart();
    }
  }
  ScheduleNextResendCheck();
}

void CastRemotingSender::ScheduleNextResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(media::cast::CastEnvironment::MAIN));

  DCHECK(!last_send_time_.is_null());
  base::TimeDelta time_to_next =
      last_send_time_ - cast_environment_->Clock()->NowTicks() + max_ack_delay_;
  time_to_next = std::max(time_to_next, kMinSchedulingDelay);
  cast_environment_->PostDelayedTask(
      media::cast::CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&CastRemotingSender::ResendCheck, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void CastRemotingSender::ResendForKickstart() {
  DCHECK(cast_environment_->CurrentlyOn(media::cast::CastEnvironment::MAIN));

  DCHECK(!last_send_time_.is_null());
  VLOG(1) << SENDER_SSRC << "Resending last packet of frame "
          << last_sent_frame_id_ << " to kick-start.";
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  transport_->ResendFrameForKickstart(ssrc_, last_sent_frame_id_);
}

// TODO(xjz): We may need to count in the frames acknowledged in
// RtcpCastMessage::received_later_frames for more accurate calculation on
// available bandwidth. Same logic should apply on
// media::cast::FrameSender::GetUnacknowledgedFrameCount().
int CastRemotingSender::NumberOfFramesInFlight() const {
  if (last_send_time_.is_null())
    return 0;
  const int count = last_sent_frame_id_ - latest_acked_frame_id_;
  DCHECK_GE(count, 0);
  return count;
}

void CastRemotingSender::RecordLatestFrameTimestamps(
    media::cast::FrameId frame_id,
    media::cast::RtpTimeTicks rtp_timestamp) {
  frame_rtp_timestamps_[frame_id.lower_8_bits()] = rtp_timestamp;
}

media::cast::RtpTimeTicks CastRemotingSender::GetRecordedRtpTimestamp(
    media::cast::FrameId frame_id) const {
  return frame_rtp_timestamps_[frame_id.lower_8_bits()];
}

void CastRemotingSender::OnReceivedCastMessage(
    const media::cast::RtcpCastMessage& cast_feedback) {
  DCHECK(cast_environment_->CurrentlyOn(media::cast::CastEnvironment::MAIN));

  if (last_send_time_.is_null())
    return;  // Cannot get an ACK without having first sent a frame.

  if (cast_feedback.missing_frames_and_packets.empty() &&
      cast_feedback.received_later_frames.empty()) {
    if (latest_acked_frame_id_ == cast_feedback.ack_frame_id) {
      VLOG(1) << SENDER_SSRC << "Received duplicate ACK for frame "
              << latest_acked_frame_id_;
      TRACE_EVENT_INSTANT2(
          "cast.stream", "Duplicate ACK", TRACE_EVENT_SCOPE_THREAD,
          "ack_frame_id", cast_feedback.ack_frame_id.lower_32_bits(),
          "last_sent_frame_id", last_sent_frame_id_.lower_32_bits());
    }
    // We only count duplicate ACKs when we have sent newer frames.
    if (latest_acked_frame_id_ == cast_feedback.ack_frame_id &&
        latest_acked_frame_id_ != last_sent_frame_id_) {
      duplicate_ack_counter_++;
    } else {
      duplicate_ack_counter_ = 0;
    }
    // TODO(miu): The values "2" and "3" should be derived from configuration.
    if (duplicate_ack_counter_ >= 2 && duplicate_ack_counter_ % 3 == 2) {
      ResendForKickstart();
    }
  } else {
    // Only count duplicated ACKs if there is no NACK request in between.
    // This is to avoid aggressive resend.
    duplicate_ack_counter_ = 0;
  }

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  auto ack_event = base::MakeUnique<media::cast::FrameEvent>();
  ack_event->timestamp = now;
  ack_event->type = media::cast::FRAME_ACK_RECEIVED;
  ack_event->media_type =
      is_audio_ ? media::cast::AUDIO_EVENT : media::cast::VIDEO_EVENT;
  ack_event->rtp_timestamp =
      GetRecordedRtpTimestamp(cast_feedback.ack_frame_id);
  ack_event->frame_id = cast_feedback.ack_frame_id;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_event));

  const bool is_acked_out_of_order =
      cast_feedback.ack_frame_id < latest_acked_frame_id_;
  VLOG(2) << SENDER_SSRC << "Received ACK"
          << (is_acked_out_of_order ? " out-of-order" : "") << " for frame "
          << cast_feedback.ack_frame_id;
  if (is_acked_out_of_order) {
    TRACE_EVENT_INSTANT2(
        "cast.stream", "ACK out of order", TRACE_EVENT_SCOPE_THREAD,
        "ack_frame_id", cast_feedback.ack_frame_id.lower_32_bits(),
        "latest_acked_frame_id", latest_acked_frame_id_.lower_32_bits());
  } else if (latest_acked_frame_id_ < cast_feedback.ack_frame_id) {
    // Cancel resends of acked frames.
    std::vector<media::cast::FrameId> frames_to_cancel;
    frames_to_cancel.reserve(cast_feedback.ack_frame_id -
                             latest_acked_frame_id_);
    do {
      ++latest_acked_frame_id_;
      frames_to_cancel.push_back(latest_acked_frame_id_);
      // This is a good place to match the trace for frame ids
      // since this ensures we not only track frame ids that are
      // implicitly ACKed, but also handles duplicate ACKs
      TRACE_EVENT_ASYNC_END1(
          "cast.stream", is_audio_ ? "Audio Transport" : "Video Transport",
          latest_acked_frame_id_.lower_32_bits(), "RTT_usecs",
          current_round_trip_time_.InMicroseconds());
    } while (latest_acked_frame_id_ < cast_feedback.ack_frame_id);
    transport_->CancelSendingFrames(ssrc_, frames_to_cancel);
  }
}

void CastRemotingSender::SendFrame() {
  DCHECK(cast_environment_->CurrentlyOn(media::cast::CastEnvironment::MAIN));

  if (NumberOfFramesInFlight() >= media::cast::kMaxUnackedFrames) {
    // TODO(xjz): Delay the sending of this frame and stop reading the next
    // frame data.
    return;
  }

  VLOG(2) << SENDER_SSRC
          << "About to send another frame: last_sent=" << last_sent_frame_id_
          << ", latest_acked=" << latest_acked_frame_id_;

  const media::cast::FrameId frame_id = last_sent_frame_id_ + 1;
  const bool is_first_frame_to_be_sent = last_send_time_.is_null();

  base::TimeTicks last_frame_reference_time = last_send_time_;
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  last_sent_frame_id_ = frame_id;
  // If this is the first frame about to be sent, fake the value of
  // |latest_acked_frame_id_| to indicate the receiver starts out all caught up.
  // Also, schedule the periodic frame re-send checks.
  if (is_first_frame_to_be_sent)
    ScheduleNextResendCheck();

  DVLOG(3) << "Sending remoting frame, id = " << frame_id;

  media::cast::EncodedFrame remoting_frame;
  remoting_frame.frame_id = frame_id;
  remoting_frame.dependency =
      (is_first_frame_to_be_sent || last_frame_was_canceled_)
          ? media::cast::EncodedFrame::KEY
          : media::cast::EncodedFrame::DEPENDENT;
  remoting_frame.referenced_frame_id =
      remoting_frame.dependency == media::cast::EncodedFrame::KEY
          ? frame_id
          : frame_id - 1;
  remoting_frame.reference_time = last_send_time_;
  media::cast::RtpTimeTicks last_frame_rtp_timestamp;
  if (is_first_frame_to_be_sent) {
    last_frame_reference_time = remoting_frame.reference_time;
    last_frame_rtp_timestamp =
        media::cast::RtpTimeTicks() - media::cast::RtpTimeDelta::FromTicks(1);
  } else {
    last_frame_rtp_timestamp = GetRecordedRtpTimestamp(frame_id - 1);
  }
  // Ensure each successive frame's RTP timestamp is unique, but otherwise just
  // base it on the reference time.
  remoting_frame.rtp_timestamp =
      last_frame_rtp_timestamp +
      std::max(media::cast::RtpTimeDelta::FromTicks(1),
               media::cast::RtpTimeDelta::FromTimeDelta(
                   remoting_frame.reference_time - last_frame_reference_time,
                   media::cast::kRemotingRtpTimebase));
  remoting_frame.data.swap(next_frame_data_);

  std::unique_ptr<media::cast::FrameEvent> remoting_event(
      new media::cast::FrameEvent());
  remoting_event->timestamp = remoting_frame.reference_time;
  // TODO(xjz): Use a new event type for remoting.
  remoting_event->type = media::cast::FRAME_ENCODED;
  remoting_event->media_type =
      is_audio_ ? media::cast::AUDIO_EVENT : media::cast::VIDEO_EVENT;
  remoting_event->rtp_timestamp = remoting_frame.rtp_timestamp;
  remoting_event->frame_id = frame_id;
  remoting_event->size = remoting_frame.data.length();
  remoting_event->key_frame =
      remoting_frame.dependency == media::cast::EncodedFrame::KEY;
  cast_environment_->logger()->DispatchFrameEvent(std::move(remoting_event));

  RecordLatestFrameTimestamps(frame_id, remoting_frame.rtp_timestamp);
  last_frame_was_canceled_ = false;

  transport_->InsertFrame(ssrc_, remoting_frame);
}

void CastRemotingSender::CancelFramesInFlight() {
  DCHECK(cast_environment_->CurrentlyOn(media::cast::CastEnvironment::MAIN));

  base::STLClearObject(&next_frame_data_);

  if (latest_acked_frame_id_ < last_sent_frame_id_) {
    std::vector<media::cast::FrameId> frames_to_cancel;
    do {
      ++latest_acked_frame_id_;
      frames_to_cancel.push_back(latest_acked_frame_id_);
    } while (latest_acked_frame_id_ < last_sent_frame_id_);
    transport_->CancelSendingFrames(ssrc_, frames_to_cancel);
  }

  last_frame_was_canceled_ = true;
}

}  // namespace cast
