// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/video_decoder_job.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "media/base/android/media_codec_bridge.h"

namespace media {

class VideoDecoderThread : public base::Thread {
 public:
  VideoDecoderThread() : base::Thread("MediaSource_VideoDecoderThread") {
    Start();
  }
};

// TODO(qinmin): Check if it is tolerable to use worker pool to handle all the
// decoding tasks so that we don't need a global thread here.
// http://crbug.com/245750
base::LazyInstance<VideoDecoderThread>::Leaky
    g_video_decoder_thread = LAZY_INSTANCE_INITIALIZER;


VideoDecoderJob* VideoDecoderJob::Create(
    const VideoCodec video_codec, const gfx::Size& size, jobject surface,
    jobject media_crypto, const base::Closure& request_data_cb) {
  scoped_ptr<VideoCodecBridge> codec(VideoCodecBridge::Create(video_codec));
  if (codec && codec->Start(video_codec, size, surface, media_crypto))
    return new VideoDecoderJob(codec.Pass(), request_data_cb);
  return NULL;
}

VideoDecoderJob::VideoDecoderJob(
    scoped_ptr<VideoCodecBridge> video_codec_bridge,
    const base::Closure& request_data_cb)
    : MediaDecoderJob(g_video_decoder_thread.Pointer()->message_loop_proxy(),
                      video_codec_bridge.get(), request_data_cb),
      video_codec_bridge_(video_codec_bridge.Pass()) {
}

VideoDecoderJob::~VideoDecoderJob() {
}

void VideoDecoderJob::ReleaseOutputBuffer(
    int outputBufferIndex, size_t size,
    const base::TimeDelta& presentation_timestamp,
    const MediaDecoderJob::DecoderCallback& callback,
    MediaCodecStatus status) {
  if (status != MEDIA_CODEC_OUTPUT_END_OF_STREAM || size != 0u)
    video_codec_bridge_->ReleaseOutputBuffer(outputBufferIndex, true);

  callback.Run(status, presentation_timestamp, 0);
}

bool VideoDecoderJob::ComputeTimeToRender() const {
  return true;
}

}  // namespace media
