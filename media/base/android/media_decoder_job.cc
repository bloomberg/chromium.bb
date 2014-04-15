// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_decoder_job.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/buffers.h"

namespace media {

// Timeout value for media codec operations. Because the first
// DequeInputBuffer() can take about 150 milliseconds, use 250 milliseconds
// here. See http://b/9357571.
static const int kMediaCodecTimeoutInMilliseconds = 250;

MediaDecoderJob::MediaDecoderJob(
    const scoped_refptr<base::SingleThreadTaskRunner>& decoder_task_runner,
    MediaCodecBridge* media_codec_bridge,
    const base::Closure& request_data_cb)
    : ui_task_runner_(base::MessageLoopProxy::current()),
      decoder_task_runner_(decoder_task_runner),
      media_codec_bridge_(media_codec_bridge),
      needs_flush_(false),
      input_eos_encountered_(false),
      output_eos_encountered_(false),
      skip_eos_enqueue_(true),
      prerolling_(true),
      request_data_cb_(request_data_cb),
      current_demuxer_data_index_(0),
      input_buf_index_(-1),
      stop_decode_pending_(false),
      destroy_pending_(false),
      is_requesting_demuxer_data_(false),
      is_incoming_data_invalid_(false),
      weak_factory_(this) {
  InitializeReceivedData();
}

MediaDecoderJob::~MediaDecoderJob() {}

void MediaDecoderJob::OnDataReceived(const DemuxerData& data) {
  DVLOG(1) << __FUNCTION__ << ": " << data.access_units.size() << " units";
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(NoAccessUnitsRemainingInChunk(false));

  TRACE_EVENT_ASYNC_END2(
      "media", "MediaDecoderJob::RequestData", this,
      "Data type", data.type == media::DemuxerStream::AUDIO ? "AUDIO" : "VIDEO",
      "Units read", data.access_units.size());

  if (is_incoming_data_invalid_) {
    is_incoming_data_invalid_ = false;

    // If there is a pending callback, need to request the data again to get
    // valid data.
    if (!on_data_received_cb_.is_null())
      request_data_cb_.Run();
    else
      is_requesting_demuxer_data_ = false;
    return;
  }

  size_t next_demuxer_data_index = inactive_demuxer_data_index();
  received_data_[next_demuxer_data_index] = data;
  access_unit_index_[next_demuxer_data_index] = 0;
  is_requesting_demuxer_data_ = false;

  base::Closure done_cb = base::ResetAndReturn(&on_data_received_cb_);

  // If this data request is for the inactive chunk, or |on_data_received_cb_|
  // was set to null by ClearData() or Release(), do nothing.
  if (done_cb.is_null())
    return;

  if (stop_decode_pending_) {
    DCHECK(is_decoding());
    OnDecodeCompleted(MEDIA_CODEC_STOPPED, kNoTimestamp(), 0);
    return;
  }

  done_cb.Run();
}

void MediaDecoderJob::Prefetch(const base::Closure& prefetch_cb) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(on_data_received_cb_.is_null());
  DCHECK(decode_cb_.is_null());

  if (HasData()) {
    DVLOG(1) << __FUNCTION__ << " : using previously received data";
    ui_task_runner_->PostTask(FROM_HERE, prefetch_cb);
    return;
  }

  DVLOG(1) << __FUNCTION__ << " : requesting data";
  RequestData(prefetch_cb);
}

bool MediaDecoderJob::Decode(
    base::TimeTicks start_time_ticks,
    base::TimeDelta start_presentation_timestamp,
    const DecoderCallback& callback) {
  DCHECK(decode_cb_.is_null());
  DCHECK(on_data_received_cb_.is_null());
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  decode_cb_ = callback;

  if (!HasData()) {
    RequestData(base::Bind(&MediaDecoderJob::DecodeCurrentAccessUnit,
                           base::Unretained(this),
                           start_time_ticks,
                           start_presentation_timestamp));
    return true;
  }

  if (DemuxerStream::kConfigChanged == CurrentAccessUnit().status) {
    // Clear received data because we need to handle a config change.
    decode_cb_.Reset();
    ClearData();
    return false;
  }

  DecodeCurrentAccessUnit(start_time_ticks, start_presentation_timestamp);
  return true;
}

void MediaDecoderJob::StopDecode() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(is_decoding());
  stop_decode_pending_ = true;
}

void MediaDecoderJob::Flush() {
  DCHECK(decode_cb_.is_null());

  // Do nothing, flush when the next Decode() happens.
  needs_flush_ = true;
  ClearData();
}

void MediaDecoderJob::BeginPrerolling(base::TimeDelta preroll_timestamp) {
  DVLOG(1) << __FUNCTION__ << "(" << preroll_timestamp.InSecondsF() << ")";
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_decoding());

  preroll_timestamp_ = preroll_timestamp;
  prerolling_ = true;
}

void MediaDecoderJob::Release() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  // If the decoder job is not waiting for data, and is still decoding, we
  // cannot delete the job immediately.
  destroy_pending_ = on_data_received_cb_.is_null() && is_decoding();

  request_data_cb_.Reset();
  on_data_received_cb_.Reset();
  decode_cb_.Reset();

  if (destroy_pending_) {
    DVLOG(1) << __FUNCTION__ << " : delete is pending decode completion";
    return;
  }

  delete this;
}

MediaCodecStatus MediaDecoderJob::QueueInputBuffer(const AccessUnit& unit) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", __FUNCTION__);

  int input_buf_index = input_buf_index_;
  input_buf_index_ = -1;

  // TODO(xhwang): Hide DequeueInputBuffer() and the index in MediaCodecBridge.
  if (input_buf_index == -1) {
    base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
        kMediaCodecTimeoutInMilliseconds);
    MediaCodecStatus status =
        media_codec_bridge_->DequeueInputBuffer(timeout, &input_buf_index);
    if (status != MEDIA_CODEC_OK) {
      DVLOG(1) << "DequeueInputBuffer fails: " << status;
      return status;
    }
  }

  // TODO(qinmin): skip frames if video is falling far behind.
  DCHECK_GE(input_buf_index, 0);
  if (unit.end_of_stream || unit.data.empty()) {
    media_codec_bridge_->QueueEOS(input_buf_index);
    return MEDIA_CODEC_INPUT_END_OF_STREAM;
  }

  if (unit.key_id.empty() || unit.iv.empty()) {
    DCHECK(unit.iv.empty() || !unit.key_id.empty());
    return media_codec_bridge_->QueueInputBuffer(
        input_buf_index, &unit.data[0], unit.data.size(), unit.timestamp);
  }

  MediaCodecStatus status = media_codec_bridge_->QueueSecureInputBuffer(
      input_buf_index,
      &unit.data[0], unit.data.size(),
      reinterpret_cast<const uint8*>(&unit.key_id[0]), unit.key_id.size(),
      reinterpret_cast<const uint8*>(&unit.iv[0]), unit.iv.size(),
      unit.subsamples.empty() ? NULL : &unit.subsamples[0],
      unit.subsamples.size(),
      unit.timestamp);

  // In case of MEDIA_CODEC_NO_KEY, we must reuse the |input_buf_index_|.
  // Otherwise MediaDrm will report errors.
  if (status == MEDIA_CODEC_NO_KEY)
    input_buf_index_ = input_buf_index;

  return status;
}

bool MediaDecoderJob::HasData() const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // When |input_eos_encountered_| is set, |access_unit_index_| and
  // |current_demuxer_data_index_| must be pointing to an EOS unit.
  // We'll reuse this unit to flush the decoder until we hit output EOS.
  DCHECK(!input_eos_encountered_ || !NoAccessUnitsRemainingInChunk(true));
  return !NoAccessUnitsRemainingInChunk(true) ||
      !NoAccessUnitsRemainingInChunk(false);
}

void MediaDecoderJob::RequestData(const base::Closure& done_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(on_data_received_cb_.is_null());
  DCHECK(!input_eos_encountered_);
  DCHECK(NoAccessUnitsRemainingInChunk(false));

  TRACE_EVENT_ASYNC_BEGIN0("media", "MediaDecoderJob::RequestData", this);

  on_data_received_cb_ = done_cb;

  // If we are already expecting new data, just set the callback and do
  // nothing.
  if (is_requesting_demuxer_data_)
    return;

  // The new incoming data will be stored as the next demuxer data chunk, since
  // the decoder might still be decoding the current one.
  size_t next_demuxer_data_index = inactive_demuxer_data_index();
  received_data_[next_demuxer_data_index] = DemuxerData();
  access_unit_index_[next_demuxer_data_index] = 0;
  is_requesting_demuxer_data_ = true;

  request_data_cb_.Run();
}

void MediaDecoderJob::DecodeCurrentAccessUnit(
    base::TimeTicks start_time_ticks,
    base::TimeDelta start_presentation_timestamp) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!decode_cb_.is_null());

  RequestCurrentChunkIfEmpty();
  const AccessUnit& access_unit = CurrentAccessUnit();
  // If the first access unit is a config change, request the player to dequeue
  // the input buffer again so that it can request config data.
  if (access_unit.status == DemuxerStream::kConfigChanged) {
    ui_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&MediaDecoderJob::OnDecodeCompleted,
                                         base::Unretained(this),
                                         MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER,
                                         kNoTimestamp(),
                                         0));
    return;
  }

  decoder_task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaDecoderJob::DecodeInternal, base::Unretained(this),
      access_unit,
      start_time_ticks, start_presentation_timestamp, needs_flush_,
      media::BindToCurrentLoop(base::Bind(
          &MediaDecoderJob::OnDecodeCompleted, base::Unretained(this)))));
  needs_flush_ = false;
}

void MediaDecoderJob::DecodeInternal(
    const AccessUnit& unit,
    base::TimeTicks start_time_ticks,
    base::TimeDelta start_presentation_timestamp,
    bool needs_flush,
    const MediaDecoderJob::DecoderCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", __FUNCTION__);

  if (needs_flush) {
    DVLOG(1) << "DecodeInternal needs flush.";
    input_eos_encountered_ = false;
    output_eos_encountered_ = false;
    MediaCodecStatus reset_status = media_codec_bridge_->Reset();
    if (MEDIA_CODEC_OK != reset_status) {
      callback.Run(reset_status, kNoTimestamp(), 0);
      return;
    }
  }

  // Once output EOS has occurred, we should not be asked to decode again.
  // MediaCodec has undefined behavior if similarly asked to decode after output
  // EOS.
  DCHECK(!output_eos_encountered_);

  // For aborted access unit, just skip it and inform the player.
  if (unit.status == DemuxerStream::kAborted) {
    // TODO(qinmin): use a new enum instead of MEDIA_CODEC_STOPPED.
    callback.Run(MEDIA_CODEC_STOPPED, kNoTimestamp(), 0);
    return;
  }

  if (skip_eos_enqueue_) {
    if (unit.end_of_stream || unit.data.empty()) {
      input_eos_encountered_ = true;
      output_eos_encountered_ = true;
      callback.Run(MEDIA_CODEC_OUTPUT_END_OF_STREAM, kNoTimestamp(), 0);
      return;
    }

    skip_eos_enqueue_ = false;
  }

  MediaCodecStatus input_status = MEDIA_CODEC_INPUT_END_OF_STREAM;
  if (!input_eos_encountered_) {
    input_status = QueueInputBuffer(unit);
    if (input_status == MEDIA_CODEC_INPUT_END_OF_STREAM) {
      input_eos_encountered_ = true;
    } else if (input_status != MEDIA_CODEC_OK) {
      callback.Run(input_status, kNoTimestamp(), 0);
      return;
    }
  }

  int buffer_index = 0;
  size_t offset = 0;
  size_t size = 0;
  base::TimeDelta presentation_timestamp;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      kMediaCodecTimeoutInMilliseconds);

  MediaCodecStatus status =
      media_codec_bridge_->DequeueOutputBuffer(timeout,
                                               &buffer_index,
                                               &offset,
                                               &size,
                                               &presentation_timestamp,
                                               &output_eos_encountered_,
                                               NULL);

  if (status != MEDIA_CODEC_OK) {
    if (status == MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED &&
        !media_codec_bridge_->GetOutputBuffers()) {
      status = MEDIA_CODEC_ERROR;
    }
    callback.Run(status, kNoTimestamp(), 0);
    return;
  }

  // TODO(xhwang/qinmin): This logic is correct but strange. Clean it up.
  if (output_eos_encountered_)
    status = MEDIA_CODEC_OUTPUT_END_OF_STREAM;
  else if (input_status == MEDIA_CODEC_INPUT_END_OF_STREAM)
    status = MEDIA_CODEC_INPUT_END_OF_STREAM;

  bool render_output  = presentation_timestamp >= preroll_timestamp_ &&
      (status != MEDIA_CODEC_OUTPUT_END_OF_STREAM || size != 0u);
  base::TimeDelta time_to_render;
  DCHECK(!start_time_ticks.is_null());
  if (render_output && ComputeTimeToRender()) {
    time_to_render = presentation_timestamp - (base::TimeTicks::Now() -
        start_time_ticks + start_presentation_timestamp);
  }

  if (time_to_render > base::TimeDelta()) {
    decoder_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MediaDecoderJob::ReleaseOutputBuffer,
                   weak_factory_.GetWeakPtr(),
                   buffer_index,
                   size,
                   render_output,
                   base::Bind(callback, status, presentation_timestamp)),
        time_to_render);
    return;
  }

  // TODO(qinmin): The codec is lagging behind, need to recalculate the
  // |start_presentation_timestamp_| and |start_time_ticks_| in
  // media_source_player.cc.
  DVLOG(1) << "codec is lagging behind :" << time_to_render.InMicroseconds();
  if (render_output) {
    // The player won't expect a timestamp smaller than the
    // |start_presentation_timestamp|. However, this could happen due to decoder
    // errors.
    presentation_timestamp = std::max(
        presentation_timestamp, start_presentation_timestamp);
  } else {
    presentation_timestamp = kNoTimestamp();
  }
  ReleaseOutputCompletionCallback completion_callback = base::Bind(
      callback, status, presentation_timestamp);
  ReleaseOutputBuffer(buffer_index, size, render_output, completion_callback);
}

void MediaDecoderJob::OnDecodeCompleted(
    MediaCodecStatus status, base::TimeDelta presentation_timestamp,
    size_t audio_output_bytes) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (destroy_pending_) {
    DVLOG(1) << __FUNCTION__ << " : completing pending deletion";
    delete this;
    return;
  }

  DCHECK(!decode_cb_.is_null());

  // If output was queued for rendering, then we have completed prerolling.
  if (presentation_timestamp != kNoTimestamp())
    prerolling_ = false;

  switch (status) {
    case MEDIA_CODEC_OK:
    case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
    case MEDIA_CODEC_OUTPUT_END_OF_STREAM:
      if (!input_eos_encountered_)
        access_unit_index_[current_demuxer_data_index_]++;
      break;

    case MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
    case MEDIA_CODEC_INPUT_END_OF_STREAM:
    case MEDIA_CODEC_NO_KEY:
    case MEDIA_CODEC_STOPPED:
    case MEDIA_CODEC_ERROR:
      // Do nothing.
      break;
  };

  stop_decode_pending_ = false;
  base::ResetAndReturn(&decode_cb_).Run(status, presentation_timestamp,
                                        audio_output_bytes);
}

const AccessUnit& MediaDecoderJob::CurrentAccessUnit() const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(HasData());
  int index = NoAccessUnitsRemainingInChunk(true) ?
      inactive_demuxer_data_index() : current_demuxer_data_index_;
  return received_data_[index].access_units[access_unit_index_[index]];
}

bool MediaDecoderJob::NoAccessUnitsRemainingInChunk(
    bool is_active_chunk) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  size_t index = is_active_chunk ? current_demuxer_data_index_ :
      inactive_demuxer_data_index();
  return received_data_[index].access_units.size() <= access_unit_index_[index];
}

void MediaDecoderJob::ClearData() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  current_demuxer_data_index_ = 0;
  InitializeReceivedData();
  on_data_received_cb_.Reset();
  if (is_requesting_demuxer_data_)
    is_incoming_data_invalid_ = true;
  input_eos_encountered_ = false;
}

void MediaDecoderJob::RequestCurrentChunkIfEmpty() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(HasData());
  if (!NoAccessUnitsRemainingInChunk(true))
    return;

  // Requests new data if the the last access unit of the next chunk is not EOS.
  current_demuxer_data_index_ = inactive_demuxer_data_index();
  const AccessUnit last_access_unit =
      received_data_[current_demuxer_data_index_].access_units.back();
  if (!last_access_unit.end_of_stream &&
      last_access_unit.status != DemuxerStream::kConfigChanged &&
      last_access_unit.status != DemuxerStream::kAborted) {
    RequestData(base::Closure());
  }
}

void MediaDecoderJob::InitializeReceivedData() {
  for (size_t i = 0; i < 2; ++i) {
    received_data_[i] = DemuxerData();
    access_unit_index_[i] = 0;
  }
}

}  // namespace media
