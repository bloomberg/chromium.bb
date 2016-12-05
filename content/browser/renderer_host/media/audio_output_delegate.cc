// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_delegate.h"

#include <utility>

#include "base/bind.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_sync_reader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_observer.h"

namespace content {

AudioOutputDelegate::AudioOutputDelegate(
    EventHandler* handler,
    media::AudioManager* audio_manager,
    std::unique_ptr<media::AudioLog> audio_log,
    int stream_id,
    int render_frame_id,
    int render_process_id,
    const media::AudioParameters& params,
    const std::string& output_device_id)
    : handler_(handler),
      audio_log_(std::move(audio_log)),
      reader_(AudioSyncReader::Create(params)),
      stream_id_(stream_id),
      render_frame_id_(render_frame_id),
      render_process_id_(render_process_id),
      weak_factory_(this) {
  DCHECK(handler_);
  DCHECK(audio_manager);
  DCHECK(audio_log_);
  weak_this_ = weak_factory_.GetWeakPtr();
  audio_log_->OnCreated(stream_id, params, output_device_id);
  controller_ = media::AudioOutputController::Create(
      audio_manager, this, params, output_device_id, reader_.get());
  DCHECK(controller_);
}

AudioOutputDelegate::~AudioOutputDelegate() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!playing_);
  DCHECK(!handler_);
}

void AudioOutputDelegate::Deleter::operator()(AudioOutputDelegate* delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  delegate->UpdatePlayingState(false);
  delegate->handler_ = nullptr;
  delegate->audio_log_->OnClosed(delegate->stream_id_);

  // |controller| will call the closure (on IO thread) when it's done closing,
  // and it is only after that call that we can delete |delegate|. By giving the
  // closure ownership of |delegate|, we keep delegate alive until |controller|
  // is closed.
  //
  // The mirroring manager is a leaky singleton, so Unretained is fine.
  delegate->controller_->Close(base::Bind(
      [](AudioOutputDelegate* delegate,
         AudioMirroringManager* mirroring_manager) {
        // De-register the controller from the AudioMirroringManager now that
        // the controller has closed the AudioOutputStream and shut itself down.
        // This ensures that calling RemoveDiverter() here won't trigger the
        // controller to re-start the default AudioOutputStream and cause a
        // brief audio blip to come out the user's speakers.
        // http://crbug.com/474432
        //
        // It's fine if the task is canceled during shutdown, since the
        // mirroring manager doesn't require that all diverters are
        // removed.
        if (mirroring_manager)
          mirroring_manager->RemoveDiverter(delegate->controller_.get());
      },
      base::Owned(delegate), base::Unretained(mirroring_manager_)));
}

// static
AudioOutputDelegate::UniquePtr AudioOutputDelegate::Create(
    EventHandler* handler,
    media::AudioManager* audio_manager,
    std::unique_ptr<media::AudioLog> audio_log,
    AudioMirroringManager* mirroring_manager,
    MediaObserver* media_observer,
    int stream_id,
    int render_frame_id,
    int render_process_id,
    const media::AudioParameters& params,
    const std::string& output_device_id) {
  if (media_observer)
    media_observer->OnCreatingAudioStream(render_process_id, render_frame_id);
  UniquePtr delegate(
      new AudioOutputDelegate(handler, audio_manager, std::move(audio_log),
                              stream_id, render_frame_id, render_process_id,
                              params, output_device_id),
      Deleter(mirroring_manager));
  if (mirroring_manager)
    mirroring_manager->AddDiverter(render_process_id, render_frame_id,
                                   delegate->controller_.get());
  return delegate;
}

void AudioOutputDelegate::OnPlayStream() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller_->Play();
  audio_log_->OnStarted(stream_id_);
}

void AudioOutputDelegate::OnPauseStream() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller_->Pause();
  audio_log_->OnStopped(stream_id_);
}

void AudioOutputDelegate::OnSetVolume(double volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GE(volume, 0);
  DCHECK_LE(volume, 1);
  controller_->SetVolume(volume);
  audio_log_->OnSetVolume(stream_id_, volume);
}

void AudioOutputDelegate::OnControllerCreated() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegate::SendCreatedNotification, weak_this_));
}

void AudioOutputDelegate::OnControllerPlaying() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegate::UpdatePlayingState, weak_this_, true));
}

void AudioOutputDelegate::OnControllerPaused() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegate::UpdatePlayingState, weak_this_, false));
}

void AudioOutputDelegate::OnControllerError() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegate::OnError, weak_this_));
}

void AudioOutputDelegate::SendCreatedNotification() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (handler_) {
    handler_->OnStreamCreated(stream_id_, reader_->shared_memory(),
                              reader_->foreign_socket());
  }
}

void AudioOutputDelegate::UpdatePlayingState(bool playing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!handler_ || playing == playing_)
    return;

  playing_ = playing;
  handler_->OnStreamStateChanged(playing);
  if (playing) {
    // Note that this takes a reference to |controller_|, and
    // (Start|Stop)MonitoringStream calls are async, so we don't have a
    // guarantee for when the controller is destroyed.
    AudioStreamMonitor::StartMonitoringStream(
        render_process_id_, render_frame_id_, stream_id_,
        base::Bind(&media::AudioOutputController::ReadCurrentPowerAndClip,
                   controller_));
  } else {
    AudioStreamMonitor::StopMonitoringStream(render_process_id_,
                                             render_frame_id_, stream_id_);
  }
}

void AudioOutputDelegate::OnError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!handler_)
    return;

  audio_log_->OnError(stream_id_);
  handler_->OnStreamError(stream_id_);
}

}  // namespace content
