// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_track_recorder.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/opus/src/include/opus.h"

// Note that this code follows the Chrome media convention of defining a "frame"
// as "one multi-channel sample" as opposed to another common definition
// meaning "a chunk of samples". Here this second definition of "frame" is
// called a "buffer"; so what might be called "frame duration" is instead
// "buffer duration", and so on.

namespace content {

namespace {

enum {
  // This is the recommended value, according to documentation in
  // third_party/opus/src/include/opus.h, so that the Opus encoder does not
  // degrade the audio due to memory constraints.
  OPUS_MAX_PAYLOAD_SIZE = 4000,

  // Support for max sampling rate of 48KHz, 2 channels, 60 ms duration.
  MAX_SAMPLES_PER_BUFFER = 48 * 2 * 60,
};

}  // anonymous namespace

// Nested class encapsulating opus-related encoding details.
// AudioEncoder is created and destroyed on ATR's main thread (usually the
// main render thread) but otherwise should operate entirely on
// |encoder_thread_|, which is owned by AudioTrackRecorder. Be sure to delete
// |encoder_thread_| before deleting the AudioEncoder using it.
class AudioTrackRecorder::AudioEncoder
    : public base::RefCountedThreadSafe<AudioEncoder> {
 public:
  explicit AudioEncoder(const OnEncodedAudioCB& on_encoded_audio_cb)
      : on_encoded_audio_cb_(on_encoded_audio_cb), opus_encoder_(nullptr) {
    // AudioEncoder is constructed on the thread that ATR lives on, but should
    // operate only on the encoder thread after that. Reset
    // |encoder_thread_checker_| here, as the next call to CalledOnValidThread()
    // will be from the encoder thread.
    encoder_thread_checker_.DetachFromThread();
  }

  void OnSetFormat(const media::AudioParameters& params);

  void EncodeAudio(scoped_ptr<media::AudioBus> audio_bus,
                   const base::TimeTicks& capture_time);

 private:
  friend class base::RefCountedThreadSafe<AudioEncoder>;

  ~AudioEncoder();

  bool is_initialized() const { return !!opus_encoder_; }

  void DestroyExistingOpusEncoder();

  void TransferSamplesIntoBuffer(const media::AudioBus* audio_bus,
                                 int source_offset,
                                 int buffer_fill_offset,
                                 int num_samples);
  bool EncodeFromFilledBuffer(std::string* out);

  const OnEncodedAudioCB on_encoded_audio_cb_;

  base::ThreadChecker encoder_thread_checker_;

  // In the case where a call to EncodeAudio() cannot completely fill the
  // buffer, this points to the position at which to populate data in a later
  // call.
  int buffer_fill_end_;

  int frames_per_buffer_;

  // The duration of one set of frames of encoded audio samples.
  base::TimeDelta buffer_duration_;

  media::AudioParameters audio_params_;

  // Buffer for passing AudioBus data to OpusEncoder.
  scoped_ptr<float[]> buffer_;

  OpusEncoder* opus_encoder_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoder);
};

AudioTrackRecorder::AudioEncoder::~AudioEncoder() {
  // We don't DCHECK that we're on the encoder thread here, as it should have
  // already been deleted at this point.
  DestroyExistingOpusEncoder();
}

void AudioTrackRecorder::AudioEncoder::OnSetFormat(
    const media::AudioParameters& params) {
  DCHECK(encoder_thread_checker_.CalledOnValidThread());
  if (audio_params_.Equals(params))
    return;

  DestroyExistingOpusEncoder();

  if (!params.IsValid()) {
    DLOG(ERROR) << "Invalid audio params: " << params.AsHumanReadableString();
    return;
  }

  buffer_duration_ = base::TimeDelta::FromMilliseconds(
      AudioTrackRecorder::GetOpusBufferDuration(params.sample_rate()));
  if (buffer_duration_ == base::TimeDelta()) {
    DLOG(ERROR) << "Could not find a valid |buffer_duration| for the given "
                << "sample rate: " << params.sample_rate();
    return;
  }

  frames_per_buffer_ =
      params.sample_rate() * buffer_duration_.InMilliseconds() / 1000;
  if (frames_per_buffer_ * params.channels() > MAX_SAMPLES_PER_BUFFER) {
    DLOG(ERROR) << "Invalid |frames_per_buffer_|: " << frames_per_buffer_;
    return;
  }

  // Initialize AudioBus buffer for OpusEncoder.
  buffer_fill_end_ = 0;
  buffer_.reset(new float[params.channels() * frames_per_buffer_]);

  // Initialize OpusEncoder.
  int opus_result;
  opus_encoder_ = opus_encoder_create(params.sample_rate(), params.channels(),
                                      OPUS_APPLICATION_AUDIO, &opus_result);
  if (opus_result < 0) {
    DLOG(ERROR) << "Couldn't init opus encoder: " << opus_strerror(opus_result)
                << ", sample rate: " << params.sample_rate()
                << ", channels: " << params.channels();
    return;
  }

  // Note: As of 2013-10-31, the encoder in "auto bitrate" mode would use a
  // variable bitrate up to 102kbps for 2-channel, 48 kHz audio and a 10 ms
  // buffer duration. The opus library authors may, of course, adjust this in
  // later versions.
  if (opus_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(OPUS_AUTO)) != OPUS_OK) {
    DLOG(ERROR) << "Failed to set opus bitrate.";
    return;
  }

  audio_params_ = params;
}

void AudioTrackRecorder::AudioEncoder::EncodeAudio(
    scoped_ptr<media::AudioBus> audio_bus,
    const base::TimeTicks& capture_time) {
  DCHECK(encoder_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(audio_bus->channels(), audio_params_.channels());

  if (!is_initialized())
    return;

  base::TimeDelta buffer_fill_duration =
      buffer_fill_end_ * buffer_duration_ / frames_per_buffer_;
  base::TimeTicks buffer_capture_time = capture_time - buffer_fill_duration;

  // Encode all audio in |audio_bus| into zero or more packets.
  int src_pos = 0;
  while (src_pos < audio_bus->frames()) {
    const int num_samples_to_xfer = std::min(
        frames_per_buffer_ - buffer_fill_end_, audio_bus->frames() - src_pos);
    TransferSamplesIntoBuffer(audio_bus.get(), src_pos, buffer_fill_end_,
                              num_samples_to_xfer);
    src_pos += num_samples_to_xfer;
    buffer_fill_end_ += num_samples_to_xfer;

    if (buffer_fill_end_ < frames_per_buffer_)
      break;

    scoped_ptr<std::string> encoded_data(new std::string());
    if (EncodeFromFilledBuffer(encoded_data.get())) {
      on_encoded_audio_cb_.Run(audio_params_, std::move(encoded_data),
                               buffer_capture_time);
    }

    // Reset the capture timestamp and internal buffer for next set of frames.
    buffer_capture_time += buffer_duration_;
    buffer_fill_end_ = 0;
  }
}

void AudioTrackRecorder::AudioEncoder::DestroyExistingOpusEncoder() {
  // We don't DCHECK that we're on the encoder thread here, as this could be
  // called from the dtor (main thread) or from OnSetForamt() (render thread);
  if (opus_encoder_) {
    opus_encoder_destroy(opus_encoder_);
    opus_encoder_ = nullptr;
  }
}

void AudioTrackRecorder::AudioEncoder::TransferSamplesIntoBuffer(
    const media::AudioBus* audio_bus,
    int source_offset,
    int buffer_fill_offset,
    int num_samples) {
  // TODO(ajose): Consider replacing with AudioBus::ToInterleaved().
  // http://crbug.com/547918
  DCHECK(encoder_thread_checker_.CalledOnValidThread());
  DCHECK(is_initialized());
  // Opus requires channel-interleaved samples in a single array.
  for (int ch = 0; ch < audio_bus->channels(); ++ch) {
    const float* src = audio_bus->channel(ch) + source_offset;
    const float* const src_end = src + num_samples;
    float* dest =
        buffer_.get() + buffer_fill_offset * audio_params_.channels() + ch;
    for (; src < src_end; ++src, dest += audio_params_.channels())
      *dest = *src;
  }
}

bool AudioTrackRecorder::AudioEncoder::EncodeFromFilledBuffer(
    std::string* out) {
  DCHECK(encoder_thread_checker_.CalledOnValidThread());
  DCHECK(is_initialized());

  out->resize(OPUS_MAX_PAYLOAD_SIZE);
  const opus_int32 result = opus_encode_float(
      opus_encoder_, buffer_.get(), frames_per_buffer_,
      reinterpret_cast<uint8_t*>(string_as_array(out)), OPUS_MAX_PAYLOAD_SIZE);
  if (result > 1) {
    // TODO(ajose): Investigate improving this. http://crbug.com/547918
    out->resize(result);
    return true;
  }
  // If |result| in {0,1}, do nothing; the documentation says that a return
  // value of zero or one means the packet does not need to be transmitted.
  // Otherwise, we have an error.
  DLOG_IF(ERROR, result < 0) << __FUNCTION__
                             << " failed: " << opus_strerror(result);
  return false;
}

AudioTrackRecorder::AudioTrackRecorder(
    const blink::WebMediaStreamTrack& track,
    const OnEncodedAudioCB& on_encoded_audio_cb)
    : track_(track),
      encoder_(new AudioEncoder(media::BindToCurrentLoop(on_encoded_audio_cb))),
      encoder_thread_("AudioEncoderThread") {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!track_.isNull());
  DCHECK(track_.extraData());

  // Start the |encoder_thread_|. From this point on, |encoder_| should work
  // only on |encoder_thread_|, as enforced by DCHECKs.
  DCHECK(!encoder_thread_.IsRunning());
  encoder_thread_.Start();

  // Connect the source provider to the track as a sink.
  MediaStreamAudioSink::AddToAudioTrack(this, track_);
}

AudioTrackRecorder::~AudioTrackRecorder() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  MediaStreamAudioSink::RemoveFromAudioTrack(this, track_);
}

void AudioTrackRecorder::OnSetFormat(const media::AudioParameters& params) {
  DCHECK(encoder_thread_.IsRunning());
  // If the source is restarted, might have changed to another capture thread.
  capture_thread_checker_.DetachFromThread();
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&AudioEncoder::OnSetFormat, encoder_, params));
}

void AudioTrackRecorder::OnData(const media::AudioBus& audio_bus,
                                base::TimeTicks capture_time) {
  DCHECK(encoder_thread_.IsRunning());
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK(!capture_time.is_null());

  scoped_ptr<media::AudioBus> audio_data =
      media::AudioBus::Create(audio_bus.channels(), audio_bus.frames());
  audio_bus.CopyTo(audio_data.get());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&AudioEncoder::EncodeAudio, encoder_,
                            base::Passed(&audio_data), capture_time));
}

int AudioTrackRecorder::GetOpusBufferDuration(int sample_rate) {
  // Valid buffer durations in millseconds. Note there are other valid
  // durations for Opus, see https://tools.ietf.org/html/rfc6716#section-2.1.4
  // Descending order as longer durations can increase compression performance.
  const std::vector<int> opus_valid_buffer_durations_ms = {60, 40, 20, 10};

  // Search for a duration such that |sample_rate| % |buffers_per_second| == 0,
  // where |buffers_per_second| = 1000ms / |possible_duration|.
  for (auto possible_duration : opus_valid_buffer_durations_ms) {
    if (sample_rate * possible_duration % 1000 == 0) {
      return possible_duration;
    }
  }

  // Otherwise, couldn't find a good duration.
  return 0;
}

}  // namespace content
