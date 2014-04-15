// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/audio_decoder_job.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/audio_timestamp_helper.h"

namespace {

// Use 16bit PCM for audio output. Keep this value in sync with the output
// format we passed to AudioTrack in MediaCodecBridge.
const int kBytesPerAudioOutputSample = 2;
}

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
    scoped_ptr<AudioTimestampHelper> audio_timestamp_helper(
        new AudioTimestampHelper(sample_rate));
    return new AudioDecoderJob(
        audio_timestamp_helper.Pass(), codec.Pass(),
        kBytesPerAudioOutputSample * channel_count, request_data_cb);
  }
  LOG(ERROR) << "Failed to create AudioDecoderJob.";
  return NULL;
}

AudioDecoderJob::AudioDecoderJob(
    scoped_ptr<AudioTimestampHelper> audio_timestamp_helper,
    scoped_ptr<AudioCodecBridge> audio_codec_bridge,
    int bytes_per_frame,
    const base::Closure& request_data_cb)
    : MediaDecoderJob(g_audio_decoder_thread.Pointer()->message_loop_proxy(),
                      audio_codec_bridge.get(), request_data_cb),
      bytes_per_frame_(bytes_per_frame),
      audio_codec_bridge_(audio_codec_bridge.Pass()),
      audio_timestamp_helper_(audio_timestamp_helper.Pass()) {
}

AudioDecoderJob::~AudioDecoderJob() {
}

void AudioDecoderJob::SetVolume(double volume) {
  audio_codec_bridge_->SetVolume(volume);
}

void AudioDecoderJob::SetBaseTimestamp(base::TimeDelta base_timestamp) {
  audio_timestamp_helper_->SetBaseTimestamp(base_timestamp);
}

void AudioDecoderJob::ReleaseOutputBuffer(
    int output_buffer_index,
    size_t size,
    bool render_output,
    base::TimeDelta current_presentation_timestamp,
    const ReleaseOutputCompletionCallback& callback) {
  render_output = render_output && (size != 0u);
  if (render_output) {
    int64 head_position = audio_codec_bridge_->PlayOutputBuffer(
        output_buffer_index, size);
    audio_timestamp_helper_->AddFrames(size / (bytes_per_frame_));
    int64 frames_to_play =
        audio_timestamp_helper_->frame_count() - head_position;
    DCHECK_GE(frames_to_play, 0);
    current_presentation_timestamp =
        audio_timestamp_helper_->GetTimestamp() -
        audio_timestamp_helper_->GetFrameDuration(frames_to_play);
  } else {
    current_presentation_timestamp = kNoTimestamp();
  }
  audio_codec_bridge_->ReleaseOutputBuffer(output_buffer_index, false);
  callback.Run(current_presentation_timestamp,
               audio_timestamp_helper_->GetTimestamp());
}

bool AudioDecoderJob::ComputeTimeToRender() const {
  return false;
}

}  // namespace media
