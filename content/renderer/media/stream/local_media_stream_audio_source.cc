// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/local_media_stream_audio_source.h"

#include <utility>

#include "content/renderer/media/audio/audio_device_factory.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/render_frame_impl.h"

namespace content {

LocalMediaStreamAudioSource::LocalMediaStreamAudioSource(
    int consumer_render_frame_id,
    const blink::MediaStreamDevice& device,
    const int* requested_buffer_size,
    bool disable_local_echo,
    const ConstraintsCallback& started_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : blink::MediaStreamAudioSource(std::move(task_runner),
                                    true /* is_local_source */,
                                    disable_local_echo),
      consumer_render_frame_id_(consumer_render_frame_id),
      started_callback_(started_callback) {
  DVLOG(1) << "LocalMediaStreamAudioSource::LocalMediaStreamAudioSource()";
  SetDevice(device);

  int frames_per_buffer = device.input.frames_per_buffer();
  if (requested_buffer_size)
    frames_per_buffer = *requested_buffer_size;

  // If the device buffer size was not provided, use a default.
  if (frames_per_buffer <= 0) {
    frames_per_buffer =
        (device.input.sample_rate() * blink::kFallbackAudioLatencyMs) / 1000;
  }

  SetFormat(media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      device.input.channel_layout(), device.input.sample_rate(),
      frames_per_buffer));
}

LocalMediaStreamAudioSource::~LocalMediaStreamAudioSource() {
  DVLOG(1) << "LocalMediaStreamAudioSource::~LocalMediaStreamAudioSource()";
  EnsureSourceIsStopped();
}

bool LocalMediaStreamAudioSource::EnsureSourceIsStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (source_)
    return true;

  std::string str = base::StringPrintf(
      "LocalMediaStreamAudioSource::EnsureSourceIsStarted. render_frame_id=%d"
      ", channel_layout=%d, sample_rate=%d, buffer_size=%d"
      ", session_id=%d, effects=%d. ",
      consumer_render_frame_id_, device().input.channel_layout(),
      device().input.sample_rate(), device().input.frames_per_buffer(),
      device().session_id, device().input.effects());
  WebRtcLogMessage(str);
  DVLOG(1) << str;

  // Sanity-check that the consuming RenderFrame still exists. This is required
  // by AudioDeviceFactory.
  if (!RenderFrameImpl::FromRoutingID(consumer_render_frame_id_))
    return false;

  VLOG(1) << "Starting local audio input device (session_id="
          << device().session_id << ") for render frame "
          << consumer_render_frame_id_ << " with audio parameters={"
          << GetAudioParameters().AsHumanReadableString() << "}.";

  source_ = AudioDeviceFactory::NewAudioCapturerSource(
      consumer_render_frame_id_,
      media::AudioSourceParameters(device().session_id));
  source_->Initialize(GetAudioParameters(), this);
  source_->Start();
  return true;
}

void LocalMediaStreamAudioSource::EnsureSourceIsStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!source_)
    return;

  source_->Stop();
  source_ = nullptr;

  VLOG(1) << "Stopped local audio input device (session_id="
          << device().session_id << ") for render frame "
          << consumer_render_frame_id_ << " with audio parameters={"
          << GetAudioParameters().AsHumanReadableString() << "}.";
}

void LocalMediaStreamAudioSource::OnCaptureStarted() {
  started_callback_.Run(this, blink::MEDIA_DEVICE_OK, "");
}

void LocalMediaStreamAudioSource::Capture(const media::AudioBus* audio_bus,
                                          int audio_delay_milliseconds,
                                          double volume,
                                          bool key_pressed) {
  DCHECK(audio_bus);
  // TODO(miu): Plumbing is needed to determine the actual capture timestamp
  // of the audio, instead of just snapshotting TimeTicks::Now(), for proper
  // audio/video sync. https://crbug.com/335335
  DeliverDataToTracks(
      *audio_bus, base::TimeTicks::Now() - base::TimeDelta::FromMilliseconds(
                                               audio_delay_milliseconds));
}

void LocalMediaStreamAudioSource::OnCaptureError(const std::string& why) {
  WebRtcLogMessage("LocalMediaStreamAudioSource::OnCaptureError: " + why);
  StopSourceOnError(why);
}

void LocalMediaStreamAudioSource::OnCaptureMuted(bool is_muted) {
  SetMutedState(is_muted);
}

void LocalMediaStreamAudioSource::ChangeSourceImpl(
    const blink::MediaStreamDevice& new_device) {
  WebRtcLogMessage(
      "LocalMediaStreamAudioSource::ChangeSourceImpl(new_device = " +
      new_device.id + ")");
  EnsureSourceIsStopped();
  SetDevice(new_device);
  EnsureSourceIsStarted();
}

}  // namespace content
