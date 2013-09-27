// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_decoder_job.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/bind_to_loop.h"

namespace media {

// Timeout value for media codec operations. Because the first
// DequeInputBuffer() can take about 150 milliseconds, use 250 milliseconds
// here. See http://b/9357571.
static const int kMediaCodecTimeoutInMilliseconds = 250;

MediaDecoderJob::MediaDecoderJob(
    const scoped_refptr<base::MessageLoopProxy>& decoder_loop,
    MediaCodecBridge* media_codec_bridge,
    const base::Closure& request_data_cb)
    : ui_loop_(base::MessageLoopProxy::current()),
      decoder_loop_(decoder_loop),
      media_codec_bridge_(media_codec_bridge),
      needs_flush_(false),
      input_eos_encountered_(false),
      weak_this_(this),
      request_data_cb_(request_data_cb),
      access_unit_index_(0),
      input_buf_index_(-1),
      stop_decode_pending_(false),
      destroy_pending_(false) {
}

MediaDecoderJob::~MediaDecoderJob() {}

void MediaDecoderJob::OnDataReceived(const DemuxerData& data) {
  DVLOG(1) << __FUNCTION__ << ": " << data.access_units.size() << " units";
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(!on_data_received_cb_.is_null());

  base::Closure done_cb = base::ResetAndReturn(&on_data_received_cb_);

  if (stop_decode_pending_) {
    OnDecodeCompleted(MEDIA_CODEC_STOPPED, kNoTimestamp(), 0);
    return;
  }

  access_unit_index_ = 0;
  received_data_ = data;
  done_cb.Run();
}

void MediaDecoderJob::Prefetch(const base::Closure& prefetch_cb) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(on_data_received_cb_.is_null());
  DCHECK(decode_cb_.is_null());

  if (HasData()) {
    ui_loop_->PostTask(FROM_HERE, prefetch_cb);
    return;
  }

  RequestData(prefetch_cb);
}

bool MediaDecoderJob::Decode(
    const base::TimeTicks& start_time_ticks,
    const base::TimeDelta& start_presentation_timestamp,
    const MediaDecoderJob::DecoderCallback& callback) {
  DCHECK(decode_cb_.is_null());
  DCHECK(on_data_received_cb_.is_null());
  DCHECK(ui_loop_->BelongsToCurrentThread());

  decode_cb_ = callback;

  if (!HasData()) {
    RequestData(base::Bind(&MediaDecoderJob::DecodeNextAccessUnit,
                           base::Unretained(this),
                           start_time_ticks,
                           start_presentation_timestamp));
    return true;
  }

  if (DemuxerStream::kConfigChanged ==
      received_data_.access_units[access_unit_index_].status) {
    // Clear received data because we need to handle a config change.
    decode_cb_.Reset();
    received_data_ = DemuxerData();
    access_unit_index_ = 0;
    return false;
  }

  DecodeNextAccessUnit(start_time_ticks, start_presentation_timestamp);
  return true;
}

void MediaDecoderJob::StopDecode() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(is_decoding());
  stop_decode_pending_ = true;
}

void MediaDecoderJob::Flush() {
  DCHECK(decode_cb_.is_null());

  // Do nothing, flush when the next Decode() happens.
  needs_flush_ = true;
  received_data_ = DemuxerData();
  input_eos_encountered_ = false;
  access_unit_index_ = 0;
  on_data_received_cb_.Reset();
}

void MediaDecoderJob::Release() {
  DCHECK(ui_loop_->BelongsToCurrentThread());

  destroy_pending_ = is_decoding();

  request_data_cb_.Reset();
  on_data_received_cb_.Reset();
  decode_cb_.Reset();

  if (destroy_pending_)
    return;

  delete this;
}

MediaCodecStatus MediaDecoderJob::QueueInputBuffer(const AccessUnit& unit) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(decoder_loop_->BelongsToCurrentThread());

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
  DCHECK(ui_loop_->BelongsToCurrentThread());
  // When |input_eos_encountered_| is set, |access_units| must not be empty and
  // |access_unit_index_| must be pointing to an EOS unit. We'll reuse this
  // unit to flush the decoder until we hit output EOS.
  DCHECK(!input_eos_encountered_ ||
         (received_data_.access_units.size() > 0 &&
          access_unit_index_ < received_data_.access_units.size()))
      << " (access_units.size(): " << received_data_.access_units.size()
      << ", access_unit_index_: " << access_unit_index_ << ")";
  return access_unit_index_ < received_data_.access_units.size() ||
      input_eos_encountered_;
}

void MediaDecoderJob::RequestData(const base::Closure& done_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(on_data_received_cb_.is_null());
  DCHECK(!input_eos_encountered_);

  received_data_ = DemuxerData();
  access_unit_index_ = 0;
  on_data_received_cb_ = done_cb;

  request_data_cb_.Run();
}

void MediaDecoderJob::DecodeNextAccessUnit(
    const base::TimeTicks& start_time_ticks,
    const base::TimeDelta& start_presentation_timestamp) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(!decode_cb_.is_null());

  decoder_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaDecoderJob::DecodeInternal, base::Unretained(this),
      received_data_.access_units[access_unit_index_],
      start_time_ticks, start_presentation_timestamp, needs_flush_,
      media::BindToLoop(ui_loop_, base::Bind(
          &MediaDecoderJob::OnDecodeCompleted, base::Unretained(this)))));
  needs_flush_ = false;
}

void MediaDecoderJob::DecodeInternal(
    const AccessUnit& unit,
    const base::TimeTicks& start_time_ticks,
    const base::TimeDelta& start_presentation_timestamp,
    bool needs_flush,
    const MediaDecoderJob::DecoderCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(decoder_loop_->BelongsToCurrentThread());

  if (needs_flush) {
    DVLOG(1) << "DecodeInternal needs flush.";
    input_eos_encountered_ = false;
    MediaCodecStatus reset_status = media_codec_bridge_->Reset();
    if (MEDIA_CODEC_OK != reset_status) {
      callback.Run(reset_status, start_presentation_timestamp, 0);
      return;
    }
  }

  MediaCodecStatus input_status = MEDIA_CODEC_INPUT_END_OF_STREAM;
  if (!input_eos_encountered_) {
    input_status = QueueInputBuffer(unit);
    if (input_status == MEDIA_CODEC_INPUT_END_OF_STREAM) {
      input_eos_encountered_ = true;
    } else if (input_status != MEDIA_CODEC_OK) {
      callback.Run(input_status, start_presentation_timestamp, 0);
      return;
    }
  }

  int buffer_index = 0;
  size_t offset = 0;
  size_t size = 0;
  base::TimeDelta presentation_timestamp;
  bool output_eos_encountered = false;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      kMediaCodecTimeoutInMilliseconds);

  MediaCodecStatus status = media_codec_bridge_->DequeueOutputBuffer(
      timeout, &buffer_index, &offset, &size, &presentation_timestamp,
      &output_eos_encountered);

  if (status != MEDIA_CODEC_OK) {
    if (status == MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED) {
        media_codec_bridge_->GetOutputBuffers();
        status = MEDIA_CODEC_OK;
    }
    callback.Run(status, start_presentation_timestamp, 0);
    return;
  }

  // TODO(xhwang/qinmin): This logic is correct but strange. Clean it up.
  if (output_eos_encountered)
    status = MEDIA_CODEC_OUTPUT_END_OF_STREAM;
  else if (input_status == MEDIA_CODEC_INPUT_END_OF_STREAM)
    status = MEDIA_CODEC_INPUT_END_OF_STREAM;

  base::TimeDelta time_to_render;
  DCHECK(!start_time_ticks.is_null());
  if (ComputeTimeToRender()) {
    time_to_render = presentation_timestamp - (base::TimeTicks::Now() -
        start_time_ticks + start_presentation_timestamp);
  }

  // TODO(acolwell): Change to > since the else will never run for audio.
  if (time_to_render >= base::TimeDelta()) {
    decoder_loop_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MediaDecoderJob::ReleaseOutputBuffer,
                   weak_this_.GetWeakPtr(), buffer_index, size,
                   presentation_timestamp, callback, status),
        time_to_render);
    return;
  }

  // TODO(qinmin): The codec is lagging behind, need to recalculate the
  // |start_presentation_timestamp_| and |start_time_ticks_|.
  DVLOG(1) << "codec is lagging behind :" << time_to_render.InMicroseconds();
  ReleaseOutputBuffer(buffer_index, size, presentation_timestamp,
                      callback, status);
}

void MediaDecoderJob::OnDecodeCompleted(
    MediaCodecStatus status, const base::TimeDelta& presentation_timestamp,
    size_t audio_output_bytes) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(status != MEDIA_CODEC_STOPPED || received_data_.access_units.empty());

  if (destroy_pending_) {
    delete this;
    return;
  }

  DCHECK(!decode_cb_.is_null());
  switch (status) {
    case MEDIA_CODEC_OK:
    case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
    case MEDIA_CODEC_OUTPUT_END_OF_STREAM:
      if (!input_eos_encountered_)
        access_unit_index_++;
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

}  // namespace media
