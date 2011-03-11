// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/audio_renderer_host.h"

#include "base/metrics/histogram.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/renderer_host/audio_sync_reader.h"
#include "ipc/ipc_logging.h"

namespace {

// The minimum number of samples in a hardware packet.
// This value is selected so that we can handle down to 5khz sample rate.
const int kMinSamplesPerHardwarePacket = 1024;

// The maximum number of samples in a hardware packet.
// This value is selected so that we can handle up to 192khz sample rate.
const int kMaxSamplesPerHardwarePacket = 64 * 1024;

// This constant governs the hardware audio buffer size, this value should be
// chosen carefully.
// This value is selected so that we have 8192 samples for 48khz streams.
const int kMillisecondsPerHardwarePacket = 170;

uint32 SelectSamplesPerPacket(const AudioParameters& params) {
  // Select the number of samples that can provide at least
  // |kMillisecondsPerHardwarePacket| worth of audio data.
  int samples = kMinSamplesPerHardwarePacket;
  while (samples <= kMaxSamplesPerHardwarePacket &&
         samples * base::Time::kMillisecondsPerSecond <
         params.sample_rate * kMillisecondsPerHardwarePacket) {
    samples *= 2;
  }
  return samples;
}

}  // namespace

AudioRendererHost::AudioEntry::AudioEntry()
    : render_view_id(0),
      stream_id(0),
      pending_buffer_request(false),
      pending_close(false) {
}

AudioRendererHost::AudioEntry::~AudioEntry() {}

///////////////////////////////////////////////////////////////////////////////
// AudioRendererHost implementations.
AudioRendererHost::AudioRendererHost() {
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
      NewRunnableMethod(
          this,
          &AudioRendererHost::DoCompleteCreation,
          make_scoped_refptr(controller)));
}

void AudioRendererHost::OnPlaying(media::AudioOutputController* controller) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &AudioRendererHost::DoSendPlayingMessage,
          make_scoped_refptr(controller)));
}

void AudioRendererHost::OnPaused(media::AudioOutputController* controller) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &AudioRendererHost::DoSendPausedMessage,
          make_scoped_refptr(controller)));
}

void AudioRendererHost::OnError(media::AudioOutputController* controller,
                                int error_code) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(this,
                        &AudioRendererHost::DoHandleError,
                        make_scoped_refptr(controller),
                        error_code));
}

void AudioRendererHost::OnMoreData(media::AudioOutputController* controller,
                                   AudioBuffersState buffers_state) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(this,
                        &AudioRendererHost::DoRequestMoreData,
                        make_scoped_refptr(controller),
                        buffers_state));
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

  if (entry->controller->LowLatencyMode()) {
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

    Send(new ViewMsg_NotifyLowLatencyAudioStreamCreated(
        entry->render_view_id, entry->stream_id, foreign_memory_handle,
        foreign_socket_handle, entry->shared_memory.created_size()));
    return;
  }

  // The normal audio stream has created, send a message to the renderer
  // process.
  Send(new ViewMsg_NotifyAudioStreamCreated(
      entry->render_view_id, entry->stream_id, foreign_memory_handle,
      entry->shared_memory.created_size()));
}

void AudioRendererHost::DoSendPlayingMessage(
    media::AudioOutputController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupByController(controller);
  if (!entry)
    return;

  ViewMsg_AudioStreamState_Params params;
  params.state = ViewMsg_AudioStreamState_Params::kPlaying;
  Send(new ViewMsg_NotifyAudioStreamStateChanged(
      entry->render_view_id, entry->stream_id, params));
}

void AudioRendererHost::DoSendPausedMessage(
    media::AudioOutputController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupByController(controller);
  if (!entry)
    return;

  ViewMsg_AudioStreamState_Params params;
  params.state = ViewMsg_AudioStreamState_Params::kPaused;
  Send(new ViewMsg_NotifyAudioStreamStateChanged(
      entry->render_view_id, entry->stream_id, params));
}

void AudioRendererHost::DoRequestMoreData(
    media::AudioOutputController* controller,
    AudioBuffersState buffers_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If we already have a pending request then return.
  AudioEntry* entry = LookupByController(controller);
  if (!entry || entry->pending_buffer_request)
    return;

  DCHECK(!entry->controller->LowLatencyMode());
  entry->pending_buffer_request = true;
  Send(new ViewMsg_RequestAudioPacket(
      entry->render_view_id, entry->stream_id, buffers_state));
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateAudioStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PlayAudioStream, OnPlayStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PauseAudioStream, OnPauseStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FlushAudioStream, OnFlushStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CloseAudioStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_NotifyAudioPacketReady, OnNotifyPacketReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetAudioVolume, OnGetVolume)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetAudioVolume, OnSetVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void AudioRendererHost::OnCreateStream(
    const IPC::Message& msg, int stream_id,
    const ViewHostMsg_Audio_CreateStream_Params& params, bool low_latency) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(LookupById(msg.routing_id(), stream_id) == NULL);

  AudioParameters audio_params(params.params);

  // Select the hardware packet size if not specified.
  if (!audio_params.samples_per_packet) {
    audio_params.samples_per_packet = SelectSamplesPerPacket(audio_params);
  }
  uint32 packet_size = audio_params.GetPacketSize();

  scoped_ptr<AudioEntry> entry(new AudioEntry());
  // Create the shared memory and share with the renderer process.
  if (!entry->shared_memory.CreateAndMapAnonymous(packet_size)) {
    // If creation of shared memory failed then send an error message.
    SendErrorMessage(msg.routing_id(), stream_id);
    return;
  }

  if (low_latency) {
    // If this is the low latency mode, we need to construct a SyncReader first.
    scoped_ptr<AudioSyncReader> reader(
        new AudioSyncReader(&entry->shared_memory));

    // Then try to initialize the sync reader.
    if (!reader->Init()) {
      SendErrorMessage(msg.routing_id(), stream_id);
      return;
    }

    // If we have successfully created the SyncReader then assign it to the
    // entry and construct an AudioOutputController.
    entry->reader.reset(reader.release());
    entry->controller =
        media::AudioOutputController::CreateLowLatency(this, audio_params,
                                                       entry->reader.get());
  } else {
    // The choice of buffer capacity is based on experiment.
    entry->controller =
        media::AudioOutputController::Create(this, audio_params,
                                             3 * packet_size);
  }

  if (!entry->controller) {
    SendErrorMessage(msg.routing_id(), stream_id);
    return;
  }

  // If we have created the controller successfully create a entry and add it
  // to the map.
  entry->render_view_id = msg.routing_id();
  entry->stream_id = stream_id;

  audio_entries_.insert(std::make_pair(
      AudioEntryId(msg.routing_id(), stream_id),
      entry.release()));
}

void AudioRendererHost::OnPlayStream(const IPC::Message& msg, int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(msg.routing_id(), stream_id);
  if (!entry) {
    SendErrorMessage(msg.routing_id(), stream_id);
    return;
  }

  entry->controller->Play();
}

void AudioRendererHost::OnPauseStream(const IPC::Message& msg, int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(msg.routing_id(), stream_id);
  if (!entry) {
    SendErrorMessage(msg.routing_id(), stream_id);
    return;
  }

  entry->controller->Pause();
}

void AudioRendererHost::OnFlushStream(const IPC::Message& msg, int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(msg.routing_id(), stream_id);
  if (!entry) {
    SendErrorMessage(msg.routing_id(), stream_id);
    return;
  }

  entry->controller->Flush();
}

void AudioRendererHost::OnCloseStream(const IPC::Message& msg, int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(msg.routing_id(), stream_id);

  if (entry)
    CloseAndDeleteStream(entry);
}

void AudioRendererHost::OnSetVolume(const IPC::Message& msg, int stream_id,
                                    double volume) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(msg.routing_id(), stream_id);
  if (!entry) {
    SendErrorMessage(msg.routing_id(), stream_id);
    return;
  }

  // Make sure the volume is valid.
  if (volume < 0 || volume > 1.0)
    return;
  entry->controller->SetVolume(volume);
}

void AudioRendererHost::OnGetVolume(const IPC::Message& msg, int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  NOTREACHED() << "This message shouldn't be received";
}

void AudioRendererHost::OnNotifyPacketReady(
    const IPC::Message& msg, int stream_id, uint32 packet_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(msg.routing_id(), stream_id);
  if (!entry) {
    SendErrorMessage(msg.routing_id(), stream_id);
    return;
  }

  DCHECK(!entry->controller->LowLatencyMode());
  CHECK(packet_size <= entry->shared_memory.created_size());

  if (!entry->pending_buffer_request) {
    NOTREACHED() << "Buffer received but no such pending request";
  }
  entry->pending_buffer_request = false;

  // Enqueue the data to media::AudioOutputController.
  entry->controller->EnqueueData(
      reinterpret_cast<uint8*>(entry->shared_memory.memory()),
      packet_size);
}

void AudioRendererHost::SendErrorMessage(int32 render_view_id,
                                         int32 stream_id) {
  ViewMsg_AudioStreamState_Params state;
  state.state = ViewMsg_AudioStreamState_Params::kError;
  Send(new ViewMsg_NotifyAudioStreamStateChanged(
      render_view_id, stream_id, state));
}

void AudioRendererHost::DeleteEntries() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (AudioEntryMap::iterator i = audio_entries_.begin();
       i != audio_entries_.end(); ++i) {
    CloseAndDeleteStream(i->second);
  }
}

void AudioRendererHost::CloseAndDeleteStream(AudioEntry* entry) {
  if (!entry->pending_close) {
    entry->controller->Close(
        NewRunnableMethod(this, &AudioRendererHost::OnStreamClosed, entry));
    entry->pending_close = true;
  }
}

void AudioRendererHost::OnStreamClosed(AudioEntry* entry) {
  // Delete the entry after we've closed the stream.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &AudioRendererHost::DeleteEntry, entry));
}

void AudioRendererHost::DeleteEntry(AudioEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Delete the entry when this method goes out of scope.
  scoped_ptr<AudioEntry> entry_deleter(entry);

  // Erase the entry from the map.
  audio_entries_.erase(
      AudioEntryId(entry->render_view_id, entry->stream_id));
}

void AudioRendererHost::DeleteEntryOnError(AudioEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Sends the error message first before we close the stream because
  // |entry| is destroyed in DeleteEntry().
  SendErrorMessage(entry->render_view_id, entry->stream_id);
  CloseAndDeleteStream(entry);
}

AudioRendererHost::AudioEntry* AudioRendererHost::LookupById(
    int route_id, int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntryMap::iterator i = audio_entries_.find(
      AudioEntryId(route_id, stream_id));
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
