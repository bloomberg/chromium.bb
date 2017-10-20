// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_renderer_host.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/sync_socket.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_delegate_impl.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "media/audio/audio_manager.h"

namespace content {

namespace {

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

}  // namespace

AudioInputRendererHost::AudioInputRendererHost(
    int render_process_id,
    media::AudioManager* audio_manager,
    MediaStreamManager* media_stream_manager,
    AudioMirroringManager* audio_mirroring_manager,
    media::UserInputMonitor* user_input_monitor)
    : BrowserMessageFilter(AudioMsgStart),
      render_process_id_(render_process_id),
      audio_manager_(audio_manager),
      media_stream_manager_(media_stream_manager),
      audio_mirroring_manager_(audio_mirroring_manager),
      user_input_monitor_(user_input_monitor),
      audio_log_(MediaInternals::GetInstance()->CreateAudioLog(
          media::AudioLogFactory::AUDIO_INPUT_CONTROLLER)) {}

AudioInputRendererHost::~AudioInputRendererHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(delegates_.empty());
}

void AudioInputRendererHost::OnChannelClosing() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Since the IPC sender is gone, close all audio streams.
  delegates_.clear();
}

void AudioInputRendererHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void AudioInputRendererHost::OnStreamCreated(
    int stream_id,
    const base::SharedMemory* shared_memory,
    std::unique_ptr<base::CancelableSyncSocket> socket,
    bool initially_muted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(PeerHandle());

  // Once the audio stream is created then complete the creation process by
  // mapping shared memory and sharing with the renderer process.
  base::SharedMemoryHandle foreign_memory_handle =
      shared_memory->handle().Duplicate();
  if (!foreign_memory_handle.IsValid()) {
    // If we failed to map and share the shared memory then close the audio
    // stream and send an error message.
    DeleteDelegateOnError(stream_id, MEMORY_SHARING_FAILED);
    return;
  }

  base::CancelableSyncSocket::TransitDescriptor socket_transit_descriptor;

  // If we failed to prepare the sync socket for the renderer then we fail
  // the construction of audio input stream.
  if (!socket->PrepareTransitDescriptor(PeerHandle(),
                                        &socket_transit_descriptor)) {
    foreign_memory_handle.Close();
    DeleteDelegateOnError(stream_id, SYNC_SOCKET_ERROR);
    return;
  }

  LogMessage(stream_id,
             base::StringPrintf("DoCompleteCreation: IPC channel and stream "
                                "are now open (initially%s muted)",
                                initially_muted ? "" : " not"),
             true);

  Send(new AudioInputMsg_NotifyStreamCreated(stream_id, foreign_memory_handle,
                                             socket_transit_descriptor,
                                             initially_muted));
}

void AudioInputRendererHost::OnStreamError(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeleteDelegateOnError(stream_id, AUDIO_INPUT_CONTROLLER_ERROR);
}

void AudioInputRendererHost::OnMuted(int stream_id, bool is_muted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Send(new AudioInputMsg_NotifyStreamMuted(stream_id, is_muted));
}

bool AudioInputRendererHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

  std::unique_ptr<media::AudioInputDelegate> delegate =
      AudioInputDelegateImpl::Create(
          this, audio_manager_, audio_mirroring_manager_, user_input_monitor_,
          media_stream_manager_->audio_input_device_manager(),
          MediaInternals::GetInstance()->CreateAudioLog(
              media::AudioLogFactory::AUDIO_INPUT_CONTROLLER),
#if defined(OS_CHROMEOS)
          std::move(keyboard_mic_registration),
#endif
          config.shared_memory_count, stream_id, session_id, render_process_id_,
          render_frame_id, config.automatic_gain_control, config.params);

  if (!delegate) {
    // Error was logged by AudioInputDelegateImpl::Create.
    Send(new AudioInputMsg_NotifyStreamError(stream_id));

    return;
  }

  delegates_.emplace(stream_id, std::move(delegate));
}

void AudioInputRendererHost::OnRecordStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LogMessage(stream_id, "OnRecordStream", true);

  media::AudioInputDelegate* delegate = LookupById(stream_id);
  if (!delegate) {
    SendErrorMessage(stream_id, INVALID_AUDIO_ENTRY);
    return;
  }

  delegate->OnRecordStream();
}

void AudioInputRendererHost::OnCloseStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LogMessage(stream_id, "OnCloseStream", true);

  delegates_.erase(stream_id);
}

void AudioInputRendererHost::OnSetVolume(int stream_id, double volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (volume < 0 || volume > 1) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::AIRH_VOLUME_OUT_OF_RANGE);
    return;
  }

  media::AudioInputDelegate* delegate = LookupById(stream_id);
  if (!delegate) {
    SendErrorMessage(stream_id, INVALID_AUDIO_ENTRY);
    return;
  }

  delegate->OnSetVolume(volume);
}

void AudioInputRendererHost::SendErrorMessage(
    int stream_id, ErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::string err_msg =
      base::StringPrintf("SendErrorMessage(error_code=%d)", error_code);
  LogMessage(stream_id, err_msg, true);

  Send(new AudioInputMsg_NotifyStreamError(stream_id));
}

void AudioInputRendererHost::DeleteDelegateOnError(int stream_id,
                                                   ErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendErrorMessage(stream_id, error_code);
  delegates_.erase(stream_id);
}

media::AudioInputDelegate* AudioInputRendererHost::LookupById(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioInputDelegateMap::iterator i = delegates_.find(stream_id);
  if (i != delegates_.end())
    return i->second.get();
  return nullptr;
}

}  // namespace content
