// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_renderer_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/process/process.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_mirroring_manager.h"
#include "content/browser/renderer_host/media/audio_sync_reader.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/audio_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_observer.h"
#include "content/public/common/content_switches.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"

using media::AudioBus;
using media::AudioManager;

namespace content {

class AudioRendererHost::AudioEntry
    : public media::AudioOutputController::EventHandler {
 public:
  AudioEntry(AudioRendererHost* host,
             int stream_id,
             int render_view_id,
             const media::AudioParameters& params,
             const std::string& output_device_id,
             scoped_ptr<base::SharedMemory> shared_memory,
             scoped_ptr<media::AudioOutputController::SyncReader> reader);
  virtual ~AudioEntry();

  int stream_id() const {
    return stream_id_;
  }

  int render_view_id() const {
    return render_view_id_;
  }

  media::AudioOutputController* controller() const { return controller_.get(); }

  base::SharedMemory* shared_memory() {
    return shared_memory_.get();
  }

  media::AudioOutputController::SyncReader* reader() const {
    return reader_.get();
  }

 private:
  // media::AudioOutputController::EventHandler implementation.
  virtual void OnCreated() OVERRIDE;
  virtual void OnPlaying() OVERRIDE;
  virtual void OnPowerMeasured(float power_dbfs, bool clipped) OVERRIDE;
  virtual void OnPaused() OVERRIDE;
  virtual void OnError() OVERRIDE;
  virtual void OnDeviceChange(int new_buffer_size, int new_sample_rate)
      OVERRIDE;

  AudioRendererHost* const host_;
  const int stream_id_;

  // The routing ID of the source render view.
  const int render_view_id_;

  // The AudioOutputController that manages the audio stream.
  const scoped_refptr<media::AudioOutputController> controller_;

  // Shared memory for transmission of the audio data.
  const scoped_ptr<base::SharedMemory> shared_memory_;

  // The synchronous reader to be used by the controller.
  const scoped_ptr<media::AudioOutputController::SyncReader> reader_;
};

AudioRendererHost::AudioEntry::AudioEntry(
    AudioRendererHost* host, int stream_id, int render_view_id,
    const media::AudioParameters& params,
    const std::string& output_device_id,
    scoped_ptr<base::SharedMemory> shared_memory,
    scoped_ptr<media::AudioOutputController::SyncReader> reader)
    : host_(host),
      stream_id_(stream_id),
      render_view_id_(render_view_id),
      controller_(media::AudioOutputController::Create(
          host->audio_manager_, this, params, output_device_id, reader.get())),
      shared_memory_(shared_memory.Pass()),
      reader_(reader.Pass()) {
  DCHECK(controller_.get());
}

AudioRendererHost::AudioEntry::~AudioEntry() {}

///////////////////////////////////////////////////////////////////////////////
// AudioRendererHost implementations.

AudioRendererHost::AudioRendererHost(
    int render_process_id,
    media::AudioManager* audio_manager,
    AudioMirroringManager* mirroring_manager,
    MediaInternals* media_internals,
    MediaStreamManager* media_stream_manager)
    : BrowserMessageFilter(AudioMsgStart),
      render_process_id_(render_process_id),
      audio_manager_(audio_manager),
      mirroring_manager_(mirroring_manager),
      audio_log_(media_internals->CreateAudioLog(
          media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER)),
      media_stream_manager_(media_stream_manager) {
  DCHECK(audio_manager_);
  DCHECK(media_stream_manager_);
}

AudioRendererHost::~AudioRendererHost() {
  DCHECK(audio_entries_.empty());
}

void AudioRendererHost::GetOutputControllers(
    int render_view_id,
    const RenderViewHost::GetAudioOutputControllersCallback& callback) const {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::DoGetOutputControllers, this,
                 render_view_id),
      callback);
}

void AudioRendererHost::OnChannelClosing() {
  // Since the IPC channel is gone, close all requested audio streams.
  while (!audio_entries_.empty()) {
    // Note: OnCloseStream() removes the entries from audio_entries_.
    OnCloseStream(audio_entries_.begin()->first);
  }
}

void AudioRendererHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void AudioRendererHost::AudioEntry::OnCreated() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::DoCompleteCreation, host_, stream_id_));
}

void AudioRendererHost::AudioEntry::OnPlaying() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&AudioRendererHost::Send), host_,
          new AudioMsg_NotifyStreamStateChanged(
              stream_id_, media::AudioOutputIPCDelegate::kPlaying)));
}

void AudioRendererHost::AudioEntry::OnPowerMeasured(float power_dbfs,
                                                    bool clipped) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::DoNotifyAudioPowerLevel, host_,
                 stream_id_, power_dbfs, clipped));
}

void AudioRendererHost::AudioEntry::OnPaused() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&AudioRendererHost::Send), host_,
          new AudioMsg_NotifyStreamStateChanged(
              stream_id_, media::AudioOutputIPCDelegate::kPaused)));
}

void AudioRendererHost::AudioEntry::OnError() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::ReportErrorAndClose, host_, stream_id_));
}

void AudioRendererHost::AudioEntry::OnDeviceChange(int new_buffer_size,
                                                   int new_sample_rate) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&AudioRendererHost::Send), host_,
                 new AudioMsg_NotifyDeviceChanged(
                     stream_id_, new_buffer_size, new_sample_rate)));
}

void AudioRendererHost::DoCompleteCreation(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!PeerHandle()) {
    DLOG(WARNING) << "Renderer process handle is invalid.";
    ReportErrorAndClose(stream_id);
    return;
  }

  AudioEntry* const entry = LookupById(stream_id);
  if (!entry) {
    ReportErrorAndClose(stream_id);
    return;
  }

  // Once the audio stream is created then complete the creation process by
  // mapping shared memory and sharing with the renderer process.
  base::SharedMemoryHandle foreign_memory_handle;
  if (!entry->shared_memory()->ShareToProcess(PeerHandle(),
                                              &foreign_memory_handle)) {
    // If we failed to map and share the shared memory then close the audio
    // stream and send an error message.
    ReportErrorAndClose(entry->stream_id());
    return;
  }

  AudioSyncReader* reader = static_cast<AudioSyncReader*>(entry->reader());

#if defined(OS_WIN)
  base::SyncSocket::Handle foreign_socket_handle;
#else
  base::FileDescriptor foreign_socket_handle;
#endif

  // If we failed to prepare the sync socket for the renderer then we fail
  // the construction of audio stream.
  if (!reader->PrepareForeignSocketHandle(PeerHandle(),
                                          &foreign_socket_handle)) {
    ReportErrorAndClose(entry->stream_id());
    return;
  }

  Send(new AudioMsg_NotifyStreamCreated(
      entry->stream_id(),
      foreign_memory_handle,
      foreign_socket_handle,
      entry->shared_memory()->requested_size()));
}

RenderViewHost::AudioOutputControllerList
AudioRendererHost::DoGetOutputControllers(int render_view_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  RenderViewHost::AudioOutputControllerList controllers;
  AudioEntryMap::const_iterator it = audio_entries_.begin();
  for (; it != audio_entries_.end(); ++it) {
    AudioEntry* entry = it->second;
    if (entry->render_view_id() == render_view_id)
      controllers.push_back(entry->controller());
  }

  return controllers;
}

void AudioRendererHost::DoNotifyAudioPowerLevel(int stream_id,
                                                float power_dbfs,
                                                bool clipped) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  MediaObserver* const media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer) {
    AudioEntry* const entry = LookupById(stream_id);
    if (entry) {
      media_observer->OnAudioStreamPlayingChanged(
          render_process_id_, entry->render_view_id(), entry->stream_id(),
          true, power_dbfs, clipped);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler
bool AudioRendererHost::OnMessageReceived(const IPC::Message& message,
                                          bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AudioRendererHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CreateStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PlayStream, OnPlayStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PauseStream, OnPauseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CloseStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_SetVolume, OnSetVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void AudioRendererHost::OnCreateStream(
    int stream_id, int render_view_id, int render_frame_id, int session_id,
    const media::AudioParameters& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DVLOG(1) << "AudioRendererHost@" << this
           << "::OnCreateStream(stream_id=" << stream_id
           << ", render_view_id=" << render_view_id
           << ", session_id=" << session_id << ")";
  DCHECK_GT(render_view_id, 0);

  // media::AudioParameters is validated in the deserializer.
  if (LookupById(stream_id) != NULL) {
    SendErrorMessage(stream_id);
    return;
  }

  // Initialize the |output_device_id| to an empty string which indicates that
  // the default device should be used. If a StreamDeviceInfo instance was found
  // though, then we use the matched output device.
  std::string output_device_id;
  const StreamDeviceInfo* info = media_stream_manager_->
      audio_input_device_manager()->GetOpenedDeviceInfoById(session_id);
  if (info)
    output_device_id = info->device.matched_output_device_id;

  // Create the shared memory and share with the renderer process.
  // For synchronized I/O (if input_channels > 0) then we allocate
  // extra memory after the output data for the input data.
  uint32 shared_memory_size = AudioBus::CalculateMemorySize(params);;
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAndMapAnonymous(shared_memory_size)) {
    SendErrorMessage(stream_id);
    return;
  }

  scoped_ptr<AudioSyncReader> reader(
      new AudioSyncReader(shared_memory.get(), params));
  if (!reader->Init()) {
    SendErrorMessage(stream_id);
    return;
  }

  MediaObserver* const media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer)
    media_observer->OnCreatingAudioStream(render_process_id_, render_frame_id);

  scoped_ptr<AudioEntry> entry(new AudioEntry(
      this, stream_id, render_view_id, params, output_device_id,
      shared_memory.Pass(),
      reader.PassAs<media::AudioOutputController::SyncReader>()));
  if (mirroring_manager_) {
    mirroring_manager_->AddDiverter(
        render_process_id_, entry->render_view_id(), entry->controller());
  }
  audio_entries_.insert(std::make_pair(stream_id, entry.release()));
  audio_log_->OnCreated(stream_id, params, output_device_id);
}

void AudioRendererHost::OnPlayStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller()->Play();
  audio_log_->OnStarted(stream_id);
}

void AudioRendererHost::OnPauseStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller()->Pause();
  audio_log_->OnStopped(stream_id);
}

void AudioRendererHost::OnSetVolume(int stream_id, double volume) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  // Make sure the volume is valid.
  if (volume < 0 || volume > 1.0)
    return;
  entry->controller()->SetVolume(volume);
  audio_log_->OnSetVolume(stream_id, volume);
}

void AudioRendererHost::SendErrorMessage(int stream_id) {
  Send(new AudioMsg_NotifyStreamStateChanged(
      stream_id, media::AudioOutputIPCDelegate::kError));
}

void AudioRendererHost::OnCloseStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Prevent oustanding callbacks from attempting to close/delete the same
  // AudioEntry twice.
  AudioEntryMap::iterator i = audio_entries_.find(stream_id);
  if (i == audio_entries_.end())
    return;
  scoped_ptr<AudioEntry> entry(i->second);
  audio_entries_.erase(i);

  media::AudioOutputController* const controller = entry->controller();
  if (mirroring_manager_) {
    mirroring_manager_->RemoveDiverter(
        render_process_id_, entry->render_view_id(), controller);
  }
  controller->Close(
      base::Bind(&AudioRendererHost::DeleteEntry, this, base::Passed(&entry)));
  audio_log_->OnClosed(stream_id);
}

void AudioRendererHost::DeleteEntry(scoped_ptr<AudioEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // At this point, make the final "say" in audio playback state.
  MediaObserver* const media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer) {
    media_observer->OnAudioStreamPlayingChanged(
        render_process_id_, entry->render_view_id(), entry->stream_id(),
        false, -std::numeric_limits<float>::infinity(), false);
  }
}

void AudioRendererHost::ReportErrorAndClose(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Make sure this isn't a stray callback executing after the stream has been
  // closed, so error notifications aren't sent after clients believe the stream
  // is closed.
  if (!LookupById(stream_id))
    return;

  SendErrorMessage(stream_id);

  audio_log_->OnError(stream_id);
  OnCloseStream(stream_id);
}

AudioRendererHost::AudioEntry* AudioRendererHost::LookupById(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntryMap::const_iterator i = audio_entries_.find(stream_id);
  return i != audio_entries_.end() ? i->second : NULL;
}

}  // namespace content
