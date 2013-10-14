// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/audio_decoder_job.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "media/base/android/media_codec_bridge.h"

namespace media {

class AudioDecoderThread : public base::Thread {
 public:
  AudioDecoderThread() : base::Thread("MediaSource_AudioDecoderThread") {
    Start();
  }
};

// TODO(qinmin): Check if it is tolerable to use worker pool to handle all the
// decoding tasks so that we don't need a global thread here.
// http://crbug.com/245750
base::LazyInstance<AudioDecoderThread>::Leaky
    g_audio_decoder_thread = LAZY_INSTANCE_INITIALIZER;

AudioDecoderJob* AudioDecoderJob::Create(
    const AudioCodec audio_codec,
    int sample_rate,
    int channel_count,
    const uint8* extra_data,
    size_t extra_data_size,
    jobject media_crypto,
    const base::Closure& request_data_cb) {
  scoped_ptr<AudioCodecBridge> codec(AudioCodecBridge::Create(audio_codec));
  if (codec && codec->Start(audio_codec, sample_rate, channel_count, extra_data,
                            extra_data_size, true, media_crypto)) {
    return new AudioDecoderJob(codec.Pass(), request_data_cb);
  }

  LOG(ERROR) << "Failed to create AudioDecoderJob.";
  return NULL;
}

AudioDecoderJob::AudioDecoderJob(
    scoped_ptr<AudioCodecBridge> audio_codec_bridge,
    const base::Closure& request_data_cb)
    : MediaDecoderJob(g_audio_decoder_thread.Pointer()->message_loop_proxy(),
                      audio_codec_bridge.get(), request_data_cb),
      audio_codec_bridge_(audio_codec_bridge.Pass()) {
}

AudioDecoderJob::~AudioDecoderJob() {
}

void AudioDecoderJob::SetVolume(double volume) {
  audio_codec_bridge_->SetVolume(volume);
}

void AudioDecoderJob::ReleaseOutputBuffer(
    int output_buffer_index,
    size_t size,
    bool render_output,
    const ReleaseOutputCompletionCallback& callback) {
  size_t size_to_render = render_output ? size : 0u;
  if (size_to_render)
    audio_codec_bridge_->PlayOutputBuffer(output_buffer_index, size_to_render);
  audio_codec_bridge_->ReleaseOutputBuffer(output_buffer_index, false);

  callback.Run(size_to_render);
}

bool AudioDecoderJob::ComputeTimeToRender() const {
  return false;
}

}  // namespace media
