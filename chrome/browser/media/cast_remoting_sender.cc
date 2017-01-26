// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_sender.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "media/cast/constants.h"

using content::BrowserThread;

namespace {

// Global map for looking-up CastRemotingSender instances by their
// |rtp_stream_id|.
using CastRemotingSenderMap = std::map<int32_t, cast::CastRemotingSender*>;
base::LazyInstance<CastRemotingSenderMap>::Leaky g_sender_map =
    LAZY_INSTANCE_INITIALIZER;

constexpr base::TimeDelta kMinSchedulingDelay =
    base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kMaxAckDelay = base::TimeDelta::FromMilliseconds(800);
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
    media::cast::CastTransport* transport,
    const media::cast::CastTransportRtpConfig& config,
    base::TimeDelta logging_flush_interval,
    const FrameEventCallback& cb)
    : rtp_stream_id_(config.rtp_stream_id),
      transport_(transport),
      ssrc_(config.ssrc),
      is_audio_(config.rtp_payload_type ==
                media::cast::RtpPayloadType::REMOTE_AUDIO),
      logging_flush_interval_(logging_flush_interval),
      frame_event_cb_(cb),
      clock_(new base::DefaultTickClock()),
      binding_(this),
      max_ack_delay_(kMaxAckDelay),
      last_sent_frame_id_(media::cast::FrameId::first() - 1),
      latest_acked_frame_id_(media::cast::FrameId::first() - 1),
      duplicate_ack_counter_(0),
      input_queue_discards_remaining_(0),
      pipe_watcher_(FROM_HERE),
      flow_restart_pending_(true),
      weak_factory_(this) {
  // Confirm this constructor is running on the IO BrowserThread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CastRemotingSender*& pointer_in_map = g_sender_map.Get()[rtp_stream_id_];
  DCHECK(!pointer_in_map);
  pointer_in_map = this;

  transport_->InitializeStream(
      config, base::MakeUnique<RemotingRtcpClient>(weak_factory_.GetWeakPtr()));

  if (!frame_event_cb_.is_null())
    DCHECK(logging_flush_interval_ > base::TimeDelta());

  if (!frame_event_cb_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&CastRemotingSender::SendFrameEvents,
                              weak_factory_.GetWeakPtr()),
        logging_flush_interval_);
  }
}

CastRemotingSender::~CastRemotingSender() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  g_sender_map.Pointer()->erase(rtp_stream_id_);
}

// static
void CastRemotingSender::FindAndBind(
    int32_t rtp_stream_id,
    mojo::ScopedDataPipeConsumerHandle pipe,
    media::mojom::RemotingDataStreamSenderRequest request,
    const base::Closure& error_callback) {
  // CastRemotingSender lives entirely on the IO thread, so trampoline if
  // necessary.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&CastRemotingSender::FindAndBind, rtp_stream_id,
                   base::Passed(&pipe), base::Passed(&request),
                   // Using media::BindToCurrentLoop() so the |error_callback|
                   // is trampolined back to the original thread.
                   media::BindToCurrentLoop(error_callback)));
    return;
  }

  DCHECK(!error_callback.is_null());

  // Look-up the CastRemotingSender instance by its |rtp_stream_id|.
  const auto it = g_sender_map.Pointer()->find(rtp_stream_id);
  if (it == g_sender_map.Pointer()->end()) {
    DLOG(ERROR) << "Cannot find CastRemotingSender instance by ID: "
                << rtp_stream_id;
    error_callback.Run();
    return;
  }
  CastRemotingSender* const sender = it->second;

  // Confirm that the CastRemotingSender isn't already bound to a message pipe.
  if (sender->binding_.is_bound()) {
    DLOG(ERROR) << "Attempt to bind to CastRemotingSender a second time (id="
                << rtp_stream_id << ")!";
    error_callback.Run();
    return;
  }

  DCHECK(sender->error_callback_.is_null());
  sender->error_callback_ = error_callback;

  sender->pipe_ = std::move(pipe);
  sender->pipe_watcher_.Start(
      sender->pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::Bind(&CastRemotingSender::ProcessInputQueue,
                 base::Unretained(sender)));
  sender->binding_.Bind(std::move(request));
  sender->binding_.set_connection_error_handler(sender->error_callback_);
}

void CastRemotingSender::OnReceivedRtt(base::TimeDelta round_trip_time) {
  DCHECK_GT(round_trip_time, base::TimeDelta());
  current_round_trip_time_ = round_trip_time;
  max_ack_delay_ = 2 * std::max(current_round_trip_time_, base::TimeDelta()) +
      kReceiverProcessTime;
  max_ack_delay_ = std::min(max_ack_delay_, kMaxAckDelay);
}

void CastRemotingSender::ResendCheck() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!last_send_time_.is_null());
  const base::TimeDelta time_since_last_send =
      clock_->NowTicks() - last_send_time_;
  if (time_since_last_send > max_ack_delay_) {
    if (latest_acked_frame_id_ == last_sent_frame_id_) {
      // Last frame acked, no point in doing anything.
    } else {
      VLOG(2) << SENDER_SSRC
              << "ACK timeout; last acked frame: " << latest_acked_frame_id_;
      ResendForKickstart();
    }
  }
  ScheduleNextResendCheck();
}

void CastRemotingSender::ScheduleNextResendCheck() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!last_send_time_.is_null());
  base::TimeDelta time_to_next =
      last_send_time_ - clock_->NowTicks() + max_ack_delay_;
  time_to_next = std::max(time_to_next, kMinSchedulingDelay);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CastRemotingSender::ResendCheck, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void CastRemotingSender::ResendForKickstart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!last_send_time_.is_null());
  VLOG(2) << SENDER_SSRC << "Resending last packet of frame "
          << last_sent_frame_id_ << " to kick-start.";
  last_send_time_ = clock_->NowTicks();
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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (last_send_time_.is_null())
    return;  // Cannot get an ACK without having first sent a frame.

  if (cast_feedback.missing_frames_and_packets.empty() &&
      cast_feedback.received_later_frames.empty()) {
    if (latest_acked_frame_id_ == cast_feedback.ack_frame_id) {
      VLOG(2) << SENDER_SSRC << "Received duplicate ACK for frame "
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

  if (!frame_event_cb_.is_null()) {
    base::TimeTicks now = clock_->NowTicks();
    media::cast::FrameEvent ack_event;
    ack_event.timestamp = now;
    ack_event.type = media::cast::FRAME_ACK_RECEIVED;
    ack_event.media_type =
        is_audio_ ? media::cast::AUDIO_EVENT : media::cast::VIDEO_EVENT;
    ack_event.rtp_timestamp =
        GetRecordedRtpTimestamp(cast_feedback.ack_frame_id);
    ack_event.frame_id = cast_feedback.ack_frame_id;
    recent_frame_events_.push_back(ack_event);
  }

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

    // One or more frames were canceled. This may allow pending input operations
    // to complete.
    ProcessInputQueue(MOJO_RESULT_OK);
  }
}

void CastRemotingSender::ConsumeDataChunk(uint32_t offset, uint32_t size,
                                          uint32_t total_payload_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  input_queue_.push(
      base::Bind(&CastRemotingSender::TryConsumeDataChunk,
                 base::Unretained(this), offset, size, total_payload_size));
  ProcessInputQueue(MOJO_RESULT_OK);
}

void CastRemotingSender::SendFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  input_queue_.push(
      base::Bind(&CastRemotingSender::TrySendFrame, base::Unretained(this)));
  ProcessInputQueue(MOJO_RESULT_OK);
}

void CastRemotingSender::ProcessInputQueue(MojoResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  while (!input_queue_.empty()) {
    if (!input_queue_.front().Run(input_queue_discards_remaining_ > 0))
      break;  // Operation must be retried later. Stop processing queue.
    input_queue_.pop();
    if (input_queue_discards_remaining_ > 0)
      --input_queue_discards_remaining_;
  }
}

bool CastRemotingSender::TryConsumeDataChunk(uint32_t offset, uint32_t size,
                                             uint32_t total_payload_size,
                                             bool discard_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  do {
    if (!pipe_.is_valid()) {
      VLOG(1) << SENDER_SSRC << "Data pipe handle no longer valid.";
      break;
    }

    if (offset + size > total_payload_size) {
      LOG(ERROR)
          << SENDER_SSRC << "BUG: offset + size > total_payload_size ("
          << offset << " + " << size << " > " << total_payload_size << ')';
      break;
    }

    // If the data is to be discarded, do a data pipe read with the DISCARD flag
    // set.
    if (discard_data) {
      const MojoResult result = mojo::ReadDataRaw(
          pipe_.get(), nullptr, &size,
          MOJO_READ_DATA_FLAG_DISCARD | MOJO_READ_DATA_FLAG_ALL_OR_NONE);
      if (result == MOJO_RESULT_OK)
        return true;  // Successfully discarded data.
      if (result == MOJO_RESULT_OUT_OF_RANGE)
        return false;  // Retry later.
      LOG(ERROR) << SENDER_SSRC
                 << "Unexpected result when discarding from data pipe ("
                 << result << ')';
      break;
    }

    // If |total_payload_size| has changed, resize the data string. If it has
    // not changed, the following statement will be a no-op.
    next_frame_data_.resize(total_payload_size);

    const MojoResult result = mojo::ReadDataRaw(
        pipe_.get(), base::string_as_array(&next_frame_data_) + offset, &size,
        MOJO_READ_DATA_FLAG_ALL_OR_NONE);
    if (result == MOJO_RESULT_OK)
      return true;  // Successfully consumed data.
    if (result == MOJO_RESULT_OUT_OF_RANGE)
      return false;  // Retry later.
    LOG(ERROR)
        << SENDER_SSRC << "Read from data pipe failed (" << result << ')';
  } while (false);

  // If this point is reached, there was a fatal error. Shut things down and run
  // the error callback.
  pipe_watcher_.Cancel();
  pipe_.reset();
  binding_.Close();
  error_callback_.Run();
  return true;
}

bool CastRemotingSender::TrySendFrame(bool discard_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If the frame's data is to be discarded, just return immediately.
  if (discard_data)
    return true;

  // If there would be too many frames in-flight, do not proceed.
  if (NumberOfFramesInFlight() >= media::cast::kMaxUnackedFrames) {
    VLOG(1) << SENDER_SSRC
            << "Cannot send frame now because too many frames are in flight.";
    return false;
  }

  VLOG(2) << SENDER_SSRC
          << "About to send another frame: last_sent=" << last_sent_frame_id_
          << ", latest_acked=" << latest_acked_frame_id_;

  const media::cast::FrameId frame_id = last_sent_frame_id_ + 1;
  const bool is_first_frame_to_be_sent = last_send_time_.is_null();

  base::TimeTicks last_frame_reference_time = last_send_time_;
  last_send_time_ = clock_->NowTicks();
  last_sent_frame_id_ = frame_id;
  // If this is the first frame about to be sent, fake the value of
  // |latest_acked_frame_id_| to indicate the receiver starts out all caught up.
  // Also, schedule the periodic frame re-send checks.
  if (is_first_frame_to_be_sent)
    ScheduleNextResendCheck();

  DVLOG(3) << "Sending remoting frame, id = " << frame_id;

  media::cast::EncodedFrame remoting_frame;
  remoting_frame.frame_id = frame_id;
  if (flow_restart_pending_) {
    remoting_frame.dependency = media::cast::EncodedFrame::KEY;
    flow_restart_pending_ = false;
  } else {
    DCHECK(!is_first_frame_to_be_sent);
    remoting_frame.dependency = media::cast::EncodedFrame::DEPENDENT;
  }
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

  if (!frame_event_cb_.is_null()) {
    media::cast::FrameEvent remoting_event;
    remoting_event.timestamp = remoting_frame.reference_time;
    // TODO(xjz): Use a new event type for remoting.
    remoting_event.type = media::cast::FRAME_ENCODED;
    remoting_event.media_type =
        is_audio_ ? media::cast::AUDIO_EVENT : media::cast::VIDEO_EVENT;
    remoting_event.rtp_timestamp = remoting_frame.rtp_timestamp;
    remoting_event.frame_id = frame_id;
    remoting_event.size = remoting_frame.data.length();
    remoting_event.key_frame =
        remoting_frame.dependency == media::cast::EncodedFrame::KEY;
    recent_frame_events_.push_back(remoting_event);
  }

  RecordLatestFrameTimestamps(frame_id, remoting_frame.rtp_timestamp);

  transport_->InsertFrame(ssrc_, remoting_frame);

  return true;
}

void CastRemotingSender::CancelInFlightData() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::STLClearObject(&next_frame_data_);

  // TODO(miu): The following code is something we want to do as an
  // optimization. However, as-is, it's not quite correct. We can only cancel
  // frames where no packets have actually hit the network yet. Said another
  // way, we can only cancel frames the receiver has definitely not seen any
  // part of (including kickstarting!). http://crbug.com/647423
#if 0
  if (latest_acked_frame_id_ < last_sent_frame_id_) {
    std::vector<media::cast::FrameId> frames_to_cancel;
    do {
      ++latest_acked_frame_id_;
      frames_to_cancel.push_back(latest_acked_frame_id_);
    } while (latest_acked_frame_id_ < last_sent_frame_id_);
    transport_->CancelSendingFrames(ssrc_, frames_to_cancel);
  }
#endif

  // Flag that all pending input operations should discard data.
  input_queue_discards_remaining_ = input_queue_.size();

  flow_restart_pending_ = true;
  VLOG(1) << SENDER_SSRC
          << "Now restarting because in-flight data was just canceled.";
}

void CastRemotingSender::SendFrameEvents() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!frame_event_cb_.is_null());

  if (!recent_frame_events_.empty()) {
    std::vector<media::cast::FrameEvent> frame_events;
    frame_events.swap(recent_frame_events_);
    frame_event_cb_.Run(frame_events);
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&CastRemotingSender::SendFrameEvents,
                            weak_factory_.GetWeakPtr()),
      logging_flush_interval_);
}

}  // namespace cast
