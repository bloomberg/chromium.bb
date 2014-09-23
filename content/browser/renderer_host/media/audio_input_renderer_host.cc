// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_renderer_host.h"

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_math.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "content/browser/media/capture/web_contents_audio_input_stream.h"
#include "content/browser/media/capture/web_contents_capture_util.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_input_sync_writer.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"

namespace {

void LogMessage(int stream_id, const std::string& msg, bool add_prefix) {
  std::ostringstream oss;
  oss << "[stream_id=" << stream_id << "] ";
  if (add_prefix)
    oss << "AIRH::";
  oss << msg;
  content::MediaStreamManager::SendMessageToNativeLog(oss.str());
  DVLOG(1) << oss.str();
}

}  // namespace

namespace content {

struct AudioInputRendererHost::AudioEntry {
  AudioEntry();
  ~AudioEntry();

  // The AudioInputController that manages the audio input stream.
  scoped_refptr<media::AudioInputController> controller;

  // The audio input stream ID in the render view.
  int stream_id;

  // Shared memory for transmission of the audio data. It has
  // |shared_memory_segment_count| equal lengthed segments.
  base::SharedMemory shared_memory;
  int shared_memory_segment_count;

  // The synchronous writer to be used by the controller. We have the
  // ownership of the writer.
  scoped_ptr<media::AudioInputController::SyncWriter> writer;

  // Set to true after we called Close() for the controller.
  bool pending_close;

  // If this entry's layout has a keyboard mic channel.
  bool has_keyboard_mic_;
};

AudioInputRendererHost::AudioEntry::AudioEntry()
    : stream_id(0),
      shared_memory_segment_count(0),
      pending_close(false),
      has_keyboard_mic_(false) {
}

AudioInputRendererHost::AudioEntry::~AudioEntry() {}

AudioInputRendererHost::AudioInputRendererHost(
    media::AudioManager* audio_manager,
    MediaStreamManager* media_stream_manager,
    AudioMirroringManager* audio_mirroring_manager,
    media::UserInputMonitor* user_input_monitor)
    : BrowserMessageFilter(AudioMsgStart),
      audio_manager_(audio_manager),
      media_stream_manager_(media_stream_manager),
      audio_mirroring_manager_(audio_mirroring_manager),
      user_input_monitor_(user_input_monitor),
      audio_log_(MediaInternals::GetInstance()->CreateAudioLog(
          media::AudioLogFactory::AUDIO_INPUT_CONTROLLER)) {}

AudioInputRendererHost::~AudioInputRendererHost() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(audio_entries_.empty());
}

void AudioInputRendererHost::OnChannelClosing() {
  // Since the IPC sender is gone, close all requested audio streams.
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

void AudioInputRendererHost::OnError(media::AudioInputController* controller,
    media::AudioInputController::ErrorCode error_code) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &AudioInputRendererHost::DoHandleError,
          this,
          make_scoped_refptr(controller),
          error_code));
}

void AudioInputRendererHost::OnData(media::AudioInputController* controller,
                                    const media::AudioBus* data) {
  NOTREACHED() << "Only low-latency mode is supported.";
}

void AudioInputRendererHost::OnLog(media::AudioInputController* controller,
                                   const std::string& message) {
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&AudioInputRendererHost::DoLog,
                                     this,
                                     make_scoped_refptr(controller),
                                     message));
}

void AudioInputRendererHost::DoCompleteCreation(
    media::AudioInputController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* entry = LookupByController(controller);
  if (!entry) {
    NOTREACHED() << "AudioInputController is invalid.";
    return;
  }

  if (!PeerHandle()) {
    NOTREACHED() << "Renderer process handle is invalid.";
    DeleteEntryOnError(entry, INVALID_PEER_HANDLE);
    return;
  }

  if (!entry->controller->SharedMemoryAndSyncSocketMode()) {
    NOTREACHED() << "Only shared-memory/sync-socket mode is supported.";
    DeleteEntryOnError(entry, INVALID_LATENCY_MODE);
    return;
  }

  // Once the audio stream is created then complete the creation process by
  // mapping shared memory and sharing with the renderer process.
  base::SharedMemoryHandle foreign_memory_handle;
  if (!entry->shared_memory.ShareToProcess(PeerHandle(),
                                           &foreign_memory_handle)) {
    // If we failed to map and share the shared memory then close the audio
    // stream and send an error message.
    DeleteEntryOnError(entry, MEMORY_SHARING_FAILED);
    return;
  }

  AudioInputSyncWriter* writer =
      static_cast<AudioInputSyncWriter*>(entry->writer.get());

  base::SyncSocket::TransitDescriptor socket_transit_descriptor;

  // If we failed to prepare the sync socket for the renderer then we fail
  // the construction of audio input stream.
  if (!writer->PrepareForeignSocket(PeerHandle(), &socket_transit_descriptor)) {
    DeleteEntryOnError(entry, SYNC_SOCKET_ERROR);
    return;
  }

  LogMessage(entry->stream_id,
             "DoCompleteCreation: IPC channel and stream are now open",
             true);

  Send(new AudioInputMsg_NotifyStreamCreated(
      entry->stream_id, foreign_memory_handle, socket_transit_descriptor,
      entry->shared_memory.requested_size(),
      entry->shared_memory_segment_count));
}

void AudioInputRendererHost::DoSendRecordingMessage(
    media::AudioInputController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(henrika): See crbug.com/115262 for details on why this method
  // should be implemented.
  AudioEntry* entry = LookupByController(controller);
  if (!entry) {
    NOTREACHED() << "AudioInputController is invalid.";
    return;
  }
  LogMessage(
      entry->stream_id, "DoSendRecordingMessage: stream is now started", true);
}

void AudioInputRendererHost::DoHandleError(
    media::AudioInputController* controller,
    media::AudioInputController::ErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AudioEntry* entry = LookupByController(controller);
  if (!entry) {
    NOTREACHED() << "AudioInputController is invalid.";
    return;
  }

  // This is a fix for crbug.com/357501. The error can be triggered when closing
  // the lid on Macs, which causes more problems than it fixes.
  // Also, in crbug.com/357569, the goal is to remove usage of the error since
  // it was added to solve a crash on Windows that no longer can be reproduced.
  if (error_code == media::AudioInputController::NO_DATA_ERROR) {
    // TODO(henrika): it might be possible to do something other than just
    // logging when we detect many NO_DATA_ERROR calls for a stream.
    LogMessage(entry->stream_id, "AIC::DoCheckForNoData: NO_DATA_ERROR", false);
    return;
  }

  std::ostringstream oss;
  oss << "AIC reports error_code=" << error_code;
  LogMessage(entry->stream_id, oss.str(), false);

  audio_log_->OnError(entry->stream_id);
  DeleteEntryOnError(entry, AUDIO_INPUT_CONTROLLER_ERROR);
}

void AudioInputRendererHost::DoLog(media::AudioInputController* controller,
                                   const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AudioEntry* entry = LookupByController(controller);
  if (!entry) {
    NOTREACHED() << "AudioInputController is invalid.";
    return;
  }

  // Add stream ID and current audio level reported by AIC to native log.
  LogMessage(entry->stream_id, message, false);
}

bool AudioInputRendererHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioInputRendererHost, message)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_CreateStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_RecordStream, OnRecordStream)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_CloseStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(AudioInputHostMsg_SetVolume, OnSetVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AudioInputRendererHost::OnCreateStream(
    int stream_id,
    int render_view_id,
    int session_id,
    const AudioInputHostMsg_CreateStream_Config& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

#if defined(OS_CHROMEOS)
  if (config.params.channel_layout() ==
      media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
    media_stream_manager_->audio_input_device_manager()
        ->RegisterKeyboardMicStream(
            base::Bind(&AudioInputRendererHost::DoCreateStream,
                       this,
                       stream_id,
                       render_view_id,
                       session_id,
                       config));
  } else {
    DoCreateStream(stream_id, render_view_id, session_id, config);
  }
#else
  DoCreateStream(stream_id, render_view_id, session_id, config);
#endif
}

void AudioInputRendererHost::DoCreateStream(
    int stream_id,
    int render_view_id,
    int session_id,
    const AudioInputHostMsg_CreateStream_Config& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::ostringstream oss;
  oss << "[stream_id=" << stream_id << "] "
      << "AIRH::OnCreateStream(render_view_id=" << render_view_id
      << ", session_id=" << session_id << ")";
  DCHECK_GT(render_view_id, 0);

  // media::AudioParameters is validated in the deserializer.
  if (LookupById(stream_id) != NULL) {
    SendErrorMessage(stream_id, STREAM_ALREADY_EXISTS);
    MaybeUnregisterKeyboardMicStream(config);
    return;
  }

  media::AudioParameters audio_params(config.params);
  if (media_stream_manager_->audio_input_device_manager()->
      ShouldUseFakeDevice()) {
    audio_params.Reset(
        media::AudioParameters::AUDIO_FAKE,
        config.params.channel_layout(), config.params.channels(),
        config.params.sample_rate(), config.params.bits_per_sample(),
        config.params.frames_per_buffer());
  }

  // Check if we have the permission to open the device and which device to use.
  std::string device_name;
  std::string device_id = media::AudioManagerBase::kDefaultDeviceId;
  if (audio_params.format() != media::AudioParameters::AUDIO_FAKE) {
    const StreamDeviceInfo* info = media_stream_manager_->
        audio_input_device_manager()->GetOpenedDeviceInfoById(session_id);
    if (!info) {
      SendErrorMessage(stream_id, PERMISSION_DENIED);
      DLOG(WARNING) << "No permission has been granted to input stream with "
                    << "session_id=" << session_id;
      MaybeUnregisterKeyboardMicStream(config);
      return;
    }

    device_id = info->device.id;
    device_name = info->device.name;
    oss << ": device_name=" << device_name;
  }

  // Create a new AudioEntry structure.
  scoped_ptr<AudioEntry> entry(new AudioEntry());

  const uint32 segment_size =
      (sizeof(media::AudioInputBufferParameters) +
       media::AudioBus::CalculateMemorySize(audio_params));
  entry->shared_memory_segment_count = config.shared_memory_count;

  // Create the shared memory and share it with the renderer process
  // using a new SyncWriter object.
  base::CheckedNumeric<uint32> size = segment_size;
  size *= entry->shared_memory_segment_count;
  if (!size.IsValid() ||
      !entry->shared_memory.CreateAndMapAnonymous(size.ValueOrDie())) {
    // If creation of shared memory failed then send an error message.
    SendErrorMessage(stream_id, SHARED_MEMORY_CREATE_FAILED);
    MaybeUnregisterKeyboardMicStream(config);
    return;
  }

  scoped_ptr<AudioInputSyncWriter> writer(new AudioInputSyncWriter(
      &entry->shared_memory, entry->shared_memory_segment_count, audio_params));

  if (!writer->Init()) {
    SendErrorMessage(stream_id, SYNC_WRITER_INIT_FAILED);
    MaybeUnregisterKeyboardMicStream(config);
    return;
  }

  // If we have successfully created the SyncWriter then assign it to the
  // entry and construct an AudioInputController.
  entry->writer.reset(writer.release());
  if (WebContentsCaptureUtil::IsWebContentsDeviceId(device_id)) {
    entry->controller = media::AudioInputController::CreateForStream(
        audio_manager_->GetTaskRunner(),
        this,
        WebContentsAudioInputStream::Create(
            device_id,
            audio_params,
            audio_manager_->GetWorkerTaskRunner(),
            audio_mirroring_manager_),
        entry->writer.get(),
        user_input_monitor_);
  } else {
    // TODO(henrika): replace CreateLowLatency() with Create() as soon
    // as satish has ensured that Speech Input also uses the default low-
    // latency path. See crbug.com/112472 for details.
    entry->controller =
        media::AudioInputController::CreateLowLatency(audio_manager_,
                                                      this,
                                                      audio_params,
                                                      device_id,
                                                      entry->writer.get(),
                                                      user_input_monitor_);
  }

  if (!entry->controller.get()) {
    SendErrorMessage(stream_id, STREAM_CREATE_ERROR);
    MaybeUnregisterKeyboardMicStream(config);
    return;
  }

  // Set the initial AGC state for the audio input stream. Note that, the AGC
  // is only supported in AUDIO_PCM_LOW_LATENCY mode.
  if (config.params.format() == media::AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    entry->controller->SetAutomaticGainControl(config.automatic_gain_control);
    oss << ", AGC=" << config.automatic_gain_control;
  }

  MediaStreamManager::SendMessageToNativeLog(oss.str());
  DVLOG(1) << oss.str();

  // Since the controller was created successfully, create an entry and add it
  // to the map.
  entry->stream_id = stream_id;
  audio_entries_.insert(std::make_pair(stream_id, entry.release()));
  audio_log_->OnCreated(stream_id, audio_params, device_id);
}

void AudioInputRendererHost::OnRecordStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LogMessage(stream_id, "OnRecordStream", true);

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id, INVALID_AUDIO_ENTRY);
    return;
  }

  entry->controller->Record();
  audio_log_->OnStarted(stream_id);
}

void AudioInputRendererHost::OnCloseStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LogMessage(stream_id, "OnCloseStream", true);

  AudioEntry* entry = LookupById(stream_id);

  if (entry)
    CloseAndDeleteStream(entry);
}

void AudioInputRendererHost::OnSetVolume(int stream_id, double volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id, INVALID_AUDIO_ENTRY);
    return;
  }

  entry->controller->SetVolume(volume);
  audio_log_->OnSetVolume(stream_id, volume);
}

void AudioInputRendererHost::SendErrorMessage(
    int stream_id, ErrorCode error_code) {
  std::string err_msg =
      base::StringPrintf("SendErrorMessage(error_code=%d)", error_code);
  LogMessage(stream_id, err_msg, true);

  Send(new AudioInputMsg_NotifyStreamStateChanged(
      stream_id, media::AudioInputIPCDelegate::kError));
}

void AudioInputRendererHost::DeleteEntries() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (AudioEntryMap::iterator i = audio_entries_.begin();
       i != audio_entries_.end(); ++i) {
    CloseAndDeleteStream(i->second);
  }
}

void AudioInputRendererHost::CloseAndDeleteStream(AudioEntry* entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!entry->pending_close) {
    LogMessage(entry->stream_id, "CloseAndDeleteStream", true);
    entry->controller->Close(base::Bind(&AudioInputRendererHost::DeleteEntry,
                                        this, entry));
    entry->pending_close = true;
    audio_log_->OnClosed(entry->stream_id);
  }
}

void AudioInputRendererHost::DeleteEntry(AudioEntry* entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LogMessage(entry->stream_id, "DeleteEntry: stream is now closed", true);

#if defined(OS_CHROMEOS)
  if (entry->has_keyboard_mic_) {
    media_stream_manager_->audio_input_device_manager()
        ->UnregisterKeyboardMicStream();
  }
#endif

  // Delete the entry when this method goes out of scope.
  scoped_ptr<AudioEntry> entry_deleter(entry);

  // Erase the entry from the map.
  audio_entries_.erase(entry->stream_id);
}

void AudioInputRendererHost::DeleteEntryOnError(AudioEntry* entry,
    ErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Sends the error message first before we close the stream because
  // |entry| is destroyed in DeleteEntry().
  SendErrorMessage(entry->stream_id, error_code);
  CloseAndDeleteStream(entry);
}

AudioInputRendererHost::AudioEntry* AudioInputRendererHost::LookupById(
    int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntryMap::iterator i = audio_entries_.find(stream_id);
  if (i != audio_entries_.end())
    return i->second;
  return NULL;
}

AudioInputRendererHost::AudioEntry* AudioInputRendererHost::LookupByController(
    media::AudioInputController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Iterate the map of entries.
  // TODO(hclam): Implement a faster look up method.
  for (AudioEntryMap::iterator i = audio_entries_.begin();
       i != audio_entries_.end(); ++i) {
    if (controller == i->second->controller.get())
      return i->second;
  }
  return NULL;
}

void AudioInputRendererHost::MaybeUnregisterKeyboardMicStream(
    const AudioInputHostMsg_CreateStream_Config& config) {
#if defined(OS_CHROMEOS)
  if (config.params.channel_layout() ==
      media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
    media_stream_manager_->audio_input_device_manager()
        ->UnregisterKeyboardMicStream();
  }
#endif
}

}  // namespace content
