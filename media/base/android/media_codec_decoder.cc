// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "media/base/android/media_codec_bridge.h"

namespace media {

namespace {

// Stop requesting new data in the kPrefetching state when the queue size
// reaches this limit.
const int kPrefetchLimit = 8;

// Request new data in the kRunning state if the queue size is less than this.
const int kPlaybackLowLimit = 4;

// Posting delay of the next frame processing, in milliseconds
const int kNextFrameDelay = 1;

// Timeout for dequeuing an input buffer from MediaCodec in milliseconds.
const int kInputBufferTimeout = 20;

// Timeout for dequeuing an output buffer from MediaCodec in milliseconds.
const int kOutputBufferTimeout = 20;
}

MediaCodecDecoder::MediaCodecDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const base::Closure& external_request_data_cb,
    const base::Closure& starvation_cb,
    const base::Closure& stop_done_cb,
    const base::Closure& error_cb,
    const char* decoder_thread_name)
    : media_task_runner_(media_task_runner),
      decoder_thread_(decoder_thread_name),
      needs_reconfigure_(false),
      external_request_data_cb_(external_request_data_cb),
      starvation_cb_(starvation_cb),
      stop_done_cb_(stop_done_cb),
      error_cb_(error_cb),
      state_(kStopped),
      eos_enqueued_(false),
      completed_(false),
      last_frame_posted_(false),
      is_data_request_in_progress_(false),
      is_incoming_data_invalid_(false),
#ifndef NDEBUG
      verify_next_frame_is_key_(false),
#endif
      weak_factory_(this) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "Decoder::Decoder() " << decoder_thread_name;

  internal_error_cb_ =
      base::Bind(&MediaCodecDecoder::OnCodecError, weak_factory_.GetWeakPtr());
  request_data_cb_ =
      base::Bind(&MediaCodecDecoder::RequestData, weak_factory_.GetWeakPtr());
}

MediaCodecDecoder::~MediaCodecDecoder() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "Decoder::~Decoder()";

  // NB: ReleaseDecoderResources() is virtual
  ReleaseDecoderResources();
}

const char* MediaCodecDecoder::class_name() const {
  return "Decoder";
}

void MediaCodecDecoder::ReleaseDecoderResources() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  // Set [kInEmergencyStop| state to block already posted ProcessNextFrame().
  SetState(kInEmergencyStop);

  decoder_thread_.Stop();  // synchronous

  SetState(kStopped);
  media_codec_bridge_.reset();
}

void MediaCodecDecoder::Flush() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  DCHECK_EQ(GetState(), kStopped);

  // Flush() is a part of the Seek request. Whenever we request a seek we need
  // to invalidate the current data request.
  if (is_data_request_in_progress_)
    is_incoming_data_invalid_ = true;

  eos_enqueued_ = false;
  completed_ = false;
  au_queue_.Flush();

#ifndef NDEBUG
  // We check and reset |verify_next_frame_is_key_| on Decoder thread.
  // This DCHECK ensures we won't need to lock this variable.
  DCHECK(!decoder_thread_.IsRunning());

  // For video the first frame after flush must be key frame.
  verify_next_frame_is_key_ = true;
#endif

  if (media_codec_bridge_) {
    // MediaCodecBridge::Reset() performs MediaCodecBridge.flush()
    MediaCodecStatus flush_status = media_codec_bridge_->Reset();
    if (flush_status != MEDIA_CODEC_OK) {
      DVLOG(0) << class_name() << "::" << __FUNCTION__
               << "MediaCodecBridge::Reset() failed";
      media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
    }
  }
}

void MediaCodecDecoder::ReleaseMediaCodec() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  media_codec_bridge_.reset();
}

bool MediaCodecDecoder::IsPrefetchingOrPlaying() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(state_lock_);
  return state_ == kPrefetching || state_ == kRunning;
}

bool MediaCodecDecoder::IsStopped() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return GetState() == kStopped;
}

bool MediaCodecDecoder::IsCompleted() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return completed_;
}

base::android::ScopedJavaLocalRef<jobject> MediaCodecDecoder::GetMediaCrypto() {
  base::android::ScopedJavaLocalRef<jobject> media_crypto;

  // TODO(timav): implement DRM.
  // drm_bridge_ is not implemented
  // if (drm_bridge_)
  //   media_crypto = drm_bridge_->GetMediaCrypto();
  return media_crypto;
}

void MediaCodecDecoder::Prefetch(const base::Closure& prefetch_done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  DCHECK(GetState() == kStopped);

  prefetch_done_cb_ = prefetch_done_cb;

  SetState(kPrefetching);
  PrefetchNextChunk();
}

MediaCodecDecoder::ConfigStatus MediaCodecDecoder::Configure() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  if (GetState() == kError) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__ << ": wrong state kError";
    return kConfigFailure;
  }

  if (needs_reconfigure_) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": needs reconfigure, deleting MediaCodec";
    needs_reconfigure_ = false;
    media_codec_bridge_.reset();

    // No need to release these buffers since the MediaCodec is deleted, just
    // remove their indexes from |delayed_buffers_|.

    ClearDelayedBuffers(false);
  }

  MediaCodecDecoder::ConfigStatus result;
  if (media_codec_bridge_) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": reconfiguration is not required, ignoring";
    result = kConfigOk;
  } else {
    result = ConfigureInternal();

#ifndef NDEBUG
    // We check and reset |verify_next_frame_is_key_| on Decoder thread.
    // This DCHECK ensures we won't need to lock this variable.
    DCHECK(!decoder_thread_.IsRunning());

    // For video the first frame after reconfiguration must be key frame.
    if (result == kConfigOk)
      verify_next_frame_is_key_ = true;
#endif
  }

  return result;
}

bool MediaCodecDecoder::Start(base::TimeDelta current_time) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__
           << " current_time:" << current_time;

  DecoderState state = GetState();
  if (state == kRunning) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << ": already started";
    return true;  // already started
  }

  if (state != kPrefetched) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__ << ": wrong state "
             << AsString(state) << ", ignoring";
    return false;
  }

  if (!media_codec_bridge_) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << ": not configured, ignoring";
    return false;
  }

  DCHECK(!decoder_thread_.IsRunning());

  // We only synchronize video stream.
  // When audio is present, the |current_time| is audio time.
  SynchronizePTSWithTime(current_time);

  last_frame_posted_ = false;

  // Start the decoder thread
  if (!decoder_thread_.Start()) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << ": cannot start decoder thread";
    return false;
  }

  DVLOG(0) << class_name() << "::" << __FUNCTION__ << " decoder thread started";

  SetState(kRunning);

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MediaCodecDecoder::ProcessNextFrame, base::Unretained(this)));

  return true;
}

void MediaCodecDecoder::SyncStop() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  if (GetState() == kError) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << ": wrong state kError, ignoring";
    return;
  }

  // After this method returns, decoder thread will not be running.

  // Set [kInEmergencyStop| state to block already posted ProcessNextFrame().
  SetState(kInEmergencyStop);

  decoder_thread_.Stop();  // synchronous

  SetState(kStopped);

  ClearDelayedBuffers(true);  // release prior to clearing  |delayed_buffers_|.
}

void MediaCodecDecoder::RequestToStop() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  DecoderState state = GetState();
  switch (state) {
    case kError:
      DVLOG(0) << class_name() << "::" << __FUNCTION__
               << ": wrong state kError, ignoring";
      break;
    case kRunning:
      SetState(kStopping);
      break;
    case kStopping:
      break;  // ignore
    case kStopped:
    case kPrefetching:
    case kPrefetched:
      // There is nothing to wait for, we can sent nofigication right away.
      DCHECK(!decoder_thread_.IsRunning());
      SetState(kStopped);
      media_task_runner_->PostTask(FROM_HERE, stop_done_cb_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void MediaCodecDecoder::OnLastFrameRendered(bool completed) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(0) << class_name() << "::" << __FUNCTION__
           << " completed:" << completed;

  decoder_thread_.Stop();  // synchronous

  SetState(kStopped);
  completed_ = completed;

  media_task_runner_->PostTask(FROM_HERE, stop_done_cb_);
}

void MediaCodecDecoder::OnDemuxerDataAvailable(const DemuxerData& data) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // If |data| contains an aborted data, the last AU will have kAborted status.
  bool aborted_data =
      !data.access_units.empty() &&
      data.access_units.back().status == DemuxerStream::kAborted;

#ifndef NDEBUG
  const char* explain_if_skipped =
      is_incoming_data_invalid_ ? " skipped as invalid"
                                : (aborted_data ? " skipped as aborted" : "");

  for (const auto& unit : data.access_units)
    DVLOG(2) << class_name() << "::" << __FUNCTION__ << explain_if_skipped
             << " au: " << unit;
  for (const auto& configs : data.demuxer_configs)
    DVLOG(2) << class_name() << "::" << __FUNCTION__ << " configs: " << configs;
#endif

  if (!is_incoming_data_invalid_ && !aborted_data)
    au_queue_.PushBack(data);

  is_incoming_data_invalid_ = false;
  is_data_request_in_progress_ = false;

  // Do not request data if we got kAborted. There is no point to request the
  // data after kAborted and before the OnDemuxerSeekDone.
  if (GetState() == kPrefetching && !aborted_data)
    PrefetchNextChunk();
}

int MediaCodecDecoder::NumDelayedRenderTasks() const {
  return 0;
}

void MediaCodecDecoder::CheckLastFrame(bool eos_encountered,
                                       bool has_delayed_tasks) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  bool last_frame_when_stopping = GetState() == kStopping && !has_delayed_tasks;

  if (last_frame_when_stopping || eos_encountered) {
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaCodecDecoder::OnLastFrameRendered,
                              weak_factory_.GetWeakPtr(), eos_encountered));
    last_frame_posted_ = true;
  }
}

void MediaCodecDecoder::OnCodecError() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Ignore codec errors from the moment surface is changed till the
  // |media_codec_bridge_| is deleted.
  if (needs_reconfigure_) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": needs reconfigure, ignoring";
    return;
  }

  SetState(kError);
  error_cb_.Run();
}

void MediaCodecDecoder::RequestData() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Ensure one data request at a time.
  if (!is_data_request_in_progress_) {
    is_data_request_in_progress_ = true;
    external_request_data_cb_.Run();
  }
}

void MediaCodecDecoder::PrefetchNextChunk() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  AccessUnitQueue::Info au_info = au_queue_.GetInfo();

  if (eos_enqueued_ || au_info.length >= kPrefetchLimit || au_info.has_eos) {
    // We are done prefetching
    SetState(kPrefetched);
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << " posting PrefetchDone";
    media_task_runner_->PostTask(FROM_HERE,
                                 base::ResetAndReturn(&prefetch_done_cb_));
    return;
  }

  request_data_cb_.Run();
}

void MediaCodecDecoder::ProcessNextFrame() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__;

  DecoderState state = GetState();

  if (state != kRunning && state != kStopping) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << ": not running";
    return;
  }

  if (state == kStopping) {
    if (NumDelayedRenderTasks() == 0 && !last_frame_posted_) {
      DVLOG(1) << class_name() << "::" << __FUNCTION__
               << ": kStopping, posting OnLastFrameRendered";
      media_task_runner_->PostTask(
          FROM_HERE, base::Bind(&MediaCodecDecoder::OnLastFrameRendered,
                                weak_factory_.GetWeakPtr(), false));
      last_frame_posted_ = true;
    }

    // We can stop processing, the |au_queue_| and MediaCodec queues can freeze.
    // We only need to let finish the delayed rendering tasks.
    return;
  }

  DCHECK(state == kRunning);

  if (!EnqueueInputBuffer())
    return;

  if (!DepleteOutputBufferQueue())
    return;

  // We need a small delay if we want to stop this thread by
  // decoder_thread_.Stop() reliably.
  // The decoder thread message loop processes all pending
  // (but not delayed) tasks before it can quit; without a delay
  // the message loop might be forever processing the pendng tasks.
  decoder_thread_.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MediaCodecDecoder::ProcessNextFrame, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kNextFrameDelay));
}

// Returns false if we should stop decoding process. Right now
// it happens if we got MediaCodec error or detected starvation.
bool MediaCodecDecoder::EnqueueInputBuffer() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__;

  if (eos_enqueued_) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": eos_enqueued, returning";
    return true;  // Nothing to do
  }

  // Keep the number pending video frames low, ideally maintaining
  // the same audio and video duration after stop request
  if (NumDelayedRenderTasks() > 1) {
    DVLOG(2) << class_name() << "::" << __FUNCTION__ << ": # delayed buffers ("
             << NumDelayedRenderTasks() << ") exceeds 1, returning";
    return true;  // Nothing to do
  }

  // Get the next frame from the queue and the queue info

  AccessUnitQueue::Info au_info = au_queue_.GetInfo();

  // Request the data from Demuxer
  if (au_info.length <= kPlaybackLowLimit && !au_info.has_eos)
    media_task_runner_->PostTask(FROM_HERE, request_data_cb_);

  // Get the next frame from the queue

  if (!au_info.length) {
    // Report starvation and return, Start() will be called again later.
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << ": starvation detected";
    media_task_runner_->PostTask(FROM_HERE, starvation_cb_);
    return true;
  }

  if (au_info.configs) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": received new configs, not implemented";
    // post an error for now?
    media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
    return false;
  }

  // We are ready to enqueue the front unit.

#ifndef NDEBUG
  if (verify_next_frame_is_key_) {
    verify_next_frame_is_key_ = false;
    VerifyUnitIsKeyFrame(au_info.front_unit);
  }
#endif

  // Dequeue input buffer

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(kInputBufferTimeout);
  int index = -1;
  MediaCodecStatus status =
      media_codec_bridge_->DequeueInputBuffer(timeout, &index);

  DVLOG(2) << class_name() << ":: DequeueInputBuffer index:" << index;

  switch (status) {
    case MEDIA_CODEC_ERROR:
      DVLOG(0) << class_name() << "::" << __FUNCTION__
               << ": MEDIA_CODEC_ERROR DequeueInputBuffer failed";
      media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
      return false;

    case MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
      DVLOG(0)
          << class_name() << "::" << __FUNCTION__
          << ": DequeueInputBuffer returned MediaCodec.INFO_TRY_AGAIN_LATER.";
      return true;

    default:
      break;
  }

  // We got the buffer
  DCHECK_EQ(status, MEDIA_CODEC_OK);
  DCHECK_GE(index, 0);

  const AccessUnit* unit = au_info.front_unit;
  DCHECK(unit);

  if (unit->is_end_of_stream) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << ": QueueEOS";
    media_codec_bridge_->QueueEOS(index);
    eos_enqueued_ = true;
    return true;
  }

  DVLOG(2) << class_name() << ":: QueueInputBuffer pts:" << unit->timestamp;

  status = media_codec_bridge_->QueueInputBuffer(
      index, &unit->data[0], unit->data.size(), unit->timestamp);

  if (status == MEDIA_CODEC_ERROR) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << ": MEDIA_CODEC_ERROR: QueueInputBuffer failed";
    media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
    return false;
  }

  // Have successfully queued input buffer, go to next access unit.
  au_queue_.Advance();
  return true;
}

// Returns false if there was MediaCodec error.
bool MediaCodecDecoder::DepleteOutputBufferQueue() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__;

  int buffer_index = 0;
  size_t offset = 0;
  size_t size = 0;
  base::TimeDelta pts;
  MediaCodecStatus status;
  bool eos_encountered = false;

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(kOutputBufferTimeout);

  // Extract all output buffers that are available.
  // Usually there will be only one, but sometimes it is preceeded by
  // MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED or MEDIA_CODEC_OUTPUT_FORMAT_CHANGED.
  do {
    status = media_codec_bridge_->DequeueOutputBuffer(
        timeout, &buffer_index, &offset, &size, &pts, &eos_encountered,
        nullptr);

    // Reset the timeout to 0 for the subsequent DequeueOutputBuffer() calls
    // to quickly break the loop after we got all currently available buffers.
    timeout = base::TimeDelta::FromMilliseconds(0);

    switch (status) {
      case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
        // Output buffers are replaced in MediaCodecBridge, nothing to do.
        break;

      case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
        DVLOG(2) << class_name() << "::" << __FUNCTION__
                 << " MEDIA_CODEC_OUTPUT_FORMAT_CHANGED";
        OnOutputFormatChanged();
        break;

      case MEDIA_CODEC_OK:
        // We got the decoded frame
        Render(buffer_index, size, true, pts, eos_encountered);
        break;

      case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
        // Nothing to do.
        break;

      case MEDIA_CODEC_ERROR:
        DVLOG(0) << class_name() << "::" << __FUNCTION__
                 << ": MEDIA_CODEC_ERROR from DequeueOutputBuffer";
        media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
        break;

      default:
        NOTREACHED();
        break;
    }

  } while (status != MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER &&
           status != MEDIA_CODEC_ERROR && !eos_encountered);

  if (eos_encountered) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << " EOS dequeued, stopping frame processing";
    return false;
  }

  if (status == MEDIA_CODEC_ERROR) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << " MediaCodec error, stopping frame processing";
    return false;
  }

  return true;
}

MediaCodecDecoder::DecoderState MediaCodecDecoder::GetState() const {
  base::AutoLock lock(state_lock_);
  return state_;
}

void MediaCodecDecoder::SetState(DecoderState state) {
  DVLOG(1) << class_name() << "::" << __FUNCTION__ << " " << AsString(state);

  base::AutoLock lock(state_lock_);
  state_ = state;
}

#undef RETURN_STRING
#define RETURN_STRING(x) \
  case x:                \
    return #x;

const char* MediaCodecDecoder::AsString(DecoderState state) {
  switch (state) {
    RETURN_STRING(kStopped);
    RETURN_STRING(kPrefetching);
    RETURN_STRING(kPrefetched);
    RETURN_STRING(kRunning);
    RETURN_STRING(kStopping);
    RETURN_STRING(kInEmergencyStop);
    RETURN_STRING(kError);
    default:
      return "Unknown DecoderState";
  }
}

#undef RETURN_STRING

}  // namespace media
