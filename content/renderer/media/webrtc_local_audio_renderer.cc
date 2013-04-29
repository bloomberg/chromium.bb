// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_renderer.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_bus.h"

namespace content {

// media::AudioRendererSink::RenderCallback implementation
int WebRtcLocalAudioRenderer::Render(
    media::AudioBus* audio_bus, int audio_delay_milliseconds) {
  base::AutoLock auto_lock(thread_lock_);

  if (!playing_) {
    audio_bus->Zero();
    return 0;
  }

  TRACE_EVENT0("audio", "WebRtcLocalAudioRenderer::Render");

  base::Time now = base::Time::Now();
  total_render_time_ += now - last_render_time_;
  last_render_time_ = now;

  DCHECK(loopback_fifo_.get() != NULL);

  // Provide data by reading from the FIFO if the FIFO contains enough
  // to fulfill the request.
  if (loopback_fifo_->frames() >= audio_bus->frames()) {
    loopback_fifo_->Consume(audio_bus, 0, audio_bus->frames());
  } else {
    audio_bus->Zero();
    // This warning is perfectly safe if it happens for the first audio
    // frames. It should not happen in a steady-state mode.
    DVLOG(2) << "loopback FIFO is empty";
  }

  return audio_bus->frames();
}

void WebRtcLocalAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
}

// content::WebRtcAudioCapturerSink implementation
void WebRtcLocalAudioRenderer::CaptureData(const int16* audio_data,
                                           int number_of_channels,
                                           int number_of_frames,
                                           int audio_delay_milliseconds,
                                           double volume) {
  TRACE_EVENT0("audio", "WebRtcLocalAudioRenderer::CaptureData");
  base::AutoLock auto_lock(thread_lock_);

  if (!playing_)
    return;

  // Push captured audio to FIFO so it can be read by a local sink.
  if (loopback_fifo_) {
    if (loopback_fifo_->frames() + number_of_frames <=
        loopback_fifo_->max_frames()) {
      scoped_ptr<media::AudioBus> audio_source = media::AudioBus::Create(
          number_of_channels, number_of_frames);
      audio_source->FromInterleaved(audio_data,
                                    audio_source->frames(),
                                    sizeof(audio_data[0]));
      loopback_fifo_->Push(audio_source.get());
    } else {
      DVLOG(1) << "FIFO is full";
    }
  }
}

void WebRtcLocalAudioRenderer::SetCaptureFormat(
    const media::AudioParameters& params) {
  audio_params_ = params;
}

// WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer implementation.
WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer(
    WebRtcLocalAudioTrack* audio_track,
    int source_render_view_id)
    : audio_track_(audio_track),
      source_render_view_id_(source_render_view_id),
      playing_(false) {
  DCHECK(audio_track);
  DVLOG(1) << "WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer()";
}

WebRtcLocalAudioRenderer::~WebRtcLocalAudioRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!sink_);
  DVLOG(1) << "WebRtcLocalAudioRenderer::~WebRtcLocalAudioRenderer()";
}

void WebRtcLocalAudioRenderer::Start() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Start()";
  DCHECK(thread_checker_.CalledOnValidThread());
  // Add this class as sink to the audio track to ensure that we receive
  // WebRtcAudioCapturerSink::CaptureData() callbacks for captured audio.
  // |audio_params_| will be updated right after the AddCapturerAudioTrack().
  audio_track_->AddSink(this);

  base::AutoLock auto_lock(thread_lock_);
  DCHECK(!sink_);

  // TODO(henrika): we could add a more dynamic solution here but I prefer
  // a fixed size combined with bad audio at overflow. The alternative is
  // that we start to build up latency and that can be more difficult to
  // detect. Tests have shown that the FIFO never contains more than 2 or 3
  // audio frames but I have selected a max size of ten buffers just
  // in case since these tests were performed on a 16 core, 64GB Win 7
  // machine. We could also add some sort of error notifier in this area if
  // the FIFO overflows.
  DCHECK(!loopback_fifo_);
  loopback_fifo_.reset(new media::AudioFifo(
      audio_params_.channels(), 10 * audio_params_.frames_per_buffer()));

  media::AudioParameters sink_params(audio_params_.format(),
                                     audio_params_.channel_layout(),
                                     audio_params_.sample_rate(),
                                     audio_params_.bits_per_sample(),
                                     2 * audio_params_.frames_per_buffer());
  sink_ = AudioDeviceFactory::NewOutputDevice(source_render_view_id_);

  // TODO(henrika): we could utilize the unified audio here instead and do
  // sink_->InitializeIO(sink_params, 2, callback_.get());
  // It would then be possible to avoid using the WebRtcAudioCapturer.
  sink_->Initialize(sink_params, this);

  // Start the capturer and local rendering. Note that, the capturer is owned
  // by the WebRTC ADM and might already bee running.
  sink_->Start();

  last_render_time_ = base::Time::Now();
  playing_ = false;
}

void WebRtcLocalAudioRenderer::Stop() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Stop()";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!sink_)
    return;

  {
    base::AutoLock auto_lock(thread_lock_);
    playing_ = false;

    if (loopback_fifo_.get() != NULL) {
      loopback_fifo_->Clear();
      loopback_fifo_.reset();
    }
  }

  // Stop the output audio stream, i.e, stop asking for data to render.
  sink_->Stop();
  sink_ = NULL;

  // Ensure that the capturer stops feeding us with captured audio.
  // Note that, we do not stop the capturer here since it may still be used by
  // the WebRTC ADM.
  audio_track_->RemoveSink(this);
  audio_track_ = NULL;
}

void WebRtcLocalAudioRenderer::Play() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Play()";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(thread_lock_);

  if (!sink_)
    return;

  // Resumes rendering by ensuring that WebRtcLocalAudioRenderer::Render()
  // now reads data from the local FIFO.
  playing_ = true;
  last_render_time_ = base::Time::Now();

  if (loopback_fifo_)
    loopback_fifo_->Clear();
}

void WebRtcLocalAudioRenderer::Pause() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Pause()";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(thread_lock_);

  if (!sink_)
    return;

  // Temporarily suspends rendering audio.
  // WebRtcLocalAudioRenderer::Render() will return early during this state
  // and only zeros will be provided to the active sink.
  playing_ = false;
}

void WebRtcLocalAudioRenderer::SetVolume(float volume) {
  DVLOG(1) << "WebRtcLocalAudioRenderer::SetVolume(" << volume << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(thread_lock_);
  if (sink_)
    sink_->SetVolume(volume);
}

base::TimeDelta WebRtcLocalAudioRenderer::GetCurrentRenderTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(thread_lock_);
  if (!sink_)
    return base::TimeDelta();
  return total_render_time();
}

bool WebRtcLocalAudioRenderer::IsLocalRenderer() const {
  return true;
}

}  // namespace content
