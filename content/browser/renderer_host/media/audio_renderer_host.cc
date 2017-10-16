// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_renderer_host.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_output_delegate_impl.h"
#include "content/browser/renderer_host/media/audio_sync_reader.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/audio_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"

using media::AudioBus;
using media::AudioManager;

namespace content {

namespace {

void UMALogDeviceAuthorizationTime(base::TimeTicks auth_start_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.OutputDeviceAuthorizationTime",
                             base::TimeTicks::Now() - auth_start_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(5000), 50);
}

// Check that the routing ID references a valid RenderFrameHost, and run
// |callback| on the IO thread with true if the ID is valid.
void ValidateRenderFrameId(int render_process_id,
                           int render_frame_id,
                           base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool frame_exists =
      !!RenderFrameHost::FromID(render_process_id, render_frame_id);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(std::move(callback), frame_exists));
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// AudioRendererHost implementations.

AudioRendererHost::AudioRendererHost(int render_process_id,
                                     media::AudioManager* audio_manager,
                                     media::AudioSystem* audio_system,
                                     AudioMirroringManager* mirroring_manager,
                                     MediaStreamManager* media_stream_manager)
    : BrowserMessageFilter(AudioMsgStart),
      render_process_id_(render_process_id),
      audio_manager_(audio_manager),
      mirroring_manager_(mirroring_manager),
      media_stream_manager_(media_stream_manager),
      authorization_handler_(audio_system,
                             media_stream_manager,
                             render_process_id_) {
  DCHECK(audio_manager_);
}

AudioRendererHost::~AudioRendererHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(delegates_.empty());
}

void AudioRendererHost::OnChannelClosing() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  delegates_.clear();

  // Remove any authorizations for streams that were not yet created
  authorizations_.clear();
}

void AudioRendererHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void AudioRendererHost::OnStreamCreated(
    int stream_id,
    const base::SharedMemory* shared_memory,
    std::unique_ptr<base::CancelableSyncSocket> foreign_socket) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!PeerHandle()) {
    DLOG(WARNING) << "Renderer process handle is invalid.";
    OnStreamError(stream_id);
    return;
  }

  if (!LookupById(stream_id)) {
    OnStreamError(stream_id);
    return;
  }

  base::SyncSocket::TransitDescriptor socket_descriptor;

  base::SharedMemoryHandle foreign_memory_handle =
      shared_memory->handle().Duplicate();
  if (!(foreign_memory_handle.IsValid() &&
        foreign_socket->PrepareTransitDescriptor(PeerHandle(),
                                                 &socket_descriptor))) {
    // Something went wrong in preparing the IPC handles.
    OnStreamError(stream_id);
    return;
  }

  Send(new AudioMsg_NotifyStreamCreated(stream_id, foreign_memory_handle,
                                        socket_descriptor));
}

void AudioRendererHost::OnStreamError(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SendErrorMessage(stream_id);
  OnCloseStream(stream_id);
}

void AudioRendererHost::DidValidateRenderFrame(int stream_id, bool is_valid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (is_valid)
    return;

  DLOG(WARNING) << "Render frame for stream (id=" << stream_id
                << ") no longer exists.";
  OnStreamError(stream_id);
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler
bool AudioRendererHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioRendererHost, message)
    IPC_MESSAGE_HANDLER(AudioHostMsg_RequestDeviceAuthorization,
                        OnRequestDeviceAuthorization)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CreateStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PlayStream, OnPlayStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PauseStream, OnPauseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CloseStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_SetVolume, OnSetVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AudioRendererHost::OnRequestDeviceAuthorization(
    int stream_id,
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const base::TimeTicks auth_start_time = base::TimeTicks::Now();
  DVLOG(1) << "AudioRendererHost@" << this << "::OnRequestDeviceAuthorization"
           << "(stream_id=" << stream_id
           << ", render_frame_id=" << render_frame_id
           << ", session_id=" << session_id << ", device_id=" << device_id
           << ", security_origin=" << security_origin << ")";

  if (LookupById(stream_id) || IsAuthorizationStarted(stream_id))
    return;

  authorizations_.insert(
      std::make_pair(stream_id, std::make_pair(false, std::string())));

  // Unretained is ok here since |this| owns |authorization_handler_| and
  // |authorization_handler_| owns the callback.
  authorization_handler_.RequestDeviceAuthorization(
      render_frame_id, session_id, device_id,
      base::BindOnce(&AudioRendererHost::AuthorizationCompleted,
                     base::Unretained(this), stream_id, auth_start_time));
}

void AudioRendererHost::AuthorizationCompleted(
    int stream_id,
    base::TimeTicks auth_start_time,
    media::OutputDeviceStatus status,
    const media::AudioParameters& params,
    const std::string& raw_device_id,
    const std::string& device_id_for_renderer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  UMALogDeviceAuthorizationTime(auth_start_time);

  auto auth_data = authorizations_.find(stream_id);
  if (auth_data == authorizations_.end())
    return;  // Stream was closed before finishing authorization

  if (status == media::OUTPUT_DEVICE_STATUS_OK) {
    auth_data->second.first = true;
    auth_data->second.second = raw_device_id;
    Send(new AudioMsg_NotifyDeviceAuthorized(stream_id, status, params,
                                             device_id_for_renderer));
  } else {
    authorizations_.erase(auth_data);
    Send(new AudioMsg_NotifyDeviceAuthorized(
        stream_id, status, media::AudioParameters::UnavailableDeviceParams(),
        std::string()));
  }
}

void AudioRendererHost::OnCreateStream(int stream_id,
                                       int render_frame_id,
                                       const media::AudioParameters& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "AudioRendererHost@" << this << "::OnCreateStream"
           << "(stream_id=" << stream_id << ")";

  // Determine whether to use the device_unique_id from an authorization, or an
  // empty string (i.e., when no previous authorization was requested, assume
  // default device).
  std::string device_unique_id;
  const auto& auth_data = authorizations_.find(stream_id);
  if (auth_data != authorizations_.end()) {
    if (!auth_data->second.first) {
      // The authorization for this stream is still pending, so it's an error
      // to create it now.
      content::bad_message::ReceivedBadMessage(
          this, bad_message::ARH_CREATED_STREAM_WITHOUT_AUTHORIZATION);
      return;
    }
    device_unique_id.swap(auth_data->second.second);
    authorizations_.erase(auth_data);
  }

  // Fail early if either of two sanity-checks fail:
  //   1. There should not yet exist an AudioOutputDelegate for the given
  //      |stream_id| since the renderer may not create two streams with the
  //      same ID.
  //   2. An out-of-range render frame ID was provided. Renderers must *always*
  //      specify a valid render frame ID for each audio output they create, as
  //      several browser-level features depend on this (e.g., OOM manager, UI
  //      audio indicator, muting, audio capture).
  // Note: media::AudioParameters is validated in the deserializer, so there is
  // no need to check that here.
  if (LookupById(stream_id)) {
    SendErrorMessage(stream_id);
    return;
  }
  if (render_frame_id <= 0) {
    SendErrorMessage(stream_id);
    return;
  }

  // Post a task to the UI thread to check that the |render_frame_id| references
  // a valid render frame. This validation is important for all the reasons
  // stated in the comments above. This does not block stream creation, but will
  // force-close the stream later if validation fails.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ValidateRenderFrameId, render_process_id_,
                     render_frame_id,
                     base::BindOnce(&AudioRendererHost::DidValidateRenderFrame,
                                    this, stream_id)));

  MediaObserver* const media_observer =
      GetContentClient()->browser()->GetMediaObserver();

  MediaInternals* const media_internals = MediaInternals::GetInstance();
  std::unique_ptr<media::AudioLog> audio_log = media_internals->CreateAudioLog(
      media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER);
  audio_log->OnCreated(stream_id, params, device_unique_id);
  media_internals->SetWebContentsTitleForAudioLogEntry(
      stream_id, render_process_id_, render_frame_id, audio_log.get());
  auto delegate = AudioOutputDelegateImpl::Create(
      this, audio_manager_, std::move(audio_log), mirroring_manager_,
      media_observer, stream_id, render_frame_id, render_process_id_, params,
      device_unique_id);
  if (delegate)
    delegates_.push_back(std::move(delegate));
  else
    SendErrorMessage(stream_id);
}

void AudioRendererHost::OnPlayStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media::AudioOutputDelegate* delegate = LookupById(stream_id);
  if (!delegate) {
    SendErrorMessage(stream_id);
    return;
  }

  delegate->OnPlayStream();
}

void AudioRendererHost::OnPauseStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media::AudioOutputDelegate* delegate = LookupById(stream_id);
  if (!delegate) {
    SendErrorMessage(stream_id);
    return;
  }

  delegate->OnPauseStream();
}

void AudioRendererHost::OnSetVolume(int stream_id, double volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media::AudioOutputDelegate* delegate = LookupById(stream_id);
  if (!delegate) {
    SendErrorMessage(stream_id);
    return;
  }

  // Make sure the volume is valid.
  if (volume < 0 || volume > 1.0)
    return;
  delegate->OnSetVolume(volume);
}

void AudioRendererHost::SendErrorMessage(int stream_id) {
  Send(new AudioMsg_NotifyStreamError(stream_id));
}

void AudioRendererHost::OnCloseStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  authorizations_.erase(stream_id);

  auto i = LookupIteratorById(stream_id);

  // Prevent oustanding callbacks from attempting to close/delete the same
  // AudioOutputDelegate twice.
  if (i == delegates_.end())
    return;

  std::swap(*i, delegates_.back());
  delegates_.pop_back();
}

AudioRendererHost::AudioOutputDelegateVector::iterator
AudioRendererHost::LookupIteratorById(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return std::find_if(
      delegates_.begin(), delegates_.end(),
      [stream_id](const std::unique_ptr<media::AudioOutputDelegate>& d) {
        return d->GetStreamId() == stream_id;
      });
}

media::AudioOutputDelegate* AudioRendererHost::LookupById(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto i = LookupIteratorById(stream_id);
  return i != delegates_.end() ? i->get() : nullptr;
}

bool AudioRendererHost::IsAuthorizationStarted(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return authorizations_.find(stream_id) != authorizations_.end();
}

void AudioRendererHost::OverrideDevicePermissionsForTesting(bool has_access) {
  authorization_handler_.OverridePermissionsForTesting(has_access);
}
}  // namespace content
