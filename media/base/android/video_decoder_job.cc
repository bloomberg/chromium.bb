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
    const VideoCodec video_codec,
    bool is_secure,
    const gfx::Size& size,
    jobject surface,
    jobject media_crypto,
    const base::Closure& request_data_cb,
    const base::Closure& request_resources_cb,
    const base::Closure& release_resources_cb) {
  scoped_ptr<VideoCodecBridge> codec(VideoCodecBridge::CreateDecoder(
      video_codec, is_secure, size, surface, media_crypto));
  if (codec)
    return new VideoDecoderJob(codec.Pass(), request_data_cb,
                               request_resources_cb, release_resources_cb);

  LOG(ERROR) << "Failed to create VideoDecoderJob.";
  return NULL;
}

VideoDecoderJob::VideoDecoderJob(
    scoped_ptr<VideoCodecBridge> video_codec_bridge,
    const base::Closure& request_data_cb,
    const base::Closure& request_resources_cb,
    const base::Closure& release_resources_cb)
    : MediaDecoderJob(g_video_decoder_thread.Pointer()->message_loop_proxy(),
                      video_codec_bridge.get(), request_data_cb),
      video_codec_bridge_(video_codec_bridge.Pass()),
      release_resources_cb_(release_resources_cb) {
  request_resources_cb.Run();
}

VideoDecoderJob::~VideoDecoderJob() {
  release_resources_cb_.Run();
}

void VideoDecoderJob::ReleaseOutputBuffer(
    int output_buffer_index,
    size_t size,
    bool render_output,
    const ReleaseOutputCompletionCallback& callback) {
  video_codec_bridge_->ReleaseOutputBuffer(output_buffer_index, render_output);
  callback.Run(0u);
}

bool VideoDecoderJob::ComputeTimeToRender() const {
  return true;
}

}  // namespace media
