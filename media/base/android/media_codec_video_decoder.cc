// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_video_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/buffers.h"
#include "media/base/demuxer_stream.h"

namespace media {

namespace {
const int kDelayForStandAloneEOS = 2;  // milliseconds
}

MediaCodecVideoDecoder::MediaCodecVideoDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const base::Closure& request_data_cb,
    const base::Closure& starvation_cb,
    const base::Closure& stop_done_cb,
    const base::Closure& error_cb,
    const SetTimeCallback& update_current_time_cb,
    const VideoSizeChangedCallback& video_size_changed_cb,
    const base::Closure& codec_created_cb)
    : MediaCodecDecoder(media_task_runner,
                        request_data_cb,
                        starvation_cb,
                        stop_done_cb,
                        error_cb,
                        "VideoDecoder"),
      update_current_time_cb_(update_current_time_cb),
      video_size_changed_cb_(video_size_changed_cb),
      codec_created_cb_(codec_created_cb) {
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  DVLOG(1) << "VideoDecoder::~VideoDecoder()";
  ReleaseDecoderResources();
}

const char* MediaCodecVideoDecoder::class_name() const {
  return "VideoDecoder";
}

bool MediaCodecVideoDecoder::HasStream() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return configs_.video_codec != kUnknownVideoCodec;
}

void MediaCodecVideoDecoder::SetDemuxerConfigs(const DemuxerConfigs& configs) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__ << " " << configs;

  configs_ = configs;

  if (video_size_.IsEmpty()) {
    video_size_ = configs_.video_size;
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(video_size_changed_cb_, video_size_));
  }
}

void MediaCodecVideoDecoder::ReleaseDecoderResources() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  MediaCodecDecoder::ReleaseDecoderResources();
  surface_ = gfx::ScopedJavaSurface();
  delayed_buffers_.clear();
}

void MediaCodecVideoDecoder::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__
           << (surface.IsEmpty() ? " empty" : " non-empty");

  surface_ = surface.Pass();

  needs_reconfigure_ = true;
}

bool MediaCodecVideoDecoder::HasVideoSurface() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return !surface_.IsEmpty();
}

bool MediaCodecVideoDecoder::IsCodecReconfigureNeeded(
    const DemuxerConfigs& curr,
    const DemuxerConfigs& next) const {
  if (curr.video_codec != next.video_codec ||
      curr.is_video_encrypted != next.is_video_encrypted) {
    return true;
  }

  // Only size changes below this point

  if (curr.video_size.width() == next.video_size.width() &&
      curr.video_size.height() == next.video_size.height()) {
    return false;  // i.e. curr == next
  }

  return !static_cast<VideoCodecBridge*>(media_codec_bridge_.get())
      ->IsAdaptivePlaybackSupported(next.video_size.width(),
                                    next.video_size.height());
}

MediaCodecDecoder::ConfigStatus MediaCodecVideoDecoder::ConfigureInternal() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  // If we cannot find a key frame in cache, the browser seek is needed.
  if (!au_queue_.RewindToLastKeyFrame()) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << " key frame required";
    return kConfigKeyFrameRequired;
  }

  if (configs_.video_codec == kUnknownVideoCodec) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << " configuration parameters are required";
    return kConfigFailure;
  }

  // TODO(timav): implement DRM.
  // bool is_secure = is_content_encrypted() && drm_bridge() &&
  //    drm_bridge()->IsProtectedSurfaceRequired();

  bool is_secure = false;  // DRM is not implemented

  if (surface_.IsEmpty()) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__ << " surface required";
    return kConfigFailure;
  }

  media_codec_bridge_.reset(VideoCodecBridge::CreateDecoder(
      configs_.video_codec,
      is_secure,
      configs_.video_size,
      surface_.j_surface().obj(),
      GetMediaCrypto().obj()));

  if (!media_codec_bridge_) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << " failed: cannot create video codec";
    return kConfigFailure;
  }

  DVLOG(0) << class_name() << "::" << __FUNCTION__ << " succeeded";

  media_task_runner_->PostTask(FROM_HERE, codec_created_cb_);

  return kConfigOk;
}

void MediaCodecVideoDecoder::SynchronizePTSWithTime(
    base::TimeDelta current_time) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  start_time_ticks_ = base::TimeTicks::Now();
  start_pts_ = current_time;
  last_seen_pts_ = current_time;
}

void MediaCodecVideoDecoder::OnOutputFormatChanged() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  gfx::Size prev_size = video_size_;

  // See b/18224769. The values reported from MediaCodecBridge::GetOutputFormat
  // correspond to the actual video frame size, but this is not necessarily the
  // size that should be output.
  video_size_ = configs_.video_size;
  if (video_size_ != prev_size) {
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(video_size_changed_cb_, video_size_));
  }
}

void MediaCodecVideoDecoder::Render(int buffer_index,
                                    size_t size,
                                    bool render_output,
                                    base::TimeDelta pts,
                                    bool eos_encountered) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__ << " pts:" << pts
           << " index:" << buffer_index << " size:" << size
           << (eos_encountered ? " EOS" : "");

  // Normally EOS comes as a separate access unit that does not have data,
  // the corresponding |size| will be 0.
  if (!size && eos_encountered) {
    // Stand-alone EOS
    // Discard the PTS that comes with it and ensure it is released last.
    pts = last_seen_pts_ +
          base::TimeDelta::FromMilliseconds(kDelayForStandAloneEOS);
  } else {
    // Keep track of last seen PTS
    last_seen_pts_ = pts;
  }

  if (!render_output) {
    ReleaseOutputBuffer(buffer_index, pts, size, false, eos_encountered);
    return;
  }

  base::TimeDelta time_to_render =
      pts - (base::TimeTicks::Now() - start_time_ticks_ + start_pts_);

  if (time_to_render < base::TimeDelta()) {
    // Skip late frames
    ReleaseOutputBuffer(buffer_index, pts, size, false, eos_encountered);
    return;
  }

  delayed_buffers_.insert(buffer_index);

  bool do_render = size > 0;
  decoder_thread_.task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&MediaCodecVideoDecoder::ReleaseOutputBuffer,
                            base::Unretained(this), buffer_index, pts,
                            size, do_render, eos_encountered),
      time_to_render);
}

int MediaCodecVideoDecoder::NumDelayedRenderTasks() const {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  return delayed_buffers_.size();
}

void MediaCodecVideoDecoder::ClearDelayedBuffers(bool release) {
  // Media thread
  // Called when there is no decoder thread
  if (release) {
    for (int index : delayed_buffers_)
      media_codec_bridge_->ReleaseOutputBuffer(index, false);
  }

  delayed_buffers_.clear();
}

#ifndef NDEBUG
void MediaCodecVideoDecoder::VerifyUnitIsKeyFrame(
    const AccessUnit* unit) const {
  // The first video frame in a sequence must be a key frame or stand-alone EOS.
  DCHECK(unit);
  bool stand_alone_eos = unit->is_end_of_stream && unit->data.empty();
  DCHECK(stand_alone_eos || unit->is_key_frame);
}
#endif

void MediaCodecVideoDecoder::ReleaseOutputBuffer(int buffer_index,
                                                 base::TimeDelta pts,
                                                 size_t size,
                                                 bool render,
                                                 bool eos_encountered) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__ << " pts:" << pts;

  media_codec_bridge_->ReleaseOutputBuffer(buffer_index, render);

  delayed_buffers_.erase(buffer_index);

  CheckLastFrame(eos_encountered, !delayed_buffers_.empty());

  // |update_current_time_cb_| might be null if there is audio stream.
  // Do not update current time for stand-alone EOS frames.
  if (!update_current_time_cb_.is_null() && !(eos_encountered && !size)) {
    media_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(update_current_time_cb_, pts, pts));
  }
}

}  // namespace media
