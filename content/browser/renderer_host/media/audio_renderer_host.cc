// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_renderer_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_mirroring_manager.h"
#include "content/browser/renderer_host/media/audio_sync_reader.h"
#include "content/common/media/audio_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_observer.h"
#include "media/audio/shared_memory_util.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"

using media::AudioBus;

namespace content {

class AudioRendererHost::AudioEntry
    : public media::AudioOutputController::EventHandler {
 public:
  AudioEntry(AudioRendererHost* host,
             int stream_id,
             int render_view_id,
             const media::AudioParameters& params,
             scoped_ptr<base::SharedMemory> shared_memory,
             scoped_ptr<media::AudioOutputController::SyncReader> reader);
  virtual ~AudioEntry();

  int stream_id() const {
    return stream_id_;
  }

  int render_view_id() const {
    return render_view_id_;
  }
  // TODO(miu): Remove this setter once my IPC clean-up change (in code review)
  // lands!
  void set_render_view_id(int render_view_id) {
    render_view_id_ = render_view_id;
  }

  media::AudioOutputController* controller() const {
    return controller_;
  }

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
  virtual void OnAudible(bool is_audible) OVERRIDE;
  virtual void OnPaused() OVERRIDE;
  virtual void OnError() OVERRIDE;
  virtual void OnDeviceChange(int new_buffer_size, int new_sample_rate)
      OVERRIDE;

  AudioRendererHost* const host_;
  const int stream_id_;

  // The routing ID of the source render view.
  int render_view_id_;

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
    scoped_ptr<base::SharedMemory> shared_memory,
    scoped_ptr<media::AudioOutputController::SyncReader> reader)
    : host_(host),
      stream_id_(stream_id),
      render_view_id_(render_view_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          controller_(media::AudioOutputController::Create(
              host->audio_manager_, this, params, reader.get()))),
      shared_memory_(shared_memory.Pass()),
      reader_(reader.Pass()) {
  DCHECK(controller_);
}

AudioRendererHost::AudioEntry::~AudioEntry() {}

///////////////////////////////////////////////////////////////////////////////
// AudioRendererHost implementations.
AudioRendererHost::AudioRendererHost(
    int render_process_id,
    media::AudioManager* audio_manager,
    AudioMirroringManager* mirroring_manager,
    MediaInternals* media_internals)
    : render_process_id_(render_process_id),
      audio_manager_(audio_manager),
      mirroring_manager_(mirroring_manager),
      media_internals_(media_internals) {
  DCHECK(audio_manager_);
}

AudioRendererHost::~AudioRendererHost() {
  DCHECK(audio_entries_.empty());
}

void AudioRendererHost::OnChannelClosing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserMessageFilter::OnChannelClosing();

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
      base::Bind(&AudioRendererHost::DoCompleteCreation, host_, this));
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

void AudioRendererHost::AudioEntry::OnAudible(bool is_audible) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::DoNotifyAudibleState, host_,
                 this, is_audible));
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

void AudioRendererHost::DoCompleteCreation(AudioEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!peer_handle()) {
    NOTREACHED() << "Renderer process handle is invalid.";
    ReportErrorAndClose(entry->stream_id());
    return;
  }

  // Once the audio stream is created then complete the creation process by
  // mapping shared memory and sharing with the renderer process.
  base::SharedMemoryHandle foreign_memory_handle;
  if (!entry->shared_memory()->ShareToProcess(peer_handle(),
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
  if (!reader->PrepareForeignSocketHandle(peer_handle(),
                                          &foreign_socket_handle)) {
    ReportErrorAndClose(entry->stream_id());
    return;
  }

  Send(new AudioMsg_NotifyStreamCreated(
      entry->stream_id(),
      foreign_memory_handle,
      foreign_socket_handle,
      media::PacketSizeInBytes(entry->shared_memory()->requested_size())));
}

void AudioRendererHost::DoNotifyAudibleState(AudioEntry* entry,
                                             bool is_audible) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  MediaObserver* const media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer) {
    DVLOG(1) << "AudioRendererHost@" << this
             << "::DoNotifyAudibleState(is_audible=" << is_audible
             << ") for stream_id=" << entry->stream_id();

    media_observer->OnAudioStreamPlayingChanged(
        render_process_id_, entry->render_view_id(), entry->stream_id(),
        is_audible);
  }
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler
bool AudioRendererHost::OnMessageReceived(const IPC::Message& message,
                                          bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AudioRendererHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CreateStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_AssociateStreamWithProducer,
                        OnAssociateStreamWithProducer)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PlayStream, OnPlayStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PauseStream, OnPauseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_FlushStream, OnFlushStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CloseStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_SetVolume, OnSetVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void AudioRendererHost::OnCreateStream(
    int stream_id, const media::AudioParameters& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // media::AudioParameters is validated in the deserializer.
  int input_channels = params.input_channels();
  if (input_channels < 0 ||
      input_channels > media::limits::kMaxChannels ||
      LookupById(stream_id) != NULL) {
    SendErrorMessage(stream_id);
    return;
  }

  // Calculate output and input memory size.
  int output_memory_size = AudioBus::CalculateMemorySize(params);
  int frames = params.frames_per_buffer();
  int input_memory_size =
      AudioBus::CalculateMemorySize(input_channels, frames);

  // Create the shared memory and share with the renderer process.
  // For synchronized I/O (if input_channels > 0) then we allocate
  // extra memory after the output data for the input data.
  uint32 io_buffer_size = output_memory_size + input_memory_size;
  uint32 shared_memory_size =
      media::TotalSharedMemorySizeInBytes(io_buffer_size);
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAndMapAnonymous(shared_memory_size)) {
    SendErrorMessage(stream_id);
    return;
  }

  scoped_ptr<AudioSyncReader> reader(
      new AudioSyncReader(shared_memory.get(), params, input_channels));
  if (!reader->Init()) {
    SendErrorMessage(stream_id);
    return;
  }

  audio_entries_.insert(std::make_pair(stream_id, new AudioEntry(
      this, stream_id, MSG_ROUTING_NONE, params, shared_memory.Pass(),
      reader.PassAs<media::AudioOutputController::SyncReader>())));

  if (media_internals_)
    media_internals_->OnSetAudioStreamStatus(this, stream_id, "created");
}

void AudioRendererHost::OnAssociateStreamWithProducer(int stream_id,
                                                      int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DVLOG(1) << "AudioRendererHost@" << this
           << "::OnAssociateStreamWithProducer(stream_id=" << stream_id
           << ", render_view_id=" << render_view_id << ")";

  AudioEntry* const entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  if (entry->render_view_id() == render_view_id)
    return;

  // TODO(miu): Merge "AssociateWithProducer" message into "CreateStream"
  // message so AudioRendererHost can assume a simpler "render_view_id is set
  // once" scheme. http://crbug.com/166779
  if (mirroring_manager_) {
    mirroring_manager_->RemoveDiverter(
        render_process_id_, entry->render_view_id(), entry->controller());
  }
  entry->set_render_view_id(render_view_id);
  if (mirroring_manager_) {
    mirroring_manager_->AddDiverter(
        render_process_id_, entry->render_view_id(), entry->controller());
  }
}

void AudioRendererHost::OnPlayStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller()->Play();
  if (media_internals_)
    media_internals_->OnSetAudioStreamPlaying(this, stream_id, true);
}

void AudioRendererHost::OnPauseStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller()->Pause();
  if (media_internals_)
    media_internals_->OnSetAudioStreamPlaying(this, stream_id, false);
}

void AudioRendererHost::OnFlushStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller()->Flush();
  if (media_internals_)
    media_internals_->OnSetAudioStreamStatus(this, stream_id, "flushed");
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
  if (media_internals_)
    media_internals_->OnSetAudioStreamVolume(this, stream_id, volume);
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

  if (media_internals_)
    media_internals_->OnSetAudioStreamStatus(this, stream_id, "closed");
}

void AudioRendererHost::DeleteEntry(scoped_ptr<AudioEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // At this point, make the final "say" in audio playback state.
  MediaObserver* const media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer) {
    media_observer->OnAudioStreamPlayingChanged(
        render_process_id_, entry->render_view_id(), entry->stream_id(), false);
  }

  // Notify the media observer.
  if (media_internals_)
    media_internals_->OnDeleteAudioStream(this, entry->stream_id());

  // Note: |entry| will be deleted upon leaving this scope.
}

void AudioRendererHost::ReportErrorAndClose(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Make sure this isn't a stray callback executing after the stream has been
  // closed, so error notifications aren't sent after clients believe the stream
  // is closed.
  if (!LookupById(stream_id))
    return;

  SendErrorMessage(stream_id);

  if (media_internals_)
    media_internals_->OnSetAudioStreamStatus(this, stream_id, "error");

  OnCloseStream(stream_id);
}

AudioRendererHost::AudioEntry* AudioRendererHost::LookupById(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntryMap::const_iterator i = audio_entries_.find(stream_id);
  return i != audio_entries_.end() ? i->second : NULL;
}

}  // namespace content
