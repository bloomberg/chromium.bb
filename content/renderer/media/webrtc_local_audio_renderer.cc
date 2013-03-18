// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_renderer.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/renderer_audio_output_device.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
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

  if (!playing_ || !track_is_enabled_) {
    return;
  }

  // Push captured audio to FIFO so it can be read by a local sink.
  if (loopback_fifo_ && track_is_enabled_) {
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
  NOTIMPLEMENTED() << "WebRtcLocalAudioRenderer::SetCaptureFormat()";
}

// webrtc::ObserverInterface implementation
void WebRtcLocalAudioRenderer::OnChanged() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::OnChanged()";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(thread_lock_);
  track_is_enabled_ = audio_track_->enabled();
}

// WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer implementation.
WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer(
    const scoped_refptr<WebRtcAudioCapturer>& source,
    webrtc::AudioTrackInterface* audio_track,
    int source_render_view_id)
    : source_(source),
      audio_track_(audio_track),
      source_render_view_id_(source_render_view_id),
      playing_(false),
      track_is_enabled_(true) {
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
  DCHECK(source_);

  if (audio_track_) {
    audio_track_->RegisterObserver(this);
    track_is_enabled_ = audio_track_->enabled();
  }

  base::AutoLock auto_lock(thread_lock_);
  DCHECK(!sink_);

  // Add this class as sink to the capturer to ensure that we receive
  // WebRtcAudioCapturerSink::CaptureData() callbacks for captured audio.
  source_->AddCapturerSink(this);

  // Use the capturing source audio parameters when opening the output audio
  // device. Any mismatch will be compensated for by the audio output back-end.
  // Note that the buffer size is modified to make the full-duplex scheme less
  // resource intensive. By doubling the buffer size (compared to the capture
  // side), the callback frequency of browser side callbacks will be lower and
  // tests have shown that it resolves issues with audio glitches for some
  // cases where resampling is needed on the output side.
  // TODO(henrika): verify this scheme on as many different devices and
  // combinations of sample rates as possible
  media::AudioParameters source_params = source_->audio_parameters();

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
      source_params.channels(),
      10 * source_params.frames_per_buffer()));

  media::AudioParameters sink_params(source_params.format(),
                                     source_params.channel_layout(),
                                     source_params.sample_rate(),
                                     source_params.bits_per_sample(),
                                     2 * source_params.frames_per_buffer());
  sink_ = AudioDeviceFactory::NewOutputDevice();
  // TODO(henrika): we could utilize the unified audio here instead and do
  // sink_->InitializeIO(sink_params, 2, callback_.get());
  // It would then be possible to avoid using the WebRtcAudioCapturer.
  sink_->Initialize(sink_params, this);
  sink_->SetSourceRenderView(source_render_view_id_);

  // Start the capturer and local rendering. Note that, the capturer is owned
  // by the WebRTC ADM and might already bee running.
  source_->Start();
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
  source_->RemoveCapturerSink(this);
  source_ = NULL;

  if (audio_track_) {
    audio_track_->UnregisterObserver(this);
    audio_track_ = NULL;
  }
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
