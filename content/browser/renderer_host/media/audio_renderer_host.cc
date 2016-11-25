// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_renderer_host.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_sync_reader.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/common/media/audio_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_streams_tracker.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"

using media::AudioBus;
using media::AudioManager;

namespace content {

namespace {

// Tracks the maximum number of simultaneous output streams browser-wide.
// Accessed on IO thread.
base::LazyInstance<media::AudioStreamsTracker> g_audio_streams_tracker =
    LAZY_INSTANCE_INITIALIZER;

void NotifyRenderProcessHostThatAudioStateChanged(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id);

  if (render_process_host)
    render_process_host->AudioStateChanged();
}

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
                           const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool frame_exists =
      !!RenderFrameHost::FromID(render_process_id, render_frame_id);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, frame_exists));
}

}  // namespace

class AudioRendererHost::AudioEntry
    : public media::AudioOutputController::EventHandler {
 public:
  ~AudioEntry() override;

  // Returns nullptr on failure.
  static std::unique_ptr<AudioRendererHost::AudioEntry> Create(
      AudioRendererHost* host,
      int stream_id,
      int render_frame_id,
      const media::AudioParameters& params,
      const std::string& output_device_id);

  int stream_id() const {
    return stream_id_;
  }

  int render_frame_id() const { return render_frame_id_; }

  media::AudioOutputController* controller() const { return controller_.get(); }

  AudioSyncReader* reader() const { return reader_.get(); }

  // Used by ARH to track the number of active streams for UMA stats.
  bool playing() const { return playing_; }
  void set_playing(bool playing) { playing_ = playing; }

 private:
  AudioEntry(AudioRendererHost* host,
             int stream_id,
             int render_frame_id,
             const media::AudioParameters& params,
             const std::string& output_device_id,
             std::unique_ptr<AudioSyncReader> reader);

  // media::AudioOutputController::EventHandler implementation.
  void OnCreated() override;
  void OnPlaying() override;
  void OnPaused() override;
  void OnError() override;

  AudioRendererHost* const host_;
  const int stream_id_;

  // The routing ID of the source RenderFrame.
  const int render_frame_id_;

  // The synchronous reader to be used by |controller_|.
  const std::unique_ptr<AudioSyncReader> reader_;

  // The AudioOutputController that manages the audio stream.
  const scoped_refptr<media::AudioOutputController> controller_;

  bool playing_;

  DISALLOW_COPY_AND_ASSIGN(AudioEntry);
};

AudioRendererHost::AudioEntry::AudioEntry(
    AudioRendererHost* host,
    int stream_id,
    int render_frame_id,
    const media::AudioParameters& params,
    const std::string& output_device_id,
    std::unique_ptr<AudioSyncReader> reader)
    : host_(host),
      stream_id_(stream_id),
      render_frame_id_(render_frame_id),
      reader_(std::move(reader)),
      controller_(media::AudioOutputController::Create(host->audio_manager_,
                                                       this,
                                                       params,
                                                       output_device_id,
                                                       reader_.get())),
      playing_(false) {
  DCHECK(controller_);
}

AudioRendererHost::AudioEntry::~AudioEntry() {}

// static
std::unique_ptr<AudioRendererHost::AudioEntry>
AudioRendererHost::AudioEntry::Create(AudioRendererHost* host,
                                      int stream_id,
                                      int render_frame_id,
                                      const media::AudioParameters& params,
                                      const std::string& output_device_id) {
  std::unique_ptr<AudioSyncReader> reader(AudioSyncReader::Create(params));
  if (!reader) {
    return nullptr;
  }
  return base::WrapUnique(new AudioEntry(host, stream_id, render_frame_id,
                                         params, output_device_id,
                                         std::move(reader)));
}

///////////////////////////////////////////////////////////////////////////////
// AudioRendererHost implementations.

AudioRendererHost::AudioRendererHost(int render_process_id,
                                     media::AudioManager* audio_manager,
                                     AudioMirroringManager* mirroring_manager,
                                     MediaInternals* media_internals,
                                     MediaStreamManager* media_stream_manager,
                                     const std::string& salt)
    : BrowserMessageFilter(AudioMsgStart),
      render_process_id_(render_process_id),
      audio_manager_(audio_manager),
      mirroring_manager_(mirroring_manager),
      audio_log_(media_internals->CreateAudioLog(
          media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER)),
      num_playing_streams_(0),
      salt_(salt),
      validate_render_frame_id_function_(&ValidateRenderFrameId),
      max_simultaneous_streams_(0),
      authorization_handler_(audio_manager_,
                             media_stream_manager,
                             render_process_id_,
                             salt) {
  DCHECK(audio_manager_);
}

AudioRendererHost::~AudioRendererHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(audio_entries_.empty());

  // If we had any streams, report UMA stats for the maximum number of
  // simultaneous streams for this render process and for the whole browser
  // process since last reported.
  if (max_simultaneous_streams_ > 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.AudioRendererIpcStreams",
                                max_simultaneous_streams_, 1, 50, 51);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Media.AudioRendererIpcStreamsTotal",
        g_audio_streams_tracker.Get().max_stream_count(),
        1, 100, 101);
    g_audio_streams_tracker.Get().ResetMaxStreamCount();
  }
}

void AudioRendererHost::GetOutputControllers(
    const RenderProcessHost::GetAudioOutputControllersCallback&
        callback) const {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioRendererHost::DoGetOutputControllers, this), callback);
}

void AudioRendererHost::OnChannelClosing() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Since the IPC sender is gone, close all requested audio streams.
  while (!audio_entries_.empty()) {
    // Note: OnCloseStream() removes the entries from audio_entries_.
    OnCloseStream(audio_entries_.begin()->first);
  }

  // Remove any authorizations for streams that were not yet created
  authorizations_.clear();
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
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioRendererHost::StreamStateChanged,
                                     host_, stream_id_, true));
}

void AudioRendererHost::AudioEntry::OnPaused() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioRendererHost::StreamStateChanged,
                                     host_, stream_id_, false));
}

void AudioRendererHost::AudioEntry::OnError() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::ReportErrorAndClose, host_, stream_id_));
}

void AudioRendererHost::DoCompleteCreation(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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

  // Now construction is done and we are ready to send the shared memory and the
  // sync socket to the renderer.
  base::SharedMemory* shared_memory = entry->reader()->shared_memory();
  base::CancelableSyncSocket* foreign_socket =
      entry->reader()->foreign_socket();

  base::SharedMemoryHandle foreign_memory_handle;
  base::SyncSocket::TransitDescriptor socket_descriptor;
  size_t shared_memory_size = shared_memory->requested_size();

  if (!(shared_memory->ShareToProcess(PeerHandle(), &foreign_memory_handle) &&
        foreign_socket->PrepareTransitDescriptor(PeerHandle(),
                                                 &socket_descriptor))) {
    // Something went wrong in preparing the IPC handles.
    ReportErrorAndClose(entry->stream_id());
    return;
  }

  Send(new AudioMsg_NotifyStreamCreated(
      stream_id, foreign_memory_handle, socket_descriptor,
      base::checked_cast<uint32_t>(shared_memory_size)));
}

void AudioRendererHost::DidValidateRenderFrame(int stream_id, bool is_valid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!is_valid) {
    DLOG(WARNING) << "Render frame for stream (id=" << stream_id
                  << ") no longer exists.";
    ReportErrorAndClose(stream_id);
  }
}

void AudioRendererHost::StreamStateChanged(int stream_id, bool is_playing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* const entry = LookupById(stream_id);
  if (!entry)
    return;

  if (is_playing) {
    AudioStreamMonitor::StartMonitoringStream(
        render_process_id_,
        entry->render_frame_id(),
        entry->stream_id(),
        base::Bind(&media::AudioOutputController::ReadCurrentPowerAndClip,
                   entry->controller()));
  } else {
    AudioStreamMonitor::StopMonitoringStream(
        render_process_id_, entry->render_frame_id(), entry->stream_id());
  }
  UpdateNumPlayingStreams(entry, is_playing);
}

RenderProcessHost::AudioOutputControllerList
AudioRendererHost::DoGetOutputControllers() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RenderProcessHost::AudioOutputControllerList controllers;
  for (AudioEntryMap::const_iterator it = audio_entries_.begin();
       it != audio_entries_.end();
       ++it) {
    controllers.push_back(it->second->controller());
  }

  return controllers;
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
      render_frame_id, session_id, device_id, security_origin,
      base::Bind(&AudioRendererHost::AuthorizationCompleted,
                 base::Unretained(this), stream_id, security_origin,
                 auth_start_time));
}

void AudioRendererHost::AuthorizationCompleted(
    int stream_id,
    const url::Origin& security_origin,
    base::TimeTicks auth_start_time,
    media::OutputDeviceStatus status,
    bool should_send_id,
    const media::AudioParameters& params,
    const std::string& raw_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto auth_data = authorizations_.find(stream_id);
  if (auth_data == authorizations_.end())
    return;  // Stream was closed before finishing authorization

  UMALogDeviceAuthorizationTime(auth_start_time);
  if (status == media::OUTPUT_DEVICE_STATUS_OK) {
    auth_data->second.first = true;
    auth_data->second.second = raw_device_id;
    if (should_send_id) {
      std::string hashed_id = MediaStreamManager::GetHMACForMediaDeviceID(
          salt_, security_origin, raw_device_id);
      Send(new AudioMsg_NotifyDeviceAuthorized(stream_id, status, params,
                                               hashed_id));
    } else {
      Send(new AudioMsg_NotifyDeviceAuthorized(stream_id, status, params,
                                               std::string()));
    }
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
  //   1. There should not yet exist an AudioEntry for the given |stream_id|
  //      since the renderer may not create two streams with the same ID.
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
      base::Bind(validate_render_frame_id_function_, render_process_id_,
                 render_frame_id,
                 base::Bind(&AudioRendererHost::DidValidateRenderFrame, this,
                            stream_id)));

  std::unique_ptr<AudioEntry> entry = AudioEntry::Create(
      this, stream_id, render_frame_id, params, device_unique_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  MediaObserver* const media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer)
    media_observer->OnCreatingAudioStream(render_process_id_, render_frame_id);

  if (mirroring_manager_) {
    mirroring_manager_->AddDiverter(
        render_process_id_, entry->render_frame_id(), entry->controller());
  }
  audio_entries_.insert(std::make_pair(stream_id, entry.release()));
  g_audio_streams_tracker.Get().IncreaseStreamCount();

  audio_log_->OnCreated(stream_id, params, device_unique_id);
  MediaInternals::GetInstance()->SetWebContentsTitleForAudioLogEntry(
      stream_id, render_process_id_, render_frame_id, audio_log_.get());

  if (audio_entries_.size() > max_simultaneous_streams_)
    max_simultaneous_streams_ = audio_entries_.size();
}

void AudioRendererHost::OnPlayStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller()->Play();
  audio_log_->OnStarted(stream_id);
}

void AudioRendererHost::OnPauseStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller()->Pause();
  audio_log_->OnStopped(stream_id);
}

void AudioRendererHost::OnSetVolume(int stream_id, double volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
  Send(new AudioMsg_NotifyStreamError(stream_id));
}

void AudioRendererHost::OnCloseStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  authorizations_.erase(stream_id);

  // Prevent oustanding callbacks from attempting to close/delete the same
  // AudioEntry twice.
  AudioEntryMap::iterator i = audio_entries_.find(stream_id);
  if (i == audio_entries_.end())
    return;
  std::unique_ptr<AudioEntry> entry(i->second);
  audio_entries_.erase(i);
  g_audio_streams_tracker.Get().DecreaseStreamCount();

  media::AudioOutputController* const controller = entry->controller();
  controller->Close(
      base::Bind(&AudioRendererHost::DeleteEntry, this, base::Passed(&entry)));
  audio_log_->OnClosed(stream_id);
}

void AudioRendererHost::DeleteEntry(std::unique_ptr<AudioEntry> entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // De-register the controller from the AudioMirroringManager now that the
  // controller has closed the AudioOutputStream and shut itself down.  This
  // ensures that calling RemoveDiverter() here won't trigger the controller to
  // re-start the default AudioOutputStream and cause a brief audio blip to come
  // out the user's speakers.  http://crbug.com/474432
  if (mirroring_manager_)
    mirroring_manager_->RemoveDiverter(entry->controller());

  AudioStreamMonitor::StopMonitoringStream(
      render_process_id_, entry->render_frame_id(), entry->stream_id());
  UpdateNumPlayingStreams(entry.get(), false);
}

void AudioRendererHost::ReportErrorAndClose(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntryMap::const_iterator i = audio_entries_.find(stream_id);
  return i != audio_entries_.end() ? i->second : NULL;
}

void AudioRendererHost::UpdateNumPlayingStreams(AudioEntry* entry,
                                                bool is_playing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (entry->playing() == is_playing)
    return;

  if (is_playing) {
    entry->set_playing(true);
    base::AtomicRefCountInc(&num_playing_streams_);

    // Inform the RenderProcessHost when audio starts playing for the first
    // time.
    if (base::AtomicRefCountIsOne(&num_playing_streams_)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&NotifyRenderProcessHostThatAudioStateChanged,
                     render_process_id_));
    }
  } else {
    entry->set_playing(false);
    // Inform the RenderProcessHost when there is no more audio playing.
    if (!base::AtomicRefCountDec(&num_playing_streams_)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&NotifyRenderProcessHostThatAudioStateChanged,
                     render_process_id_));
    }
  }
}

bool AudioRendererHost::IsAuthorizationStarted(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return authorizations_.find(stream_id) != authorizations_.end();
}

bool AudioRendererHost::HasActiveAudio() {
  return !base::AtomicRefCountIsZero(&num_playing_streams_);
}

void AudioRendererHost::OverrideDevicePermissionsForTesting(bool has_access) {
  authorization_handler_.OverridePermissionsForTesting(has_access);
}
}  // namespace content
