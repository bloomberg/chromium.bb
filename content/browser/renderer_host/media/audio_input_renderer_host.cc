// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_renderer_host.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_input_sync_writer.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/web_contents_audio_input_stream.h"
#include "content/browser/renderer_host/media/web_contents_capture_util.h"
#include "content/common/media/audio_messages.h"

namespace content {

struct AudioInputRendererHost::AudioEntry {
  AudioEntry();
  ~AudioEntry();

  // The AudioInputController that manages the audio input stream.
  scoped_refptr<media::AudioInputController> controller;

  // The audio input stream ID in the render view.
  int stream_id;

  // Shared memory for transmission of the audio data.
  base::SharedMemory shared_memory;

  // The synchronous writer to be used by the controller. We have the
  // ownership of the writer.
  scoped_ptr<media::AudioInputController::SyncWriter> writer;

  // Set to true after we called Close() for the controller.
  bool pending_close;
};

AudioInputRendererHost::AudioEntry::AudioEntry()
    : stream_id(0),
      pending_close(false) {
}

AudioInputRendererHost::AudioEntry::~AudioEntry() {}

AudioInputRendererHost::AudioInputRendererHost(
    media::AudioManager* audio_manager,
    MediaStreamManager* media_stream_manager)
    : audio_manager_(audio_manager),
      media_stream_manager_(media_stream_manager) {
}

AudioInputRendererHost::~AudioInputRendererHost() {
  DCHECK(audio_entries_.empty());
}

void AudioInputRendererHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Since the IPC channel is gone, close all requested audio streams.
  DeleteEntries();
}

void AudioInputRendererHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void AudioInputRendererHost::OnCreated(
    media::AudioInputController* controller) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &AudioInputRendererHost::DoCompleteCreation,
          this,
          make_scoped_refptr(controller)));
}

void AudioInputRendererHost::OnRecording(
    media::AudioInputController* controller) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &AudioInputRendererHost::DoSendRecordingMessage,
          this,
          make_scoped_refptr(controller)));
}

void AudioInputRendererHost::OnError(
    media::AudioInputController* controller,
    int error_code) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioInputRendererHost::DoHandleError,
          this, make_scoped_refptr(controller), error_code));
}

void AudioInputRendererHost::OnData(media::AudioInputController* controller,
                                    const uint8* data,
                                    uint32 size) {
  NOTREACHED() << "Only low-latency mode is supported.";
}

void AudioInputRendererHost::DoCompleteCreation(
    media::AudioInputController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupByController(controller);
  if (!entry)
    return;

  if (!peer_handle()) {
    NOTREACHED() << "Renderer process handle is invalid.";
    DeleteEntryOnError(entry);
    return;
  }

  if (!entry->controller->LowLatencyMode()) {
    NOTREACHED() << "Only low-latency mode is supported.";
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

  AudioInputSyncWriter* writer =
      static_cast<AudioInputSyncWriter*>(entry->writer.get());

#if defined(OS_WIN)
  base::SyncSocket::Handle foreign_socket_handle;
#else
  base::FileDescriptor foreign_socket_handle;
#endif

  // If we failed to prepare the sync socket for the renderer then we fail
  // the construction of audio input stream.
  if (!writer->PrepareForeignSocketHandle(peer_handle(),
                                          &foreign_socket_handle)) {
    DeleteEntryOnError(entry);
    return;
  }

  Send(new AudioInputMsg_NotifyStreamCreated(entry->stream_id,
      foreign_memory_handle, foreign_socket_handle,
      entry->shared_memory.created_size()));
}

void AudioInputRendererHost::DoSendRecordingMessage(
    media::AudioInputController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // TODO(henrika): See crbug.com/115262 for details on why this method
  // should be implemented.
}

void AudioInputRendererHost::DoHandleError(
    media::AudioInputController* controller,
    int error_code) {
  DLOG(WARNING) << "AudioInputRendererHost::DoHandleError(error_code="
                << error_code << ")";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupByController(controller);
  if (!entry)
    return;

  DeleteEntryOnError(entry);
}

bool AudioInputRendererHost::OnMessageReceived(const IPC::Message& message,
                                               bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AudioInputRendererHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_StartDevice, OnStartDevice)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_CreateStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_AssociateStreamWithConsumer,
                        OnAssociateStreamWithConsumer)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_RecordStream, OnRecordStream)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_CloseStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_SetVolume, OnSetVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void AudioInputRendererHost::OnStartDevice(int stream_id, int session_id) {
  VLOG(1) << "AudioInputRendererHost::OnStartDevice(stream_id="
          << stream_id << ", session_id = " << session_id << ")";

  // Add the session entry to the map.
  session_entries_[session_id] = stream_id;

  // Start the device with the session_id. If the device is started
  // successfully, OnDeviceStarted() callback will be triggered.
  media_stream_manager_->audio_input_device_manager()->Start(session_id, this);
}

void AudioInputRendererHost::OnCreateStream(
    int stream_id, const media::AudioParameters& params,
    const std::string& device_id, bool automatic_gain_control) {
  VLOG(1) << "AudioInputRendererHost::OnCreateStream(stream_id="
          << stream_id << ")";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // media::AudioParameters is validated in the deserializer.
  if (LookupById(stream_id) != NULL) {
    SendErrorMessage(stream_id);
    return;
  }

  media::AudioParameters audio_params(params);

  if (media_stream_manager_->audio_input_device_manager()->
      ShouldUseFakeDevice()) {
    audio_params.Reset(media::AudioParameters::AUDIO_FAKE,
                       params.channel_layout(), params.sample_rate(),
                       params.bits_per_sample(), params.frames_per_buffer());
  }

  uint32 buffer_size = audio_params.GetBytesPerBuffer();

  // Create a new AudioEntry structure.
  scoped_ptr<AudioEntry> entry(new AudioEntry());

  uint32 mem_size = sizeof(media::AudioInputBufferParameters) + buffer_size;

  // Create the shared memory and share it with the renderer process
  // using a new SyncWriter object.
  if (!entry->shared_memory.CreateAndMapAnonymous(mem_size)) {
    // If creation of shared memory failed then send an error message.
    SendErrorMessage(stream_id);
    return;
  }

  scoped_ptr<AudioInputSyncWriter> writer(
      new AudioInputSyncWriter(&entry->shared_memory));

  if (!writer->Init()) {
    SendErrorMessage(stream_id);
    return;
  }

  // If we have successfully created the SyncWriter then assign it to the
  // entry and construct an AudioInputController.
  entry->writer.reset(writer.release());
  if (WebContentsCaptureUtil::IsWebContentsDeviceId(device_id)) {
    entry->controller = media::AudioInputController::CreateForStream(
        audio_manager_,
        this,
        WebContentsAudioInputStream::Create(
            device_id, audio_params, audio_manager_->GetMessageLoop()),
        entry->writer.get());
  } else {
    // TODO(henrika): replace CreateLowLatency() with Create() as soon
    // as satish has ensured that Speech Input also uses the default low-
    // latency path. See crbug.com/112472 for details.
    entry->controller = media::AudioInputController::CreateLowLatency(
        audio_manager_,
        this,
        audio_params,
        device_id,
        entry->writer.get());
  }

  if (!entry->controller) {
    SendErrorMessage(stream_id);
    return;
  }

  // Set the initial AGC state for the audio input stream. Note that, the AGC
  // is only supported in AUDIO_PCM_LOW_LATENCY mode.
  if (params.format() == media::AudioParameters::AUDIO_PCM_LOW_LATENCY)
    entry->controller->SetAutomaticGainControl(automatic_gain_control);

  // If we have created the controller successfully create a entry and add it
  // to the map.
  entry->stream_id = stream_id;

  audio_entries_.insert(std::make_pair(stream_id, entry.release()));
}

void AudioInputRendererHost::OnAssociateStreamWithConsumer(int stream_id,
                                                           int render_view_id) {
  // TODO(miu): Will use render_view_id in upcoming change.
  DVLOG(1) << "AudioInputRendererHost@" << this
           << "::OnAssociateStreamWithConsumer(stream_id=" << stream_id
           << ", render_view_id=" << render_view_id << ")";
}

void AudioInputRendererHost::OnRecordStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller->Record();
}

void AudioInputRendererHost::OnCloseStream(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);

  if (entry)
    CloseAndDeleteStream(entry);

  int session_id = LookupSessionById(stream_id);

  if (session_id)
    StopAndDeleteDevice(session_id);
}

void AudioInputRendererHost::OnSetVolume(int stream_id, double volume) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller->SetVolume(volume);
}

void AudioInputRendererHost::SendErrorMessage(int stream_id) {
  Send(new AudioInputMsg_NotifyStreamStateChanged(
      stream_id, media::AudioInputIPCDelegate::kError));
}

void AudioInputRendererHost::DeleteEntries() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (AudioEntryMap::iterator i = audio_entries_.begin();
       i != audio_entries_.end(); ++i) {
    CloseAndDeleteStream(i->second);
  }
}

void AudioInputRendererHost::OnDeviceStarted(
    int session_id, const std::string& device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SessionEntryMap::iterator it = session_entries_.find(session_id);
  if (it == session_entries_.end()) {
    DLOG(WARNING) << "AudioInputRendererHost::OnDeviceStarted()"
        " session does not exist.";
    return;
  }

  // Notify the renderer with the id of the opened device.
  Send(new AudioInputMsg_NotifyDeviceStarted(it->second, device_id));
}

void AudioInputRendererHost::OnDeviceStopped(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SessionEntryMap::iterator it = session_entries_.find(session_id);
  // Return if the stream has been closed.
  if (it == session_entries_.end())
    return;

  int stream_id = it->second;
  AudioEntry* entry = LookupById(stream_id);

  if (entry) {
    // Device has been stopped, close the input stream.
    CloseAndDeleteStream(entry);
    // Notify the renderer that the state of the input stream has changed.
    Send(new AudioInputMsg_NotifyStreamStateChanged(
        stream_id, media::AudioInputIPCDelegate::kStopped));
  }

  // Delete the session entry.
  session_entries_.erase(it);
}

void AudioInputRendererHost::StopAndDeleteDevice(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  media_stream_manager_->audio_input_device_manager()->Stop(session_id);

  // Delete the session entry.
  session_entries_.erase(session_id);
}

void AudioInputRendererHost::CloseAndDeleteStream(AudioEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!entry->pending_close) {
    entry->controller->Close(base::Bind(&AudioInputRendererHost::DeleteEntry,
                                        this, entry));
    entry->pending_close = true;
  }
}

void AudioInputRendererHost::DeleteEntry(AudioEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Delete the entry when this method goes out of scope.
  scoped_ptr<AudioEntry> entry_deleter(entry);

  // Erase the entry from the map.
  audio_entries_.erase(entry->stream_id);
}

void AudioInputRendererHost::DeleteEntryOnError(AudioEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Sends the error message first before we close the stream because
  // |entry| is destroyed in DeleteEntry().
  SendErrorMessage(entry->stream_id);
  CloseAndDeleteStream(entry);
}

AudioInputRendererHost::AudioEntry* AudioInputRendererHost::LookupById(
    int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  AudioEntryMap::iterator i = audio_entries_.find(stream_id);
  if (i != audio_entries_.end())
    return i->second;
  return NULL;
}

AudioInputRendererHost::AudioEntry* AudioInputRendererHost::LookupByController(
    media::AudioInputController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Iterate the map of entries.
  // TODO(hclam): Implement a faster look up method.
  for (AudioEntryMap::iterator i = audio_entries_.begin();
       i != audio_entries_.end(); ++i) {
    if (controller == i->second->controller.get())
      return i->second;
  }
  return NULL;
}

int AudioInputRendererHost::LookupSessionById(int stream_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (SessionEntryMap::iterator it = session_entries_.begin();
       it != session_entries_.end(); ++it) {
    if (stream_id == it->second) {
      return it->first;
    }
  }
  return 0;
}

}  // namespace content
