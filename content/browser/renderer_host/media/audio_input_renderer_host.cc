// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_renderer_host.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sync_socket.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/capture/web_contents_audio_input_stream.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_input_sync_writer.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/media_switches.h"
#include "media/media_features.h"

namespace content {

namespace {

#if BUILDFLAG(ENABLE_WEBRTC)
const base::FilePath::CharType kDebugRecordingFileNameAddition[] =
    FILE_PATH_LITERAL("source_input");
#endif

void LogMessage(int stream_id, const std::string& msg, bool add_prefix) {
  std::ostringstream oss;
  oss << "[stream_id=" << stream_id << "] ";
  if (add_prefix)
    oss << "AIRH::";
  oss << msg;
  const std::string message = oss.str();
  content::MediaStreamManager::SendMessageToNativeLog(message);
  DVLOG(1) << message;
}

void NotifyProcessHostStreamAdded(int render_process_id) {
  auto* process_host = RenderProcessHost::FromID(render_process_id);
  if (process_host)
    process_host->OnMediaStreamAdded();
}

void NotifyProcessHostStreamRemoved(int render_process_id) {
  auto* process_host = RenderProcessHost::FromID(render_process_id);
  if (process_host)
    process_host->OnMediaStreamRemoved();
}

}  // namespace

struct AudioInputRendererHost::AudioEntry {
  AudioEntry();
  ~AudioEntry();

  // The AudioInputController that manages the audio input stream.
  scoped_refptr<media::AudioInputController> controller;

  // The audio input stream ID in the RenderFrame.
  int stream_id;

  // The synchronous writer to be used by the controller. We have the
  // ownership of the writer.
  std::unique_ptr<AudioInputSyncWriter> writer;

  // The socket, paired with |writer|s socket, which should be used on the
  // remote end.
  base::CancelableSyncSocket foreign_socket;

  // Set to true after we called Close() for the controller.
  bool pending_close;

#if defined(OS_CHROMEOS)
  AudioInputDeviceManager::KeyboardMicRegistration keyboard_mic_registration;
#endif
};

AudioInputRendererHost::AudioEntry::AudioEntry()
    : stream_id(0), pending_close(false) {}

AudioInputRendererHost::AudioEntry::~AudioEntry() {
}

AudioInputRendererHost::AudioInputRendererHost(
    int render_process_id,
    media::AudioManager* audio_manager,
    MediaStreamManager* media_stream_manager,
    AudioMirroringManager* audio_mirroring_manager,
    media::UserInputMonitor* user_input_monitor)
    : BrowserMessageFilter(AudioMsgStart),
      render_process_id_(render_process_id),
      renderer_pid_(0),
      audio_manager_(audio_manager),
      media_stream_manager_(media_stream_manager),
      audio_mirroring_manager_(audio_mirroring_manager),
      user_input_monitor_(user_input_monitor),
      audio_log_(MediaInternals::GetInstance()->CreateAudioLog(
          media::AudioLogFactory::AUDIO_INPUT_CONTROLLER)) {}

AudioInputRendererHost::~AudioInputRendererHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(audio_entries_.empty());
}

#if BUILDFLAG(ENABLE_WEBRTC)
void AudioInputRendererHost::EnableDebugRecording(const base::FilePath& file) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (audio_entries_.empty())
    return;
  base::FilePath file_with_extensions =
      GetDebugRecordingFilePathWithExtensions(file);
  for (const auto& entry : audio_entries_)
    EnableDebugRecordingForId(file_with_extensions, entry.first);
}

void AudioInputRendererHost::DisableDebugRecording() {
  for (const auto& entry : audio_entries_) {
    entry.second->controller->DisableDebugRecording();
  }
}
#endif  // ENABLE_WEBRTC

void AudioInputRendererHost::OnChannelClosing() {
  // Since the IPC sender is gone, close all requested audio streams.
  DeleteEntries();
}

void AudioInputRendererHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void AudioInputRendererHost::OnCreated(media::AudioInputController* controller,
                                       bool initially_muted) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&AudioInputRendererHost::DoCompleteCreation, this,
                     base::RetainedRef(controller), initially_muted));
}

void AudioInputRendererHost::OnError(media::AudioInputController* controller,
    media::AudioInputController::ErrorCode error_code) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&AudioInputRendererHost::DoHandleError, this,
                     base::RetainedRef(controller), error_code));
}

void AudioInputRendererHost::OnLog(media::AudioInputController* controller,
                                   const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&AudioInputRendererHost::DoLog, this,
                     base::RetainedRef(controller), message));
}

void AudioInputRendererHost::OnMuted(media::AudioInputController* controller,
                                     bool is_muted) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&AudioInputRendererHost::DoNotifyMutedState, this,
                     base::RetainedRef(controller), is_muted));
}

void AudioInputRendererHost::set_renderer_pid(int32_t renderer_pid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  renderer_pid_ = renderer_pid;
}

void AudioInputRendererHost::DoCompleteCreation(
    media::AudioInputController* controller,
    bool initially_muted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* entry = LookupByController(controller);
  DCHECK(entry);
  AudioInputSyncWriter* writer = entry->writer.get();
  DCHECK(writer);
  DCHECK(PeerHandle());

  // Once the audio stream is created then complete the creation process by
  // mapping shared memory and sharing with the renderer process.
  base::SharedMemoryHandle foreign_memory_handle =
      writer->shared_memory()->handle().Duplicate();
  if (!foreign_memory_handle.IsValid()) {
    // If we failed to map and share the shared memory then close the audio
    // stream and send an error message.
    DeleteEntryOnError(entry, MEMORY_SHARING_FAILED);
    return;
  }

  base::CancelableSyncSocket::TransitDescriptor socket_transit_descriptor;

  // If we failed to prepare the sync socket for the renderer then we fail
  // the construction of audio input stream.
  if (!entry->foreign_socket.PrepareTransitDescriptor(
          PeerHandle(), &socket_transit_descriptor)) {
    foreign_memory_handle.Close();
    DeleteEntryOnError(entry, SYNC_SOCKET_ERROR);
    return;
  }

  LogMessage(entry->stream_id,
             base::StringPrintf("DoCompleteCreation: IPC channel and stream "
                                "are now open (initially %s",
                                initially_muted ? "muted" : "not muted"),
             true);

  Send(new AudioInputMsg_NotifyStreamCreated(
      entry->stream_id, foreign_memory_handle, socket_transit_descriptor,
      initially_muted));

  // Free the foreign socket on here since it isn't needed anymore in this
  // process.
  entry->foreign_socket.Close();
}

void AudioInputRendererHost::DoHandleError(
    media::AudioInputController* controller,
    media::AudioInputController::ErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AudioEntry* entry = LookupByController(controller);
  DCHECK(entry);

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
  DCHECK(entry);

  // Add stream ID and current audio level reported by AIC to native log.
  LogMessage(entry->stream_id, message, false);
}

void AudioInputRendererHost::DoNotifyMutedState(
    media::AudioInputController* controller,
    bool is_muted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AudioEntry* entry = LookupByController(controller);
  DCHECK(entry);
  LogMessage(entry->stream_id,
             base::StringPrintf("OnMuted: State changed to: %s",
                                (is_muted ? "muted" : "not muted")),
             true);
  Send(new AudioInputMsg_NotifyStreamMuted(entry->stream_id, is_muted));
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
    int render_frame_id,
    int session_id,
    const AudioInputHostMsg_CreateStream_Config& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

#if defined(OS_CHROMEOS)
  if (config.params.channel_layout() ==
      media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
    media_stream_manager_->audio_input_device_manager()
        ->RegisterKeyboardMicStream(
            base::BindOnce(&AudioInputRendererHost::DoCreateStream, this,
                           stream_id, render_frame_id, session_id, config));
  } else {
    DoCreateStream(stream_id, render_frame_id, session_id, config,
                   AudioInputDeviceManager::KeyboardMicRegistration());
  }
#else
  DoCreateStream(stream_id, render_frame_id, session_id, config);
#endif
}

void AudioInputRendererHost::DoCreateStream(
    int stream_id,
    int render_frame_id,
    int session_id,
    const AudioInputHostMsg_CreateStream_Config& config
#if defined(OS_CHROMEOS)
    ,
    AudioInputDeviceManager::KeyboardMicRegistration keyboard_mic_registration
#endif
    ) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK_GT(render_frame_id, 0);

  // media::AudioParameters is validated in the deserializer.
  if (LookupById(stream_id)) {
    SendErrorMessage(stream_id, STREAM_ALREADY_EXISTS);
    return;
  }

  // Check if we have the permission to open the device and which device to use.
  const MediaStreamDevice* device =
      media_stream_manager_->audio_input_device_manager()->GetOpenedDeviceById(
          session_id);
  if (!device) {
    SendErrorMessage(stream_id, PERMISSION_DENIED);
    DLOG(WARNING) << "No permission has been granted to input stream with "
                  << "session_id=" << session_id;
    return;
  }

  const MediaStreamType& type = device->type;
  const std::string& device_id = device->id;
  const std::string& device_name = device->name;
  media::AudioParameters audio_params(config.params);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    audio_params.set_format(media::AudioParameters::AUDIO_FAKE);
  }

  std::ostringstream oss;
  oss << "[stream_id=" << stream_id << "] "
      << "AIRH::OnCreateStream(render_frame_id=" << render_frame_id
      << ", session_id=" << session_id << ")"
      << ": device_name=" << device_name;

  // Create a new AudioEntry structure.
  std::unique_ptr<AudioEntry> entry = base::MakeUnique<AudioEntry>();

  entry->writer = AudioInputSyncWriter::Create(
      config.shared_memory_count, audio_params, &entry->foreign_socket);

  if (!entry->writer) {
    SendErrorMessage(stream_id, SYNC_WRITER_INIT_FAILED);
    return;
  }

  if (WebContentsMediaCaptureId::Parse(device_id, nullptr)) {
    // For MEDIA_DESKTOP_AUDIO_CAPTURE, the source is selected from picker
    // window, we do not mute the source audio.
    // For MEDIA_TAB_AUDIO_CAPTURE, the probable use case is Cast, we mute
    // the source audio.
    // TODO(qiangchen): Analyze audio constraints to make a duplicating or
    // diverting decision. It would give web developer more flexibility.
    entry->controller = media::AudioInputController::CreateForStream(
        audio_manager_->GetTaskRunner(), this,
        WebContentsAudioInputStream::Create(
            device_id, audio_params, audio_manager_->GetWorkerTaskRunner(),
            audio_mirroring_manager_),
        entry->writer.get(), user_input_monitor_,
        audio_params);
    // Only count for captures from desktop media picker dialog.
    if (entry->controller.get() && type == MEDIA_DESKTOP_AUDIO_CAPTURE)
      IncrementDesktopCaptureCounter(TAB_AUDIO_CAPTURER_CREATED);
  } else {
    entry->controller = media::AudioInputController::Create(
        audio_manager_, this, entry->writer.get(), user_input_monitor_,
        audio_params, device_id, config.automatic_gain_control);
    oss << ", AGC=" << config.automatic_gain_control;

    // Only count for captures from desktop media picker dialog and system loop
    // back audio.
    if (entry->controller.get() && type == MEDIA_DESKTOP_AUDIO_CAPTURE &&
        (device_id == media::AudioDeviceDescription::kLoopbackInputDeviceId ||
         device_id ==
             media::AudioDeviceDescription::kLoopbackWithMuteDeviceId)) {
      IncrementDesktopCaptureCounter(SYSTEM_LOOPBACK_AUDIO_CAPTURER_CREATED);
    }
  }

  if (!entry->controller.get()) {
    SendErrorMessage(stream_id, STREAM_CREATE_ERROR);
    return;
  }

#if defined(OS_CHROMEOS)
  entry->keyboard_mic_registration = std::move(keyboard_mic_registration);
#endif

  const std::string log_message = oss.str();
  MediaStreamManager::SendMessageToNativeLog(log_message);
  DVLOG(1) << log_message;

  // Since the controller was created successfully, create an entry and add it
  // to the map.
  entry->stream_id = stream_id;
  audio_entries_.insert(std::make_pair(stream_id, entry.release()));
  audio_log_->OnCreated(stream_id, audio_params, device_id);
  MediaInternals::GetInstance()->SetWebContentsTitleForAudioLogEntry(
      stream_id, render_process_id_, render_frame_id, audio_log_.get());

  // Prevent process backgrounding while audio input is active:
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&NotifyProcessHostStreamAdded, render_process_id_));

#if BUILDFLAG(ENABLE_WEBRTC)
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&AudioInputRendererHost::MaybeEnableDebugRecordingForId,
                     this, stream_id));
#endif
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

  if (volume < 0 || volume > 1) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::AIRH_VOLUME_OUT_OF_RANGE);
    return;
  }

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

  Send(new AudioInputMsg_NotifyStreamError(stream_id));
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
    entry->controller->Close(
        base::BindOnce(&AudioInputRendererHost::DeleteEntry, this, entry));
    entry->pending_close = true;
    audio_log_->OnClosed(entry->stream_id);

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&NotifyProcessHostStreamRemoved, render_process_id_));
  }
}

void AudioInputRendererHost::DeleteEntry(AudioEntry* entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LogMessage(entry->stream_id, "DeleteEntry: stream is now closed", true);

  // Delete the entry when this method goes out of scope.
  std::unique_ptr<AudioEntry> entry_deleter(entry);

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
  return nullptr;
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
  return nullptr;
}

#if BUILDFLAG(ENABLE_WEBRTC)
void AudioInputRendererHost::MaybeEnableDebugRecordingForId(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (WebRTCInternals::GetInstance()->IsAudioDebugRecordingsEnabled()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &AudioInputRendererHost::
                AddExtensionsToPathAndEnableDebugRecordingForId,
            this,
            WebRTCInternals::GetInstance()->GetAudioDebugRecordingsFilePath(),
            stream_id));
  }
}

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

base::FilePath AudioInputRendererHost::GetDebugRecordingFilePathWithExtensions(
    const base::FilePath& file) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // We expect |renderer_pid_| to be set.
  DCHECK_GT(renderer_pid_, 0);
  return file.AddExtension(IntToStringType(renderer_pid_))
             .AddExtension(kDebugRecordingFileNameAddition);
}

void AudioInputRendererHost::EnableDebugRecordingForId(
    const base::FilePath& file_name,
    int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AudioEntry* entry = LookupById(stream_id);
  if (!entry)
    return;
  entry->controller->EnableDebugRecording(
      file_name.AddExtension(IntToStringType(stream_id)));
}

#undef IntToStringType

void AudioInputRendererHost::AddExtensionsToPathAndEnableDebugRecordingForId(
    const base::FilePath& file,
    int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EnableDebugRecordingForId(GetDebugRecordingFilePathWithExtensions(file),
                            stream_id);
}

#endif  // BUILDFLAG(ENABLE_WEBRTC)

}  // namespace content
