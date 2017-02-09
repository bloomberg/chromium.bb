// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_delegate_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_sync_reader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_observer.h"
#include "media/audio/audio_output_controller.h"

namespace content {

// This class trampolines callbacks from the controller to the delegate. Since
// callbacks from the controller are stopped asynchronously, this class holds
// a weak pointer to the delegate, allowing the delegate to stop callbacks
// synchronously.
class AudioOutputDelegateImpl::ControllerEventHandler
    : public media::AudioOutputController::EventHandler {
 public:
  explicit ControllerEventHandler(
      base::WeakPtr<AudioOutputDelegateImpl> delegate);

 private:
  void OnControllerCreated() override;
  void OnControllerPlaying() override;
  void OnControllerPaused() override;
  void OnControllerError() override;

  base::WeakPtr<AudioOutputDelegateImpl> delegate_;
};

AudioOutputDelegateImpl::ControllerEventHandler::ControllerEventHandler(
    base::WeakPtr<AudioOutputDelegateImpl> delegate)
    : delegate_(std::move(delegate)) {}

void AudioOutputDelegateImpl::ControllerEventHandler::OnControllerCreated() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegateImpl::SendCreatedNotification, delegate_));
}

void AudioOutputDelegateImpl::ControllerEventHandler::OnControllerPlaying() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegateImpl::UpdatePlayingState, delegate_,
                 true));
}

void AudioOutputDelegateImpl::ControllerEventHandler::OnControllerPaused() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegateImpl::UpdatePlayingState, delegate_,
                 false));
}

void AudioOutputDelegateImpl::ControllerEventHandler::OnControllerError() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioOutputDelegateImpl::OnError, delegate_));
}

AudioOutputDelegateImpl::AudioOutputDelegateImpl(
    EventHandler* handler,
    media::AudioManager* audio_manager,
    std::unique_ptr<media::AudioLog> audio_log,
    AudioMirroringManager* mirroring_manager,
    MediaObserver* media_observer,
    int stream_id,
    int render_frame_id,
    int render_process_id,
    const media::AudioParameters& params,
    const std::string& output_device_id)
    : subscriber_(handler),
      audio_log_(std::move(audio_log)),
      reader_(AudioSyncReader::Create(params)),
      mirroring_manager_(mirroring_manager),
      stream_id_(stream_id),
      render_frame_id_(render_frame_id),
      render_process_id_(render_process_id),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(subscriber_);
  DCHECK(audio_manager);
  DCHECK(audio_log_);
  DCHECK(mirroring_manager_);
  // Since the event handler never directly calls functions on |this| but rather
  // posts them to the IO thread, passing a pointer from the constructor is
  // safe.
  controller_event_handler_ =
      base::MakeUnique<ControllerEventHandler>(weak_factory_.GetWeakPtr());
  audio_log_->OnCreated(stream_id, params, output_device_id);
  controller_ = media::AudioOutputController::Create(
      audio_manager, controller_event_handler_.get(), params, output_device_id,
      reader_.get());
  DCHECK(controller_);
  if (media_observer)
    media_observer->OnCreatingAudioStream(render_process_id, render_frame_id);
  mirroring_manager_->AddDiverter(render_process_id, render_frame_id,
                                  controller_.get());
}

AudioOutputDelegateImpl::~AudioOutputDelegateImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UpdatePlayingState(false);
  audio_log_->OnClosed(stream_id_);

  // Since the ownership of |controller_| is shared, we instead use its Close
  // method to stop callbacks from it. |controller_| will call the closure (on
  // the IO thread) when it's done closing, and it is only after that call that
  // we can delete |controller_event_handler_| and |reader_|. By giving the
  // closure ownership of these, we keep them alive until |controller_| is
  // closed. |mirroring_manager_| is a lazy instance, so passing it is safe.
  controller_->Close(base::Bind(
      [](AudioMirroringManager* mirroring_manager,
         std::unique_ptr<ControllerEventHandler> event_handler,
         std::unique_ptr<AudioSyncReader> reader,
         scoped_refptr<media::AudioOutputController> controller) {
        // De-register the controller from the AudioMirroringManager now that
        // the controller has closed the AudioOutputStream and shut itself down.
        // This ensures that calling RemoveDiverter() here won't trigger the
        // controller to re-start the default AudioOutputStream and cause a
        // brief audio blip to come out the user's speakers.
        // http://crbug.com/474432
        //
        // It's fine if this task is canceled during shutdown, since the
        // mirroring manager doesn't require that all diverters are
        // removed.
        mirroring_manager->RemoveDiverter(controller.get());
      },
      mirroring_manager_, base::Passed(&controller_event_handler_),
      base::Passed(&reader_), controller_));
}

scoped_refptr<media::AudioOutputController>
AudioOutputDelegateImpl::GetController() const {
  return controller_;
}

int AudioOutputDelegateImpl::GetStreamId() const {
  return stream_id_;
}

void AudioOutputDelegateImpl::OnPlayStream() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller_->Play();
  audio_log_->OnStarted(stream_id_);
}

void AudioOutputDelegateImpl::OnPauseStream() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller_->Pause();
  audio_log_->OnStopped(stream_id_);
}

void AudioOutputDelegateImpl::OnSetVolume(double volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GE(volume, 0);
  DCHECK_LE(volume, 1);
  controller_->SetVolume(volume);
  audio_log_->OnSetVolume(stream_id_, volume);
}

void AudioOutputDelegateImpl::SendCreatedNotification() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  subscriber_->OnStreamCreated(stream_id_, reader_->shared_memory(),
                               reader_->foreign_socket());
}

void AudioOutputDelegateImpl::UpdatePlayingState(bool playing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (playing == playing_)
    return;

  playing_ = playing;
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

void AudioOutputDelegateImpl::OnError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  audio_log_->OnError(stream_id_);
  subscriber_->OnStreamError(stream_id_);
}

}  // namespace content
