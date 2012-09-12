// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_renderer_host.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/audio_sync_reader.h"
#include "content/common/media/audio_messages.h"
#include "content/public/browser/media_observer.h"
#include "media/audio/shared_memory_util.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"

using content::BrowserMessageFilter;
using content::BrowserThread;
using media::AudioBus;

AudioRendererHost::AudioEntry::AudioEntry()
    : stream_id(0),
      pending_close(false) {
}

AudioRendererHost::AudioEntry::~AudioEntry() {}

///////////////////////////////////////////////////////////////////////////////
// AudioRendererHost implementations.
AudioRendererHost::AudioRendererHost(
    media::AudioManager* audio_manager,
    content::MediaObserver* media_observer)
    : audio_manager_(audio_manager),
      media_observer_(media_observer) {
}

AudioRendererHost::~AudioRendererHost() {
  DCHECK(audio_entries_.empty());
}

void AudioRendererHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Since the IPC channel is gone, close all requested audio streams.
  DeleteEntries();
}

void AudioRendererHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

///////////////////////////////////////////////////////////////////////////////
// media::AudioOutputController::EventHandler implementations.
void AudioRendererHost::OnCreated(media::AudioOutputController* controller) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &AudioRendererHost::DoCompleteCreation,
          this,
          make_scoped_refptr(controller)));
}

void AudioRendererHost::OnPlaying(media::AudioOutputController* controller) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &AudioRendererHost::DoSendPlayingMessage,
          this,
          make_scoped_refptr(controller)));
}

void AudioRendererHost::OnPaused(media::AudioOutputController* controller) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &AudioRendererHost::DoSendPausedMessage,
          this,
          make_scoped_refptr(controller)));
}

void AudioRendererHost::OnError(media::AudioOutputController* controller,
                                int error_code) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::DoHandleError,
                 this, make_scoped_refptr(controller), error_code));
}

void AudioRendererHost::DoCompleteCreation(
    media::AudioOutputController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupByController(controller);
  if (!entry)
    return;

  if (!peer_handle()) {
    NOTREACHED() << "Renderer process handle is invalid.";
    DeleteEntryOnError(entry);
    return;
  }

  // Once the audio stream is created then complete the creation process by
  // mapping shared memory and sharing with the renderer process.
  base::SharedMemoryHandle foreign_memory_handle;
  if (!entry->shared_memory.ShareToProcess(peer_handle(),
                                           &foreign_memory_handle)) {
    // If we failed to map and share the shared memory then close the audio
    // stream and send an error message.
    DeleteEntryOnError(entry);
    return;
  }

  AudioSyncReader* reader =
      static_cast<AudioSyncReader*>(entry->reader.get());

#if defined(OS_WIN)
  base::SyncSocket::Handle foreign_socket_handle;
#else
  base::FileDescriptor foreign_socket_handle;
#endif

  // If we failed to prepare the sync socket for the renderer then we fail
  // the construction of audio stream.
  if (!reader->PrepareForeignSocketHandle(peer_handle(),
                                          &foreign_socket_handle)) {
    DeleteEntryOnError(entry);
    return;
  }

  Send(new AudioMsg_NotifyStreamCreated(
      entry->stream_id,
      foreign_memory_handle,
      foreign_socket_handle,
      media::PacketSizeInBytes(entry->shared_memory.created_size())));
}

void AudioRendererHost::DoSendPlayingMessage(
    media::AudioOutputController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupByController(controller);
  if (!entry)
    return;

  Send(new AudioMsg_NotifyStreamStateChanged(
      entry->stream_id, media::AudioOutputIPCDelegate::kPlaying));
}

void AudioRendererHost::DoSendPausedMessage(
    media::AudioOutputController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupByController(controller);
  if (!entry)
    return;

  Send(new AudioMsg_NotifyStreamStateChanged(
      entry->stream_id, media::AudioOutputIPCDelegate::kPaused));
}

void AudioRendererHost::DoHandleError(media::AudioOutputController* controller,
                                      int error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupByController(controller);
  if (!entry)
    return;

  DeleteEntryOnError(entry);
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
    IPC_MESSAGE_HANDLER(AudioHostMsg_FlushStream, OnFlushStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CloseStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_SetVolume, OnSetVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void AudioRendererHost::OnCreateStream(
    int stream_id, const media::AudioParameters& params, int input_channels) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(LookupById(stream_id) == NULL);

  media::AudioParameters audio_params(params);
  uint32 buffer_size = media::AudioBus::CalculateMemorySize(audio_params);
  DCHECK_GT(buffer_size, 0U);
  DCHECK_LE(buffer_size,
            static_cast<uint32>(media::limits::kMaxPacketSizeInBytes));

  DCHECK_GE(input_channels, 0);
  DCHECK_LT(input_channels, media::limits::kMaxChannels);

  // Calculate output and input memory size.
  int output_memory_size = AudioBus::CalculateMemorySize(audio_params);
  DCHECK_GT(output_memory_size, 0);

  int frames = audio_params.frames_per_buffer();
  int input_memory_size =
      AudioBus::CalculateMemorySize(input_channels, frames);

  DCHECK_GE(input_memory_size, 0);

  scoped_ptr<AudioEntry> entry(new AudioEntry());

  // Create the shared memory and share with the renderer process.
  // For synchronized I/O (if input_channels > 0) then we allocate
  // extra memory after the output data for the input data.
  uint32 io_buffer_size = output_memory_size + input_memory_size;

  uint32 shared_memory_size =
      media::TotalSharedMemorySizeInBytes(io_buffer_size);
  if (!entry->shared_memory.CreateAndMapAnonymous(shared_memory_size)) {
    // If creation of shared memory failed then send an error message.
    SendErrorMessage(stream_id);
    return;
  }

  // Create sync reader and try to initialize it.
  scoped_ptr<AudioSyncReader> reader(
      new AudioSyncReader(&entry->shared_memory, params, input_channels));

  if (!reader->Init()) {
    SendErrorMessage(stream_id);
    return;
  }

  // If we have successfully created the SyncReader then assign it to the
  // entry and construct an AudioOutputController.
  entry->reader.reset(reader.release());
  entry->controller = media::AudioOutputController::Create(
      audio_manager_, this, audio_params, entry->reader.get());

  if (!entry->controller) {
    SendErrorMessage(stream_id);
    return;
  }

  // If we have created the controller successfully, create an entry and add it
  // to the map.
  entry->stream_id = stream_id;
  audio_entries_.insert(std::make_pair(stream_id, entry.release()));
  if (media_observer_)
    media_observer_->OnSetAudioStreamStatus(this, stream_id, "created");
}

void AudioRendererHost::OnPlayStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller->Play();
  if (media_observer_)
    media_observer_->OnSetAudioStreamPlaying(this, stream_id, true);
}

void AudioRendererHost::OnPauseStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller->Pause();
  if (media_observer_)
    media_observer_->OnSetAudioStreamPlaying(this, stream_id, false);
}

void AudioRendererHost::OnFlushStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller->Flush();
  if (media_observer_)
    media_observer_->OnSetAudioStreamStatus(this, stream_id, "flushed");
}

void AudioRendererHost::OnCloseStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (media_observer_)
    media_observer_->OnSetAudioStreamStatus(this, stream_id, "closed");

  AudioEntry* entry = LookupById(stream_id);

  if (entry)
    CloseAndDeleteStream(entry);
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
  entry->controller->SetVolume(volume);
  if (media_observer_)
    media_observer_->OnSetAudioStreamVolume(this, stream_id, volume);
}

void AudioRendererHost::SendErrorMessage(int32 stream_id) {
  Send(new AudioMsg_NotifyStreamStateChanged(
      stream_id, media::AudioOutputIPCDelegate::kError));
}

void AudioRendererHost::DeleteEntries() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (AudioEntryMap::iterator i = audio_entries_.begin();
       i != audio_entries_.end(); ++i) {
    CloseAndDeleteStream(i->second);
  }
}

void AudioRendererHost::CloseAndDeleteStream(AudioEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!entry->pending_close) {
    entry->controller->Close(
        base::Bind(&AudioRendererHost::DeleteEntry, this, entry));
    entry->pending_close = true;
  }
}

void AudioRendererHost::DeleteEntry(AudioEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Delete the entry when this method goes out of scope.
  scoped_ptr<AudioEntry> entry_deleter(entry);

  // Erase the entry identified by |stream_id| from the map.
  audio_entries_.erase(entry->stream_id);

  // Notify the media observer.
  if (media_observer_)
    media_observer_->OnDeleteAudioStream(this, entry->stream_id);
}

void AudioRendererHost::DeleteEntryOnError(AudioEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Sends the error message first before we close the stream because
  // |entry| is destroyed in DeleteEntry().
  SendErrorMessage(entry->stream_id);

  if (media_observer_)
    media_observer_->OnSetAudioStreamStatus(this, entry->stream_id, "error");
  CloseAndDeleteStream(entry);
}

AudioRendererHost::AudioEntry* AudioRendererHost::LookupById(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntryMap::iterator i = audio_entries_.find(stream_id);
  if (i != audio_entries_.end() && !i->second->pending_close)
    return i->second;
  return NULL;
}

AudioRendererHost::AudioEntry* AudioRendererHost::LookupByController(
    media::AudioOutputController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Iterate the map of entries.
  // TODO(hclam): Implement a faster look up method.
  for (AudioEntryMap::iterator i = audio_entries_.begin();
       i != audio_entries_.end(); ++i) {
    if (!i->second->pending_close && controller == i->second->controller.get())
      return i->second;
  }
  return NULL;
}
