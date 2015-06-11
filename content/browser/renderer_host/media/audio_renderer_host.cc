// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_renderer_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/process/process.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_sync_reader.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/media/audio_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"

using media::AudioBus;
using media::AudioManager;

namespace content {

namespace {
// TODO(aiolos): This is a temporary hack until the resource scheduler is
// migrated to RenderFrames for the Site Isolation project. It's called in
// response to low frequency playback state changes. http://crbug.com/472869
int RenderFrameIdToRenderViewId(int render_process_id, int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* const frame =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  return frame ? frame->GetRenderViewHost()->GetRoutingID() : MSG_ROUTING_NONE;
}

void NotifyResourceDispatcherOfAudioStateChange(int render_process_id,
                                                bool is_playing,
                                                int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (render_view_id == MSG_ROUTING_NONE || !ResourceDispatcherHostImpl::Get())
    return;

  ResourceDispatcherHostImpl::Get()->OnAudioRenderHostStreamStateChanged(
      render_process_id, render_view_id, is_playing);
}

}  // namespace

class AudioRendererHost::AudioEntry
    : public media::AudioOutputController::EventHandler {
 public:
  AudioEntry(AudioRendererHost* host,
             int stream_id,
             int render_frame_id,
             const media::AudioParameters& params,
             const std::string& output_device_id,
             scoped_ptr<base::SharedMemory> shared_memory,
             scoped_ptr<media::AudioOutputController::SyncReader> reader);
  ~AudioEntry() override;

  int stream_id() const {
    return stream_id_;
  }

  int render_frame_id() const { return render_frame_id_; }

  media::AudioOutputController* controller() const { return controller_.get(); }

  base::SharedMemory* shared_memory() {
    return shared_memory_.get();
  }

  media::AudioOutputController::SyncReader* reader() const {
    return reader_.get();
  }

  bool playing() const { return playing_; }
  void set_playing(bool playing) { playing_ = playing; }

 private:
  // media::AudioOutputController::EventHandler implementation.
  void OnCreated() override;
  void OnPlaying() override;
  void OnPaused() override;
  void OnError() override;

  AudioRendererHost* const host_;
  const int stream_id_;

  // The routing ID of the source RenderFrame.
  const int render_frame_id_;

  // Shared memory for transmission of the audio data.  Used by |reader_|.
  const scoped_ptr<base::SharedMemory> shared_memory_;

  // The synchronous reader to be used by |controller_|.
  const scoped_ptr<media::AudioOutputController::SyncReader> reader_;

  // The AudioOutputController that manages the audio stream.
  const scoped_refptr<media::AudioOutputController> controller_;

  bool playing_;
};

AudioRendererHost::AudioEntry::AudioEntry(
    AudioRendererHost* host,
    int stream_id,
    int render_frame_id,
    const media::AudioParameters& params,
    const std::string& output_device_id,
    scoped_ptr<base::SharedMemory> shared_memory,
    scoped_ptr<media::AudioOutputController::SyncReader> reader)
    : host_(host),
      stream_id_(stream_id),
      render_frame_id_(render_frame_id),
      shared_memory_(shared_memory.Pass()),
      reader_(reader.Pass()),
      controller_(media::AudioOutputController::Create(host->audio_manager_,
                                                       this,
                                                       params,
                                                       output_device_id,
                                                       reader_.get())),
      playing_(false) {
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
    MediaStreamManager* media_stream_manager,
    const ResourceContext::SaltCallback& salt_callback)
    : BrowserMessageFilter(AudioMsgStart),
      render_process_id_(render_process_id),
      audio_manager_(audio_manager),
      mirroring_manager_(mirroring_manager),
      audio_log_(media_internals->CreateAudioLog(
          media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER)),
      media_stream_manager_(media_stream_manager),
      num_playing_streams_(0),
      salt_callback_(salt_callback) {
  DCHECK(audio_manager_);
  DCHECK(media_stream_manager_);
}

AudioRendererHost::~AudioRendererHost() {
  DCHECK(audio_entries_.empty());
}

void AudioRendererHost::GetOutputControllers(
    const RenderProcessHost::GetAudioOutputControllersCallback&
        callback) const {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioRendererHost::DoGetOutputControllers, this), callback);
}

void AudioRendererHost::OnChannelClosing() {
  // Since the IPC sender is gone, close all requested audio streams.
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
      base::Bind(&AudioRendererHost::DoNotifyStreamStateChanged,
                 host_,
                 stream_id_,
                 true));
}

void AudioRendererHost::AudioEntry::OnPaused() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::DoNotifyStreamStateChanged,
                 host_,
                 stream_id_,
                 false));
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

  base::SyncSocket::TransitDescriptor socket_descriptor;

  // If we failed to prepare the sync socket for the renderer then we fail
  // the construction of audio stream.
  if (!reader->PrepareForeignSocket(PeerHandle(), &socket_descriptor)) {
    ReportErrorAndClose(entry->stream_id());
    return;
  }

  Send(new AudioMsg_NotifyStreamCreated(
      entry->stream_id(), foreign_memory_handle, socket_descriptor,
      entry->shared_memory()->requested_size()));
}

void AudioRendererHost::DoNotifyStreamStateChanged(int stream_id,
                                                   bool is_playing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* const entry = LookupById(stream_id);
  if (!entry)
    return;

  Send(new AudioMsg_NotifyStreamStateChanged(
      stream_id,
      is_playing ? media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_PLAYING
                 : media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_PAUSED));

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
    IPC_MESSAGE_HANDLER(AudioHostMsg_CreateStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PlayStream, OnPlayStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PauseStream, OnPauseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CloseStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_SetVolume, OnSetVolume)
    IPC_MESSAGE_HANDLER(AudioHostMsg_SwitchOutputDevice, OnSwitchOutputDevice)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AudioRendererHost::OnCreateStream(int stream_id,
                                       int render_frame_id,
                                       int session_id,
                                       const media::AudioParameters& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DVLOG(1) << "AudioRendererHost@" << this
           << "::OnCreateStream(stream_id=" << stream_id
           << ", render_frame_id=" << render_frame_id
           << ", session_id=" << session_id << ")";
  DCHECK_GT(render_frame_id, 0);

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
  uint32 shared_memory_size = AudioBus::CalculateMemorySize(params);
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

  scoped_ptr<AudioEntry> entry(new AudioEntry(this,
                                              stream_id,
                                              render_frame_id,
                                              params,
                                              output_device_id,
                                              shared_memory.Pass(),
                                              reader.Pass()));
  if (mirroring_manager_) {
    mirroring_manager_->AddDiverter(
        render_process_id_, entry->render_frame_id(), entry->controller());
  }
  audio_entries_.insert(std::make_pair(stream_id, entry.release()));
  audio_log_->OnCreated(stream_id, params, output_device_id);
  MediaInternals::GetInstance()->SetWebContentsTitleForAudioLogEntry(
      stream_id, render_process_id_, render_frame_id, audio_log_.get());
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

void AudioRendererHost::OnSwitchOutputDevice(int stream_id,
                                             int render_frame_id,
                                             const std::string& device_id,
                                             const GURL& security_origin,
                                             int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "AudioRendererHost@" << this
           << "::OnSwitchOutputDevice(stream_id=" << stream_id
           << ", render_frame_id=" << render_frame_id
           << ", device_id=" << device_id
           << ", security_origin=" << security_origin
           << ", request_id=" << request_id << ")";
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          render_process_id_, security_origin)) {
    content::bad_message::ReceivedBadMessage(this,
                                             bad_message::ARH_UNAUTHORIZED_URL);
    return;
  }

  if (device_id.empty()) {
    DVLOG(1) << __FUNCTION__ << ": default output device requested. "
             << "No permissions check or device translation/validation needed.";
    DoSwitchOutputDevice(stream_id, device_id, request_id);
  } else {
    // Check that MediaStream device permissions have been granted,
    // hence the use of a MediaStreamUIProxy.
    scoped_ptr<MediaStreamUIProxy> ui_proxy = MediaStreamUIProxy::Create();

    // Use MEDIA_DEVICE_AUDIO_CAPTURE instead of MEDIA_DEVICE_AUDIO_OUTPUT
    // because MediaStreamUIProxy::CheckAccess does not currently support
    // MEDIA_DEVICE_AUDIO_OUTPUT.
    // TODO(guidou): Change to MEDIA_DEVICE_AUDIO_OUTPUT when support becomes
    // available. http://crbug.com/498675
    ui_proxy->CheckAccess(
        security_origin, MEDIA_DEVICE_AUDIO_CAPTURE,
        render_process_id_, render_frame_id,
        base::Bind(&AudioRendererHost::OutputDeviceAccessChecked, this,
                   base::Passed(&ui_proxy), stream_id, device_id,
                   security_origin, render_frame_id, request_id));
  }
}

void AudioRendererHost::OutputDeviceAccessChecked(
    scoped_ptr<MediaStreamUIProxy> ui_proxy,
    int stream_id,
    const std::string& device_id,
    const GURL& security_origin,
    int render_frame_id,
    int request_id,
    bool have_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << __FUNCTION__;
  if (!have_access) {
    DVLOG(0) << __FUNCTION__
             << ": Have no access to media devices. Not switching device.";
    Send(new AudioMsg_NotifyOutputDeviceSwitched(
        stream_id, request_id,
        media::SWITCH_OUTPUT_DEVICE_RESULT_ERROR_NOT_AUTHORIZED));
    return;
  }

  scoped_refptr<base::SingleThreadTaskRunner> audio_worker_runner =
      AudioManager::Get()->GetWorkerTaskRunner();
  audio_worker_runner->PostTask(
      FROM_HERE,
      base::Bind(&AudioRendererHost::StartTranslateOutputDeviceName, this,
                 stream_id, device_id, security_origin, request_id));
}

void AudioRendererHost::StartTranslateOutputDeviceName(
    int stream_id,
    const std::string& device_id,
    const GURL& security_origin,
    int request_id) {
  DCHECK(AudioManager::Get()->GetWorkerTaskRunner()->BelongsToCurrentThread());
  DCHECK(!device_id.empty());
  DVLOG(1) << __FUNCTION__;

  media::AudioDeviceNames* device_names(new media::AudioDeviceNames);
  AudioManager::Get()->GetAudioOutputDeviceNames(device_names);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioRendererHost::FinishTranslateOutputDeviceName, this,
                 stream_id, device_id, security_origin, request_id,
                 base::Owned(device_names)));
}

void AudioRendererHost::FinishTranslateOutputDeviceName(
    int stream_id,
    const std::string& device_id,
    const GURL& security_origin,
    int request_id,
    media::AudioDeviceNames* device_names) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!device_id.empty());
  DVLOG(1) << __FUNCTION__;

  std::string raw_device_id;
  // Process the enumeration here because |salt_callback_| can run
  // only on the IO thread
  for (const auto& device_name : *device_names) {
    const std::string candidate_device_id = content::GetHMACForMediaDeviceID(
        salt_callback_, security_origin, device_name.unique_id);
    if (candidate_device_id == device_id) {
      DVLOG(1) << "Requested device " << device_name.unique_id << " - "
               << device_name.device_name;
      raw_device_id = device_name.unique_id;
    }
  }

  if (raw_device_id.empty()) {
    DVLOG(1) << "Requested device " << device_id << " could not be found.";
    Send(new AudioMsg_NotifyOutputDeviceSwitched(
        stream_id, request_id,
        media::SWITCH_OUTPUT_DEVICE_RESULT_ERROR_NOT_FOUND));
    return;
  }

  DoSwitchOutputDevice(stream_id, raw_device_id, request_id);
}

void AudioRendererHost::DoSwitchOutputDevice(int stream_id,
                                             const std::string& raw_device_id,
                                             int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << __FUNCTION__ << "(" << stream_id << ", " << raw_device_id << ", "
           << request_id << ")";
  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    Send(new AudioMsg_NotifyOutputDeviceSwitched(
        stream_id, request_id,
        media::SWITCH_OUTPUT_DEVICE_RESULT_ERROR_OBSOLETE));
    return;
  }

  entry->controller()->SwitchOutputDevice(
      raw_device_id, base::Bind(&AudioRendererHost::DoOutputDeviceSwitched,
                                this, stream_id, request_id));
  audio_log_->OnSwitchOutputDevice(entry->stream_id(), raw_device_id);
}

void AudioRendererHost::DoOutputDeviceSwitched(int stream_id, int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << __FUNCTION__ << "(" << stream_id << ", " << request_id << ")";
  Send(new AudioMsg_NotifyOutputDeviceSwitched(
      stream_id, request_id, media::SWITCH_OUTPUT_DEVICE_RESULT_SUCCESS));
}

void AudioRendererHost::SendErrorMessage(int stream_id) {
  Send(new AudioMsg_NotifyStreamStateChanged(
      stream_id, media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_ERROR));
}

void AudioRendererHost::OnCloseStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Prevent oustanding callbacks from attempting to close/delete the same
  // AudioEntry twice.
  AudioEntryMap::iterator i = audio_entries_.find(stream_id);
  if (i == audio_entries_.end())
    return;
  scoped_ptr<AudioEntry> entry(i->second);
  audio_entries_.erase(i);

  media::AudioOutputController* const controller = entry->controller();
  controller->Close(
      base::Bind(&AudioRendererHost::DeleteEntry, this, base::Passed(&entry)));
  audio_log_->OnClosed(stream_id);
}

void AudioRendererHost::DeleteEntry(scoped_ptr<AudioEntry> entry) {
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

  bool should_alert_resource_scheduler;
  if (is_playing) {
    should_alert_resource_scheduler =
        !RenderFrameHasActiveAudio(entry->render_frame_id());
    entry->set_playing(true);
    base::AtomicRefCountInc(&num_playing_streams_);
  } else {
    entry->set_playing(false);
    should_alert_resource_scheduler =
        !RenderFrameHasActiveAudio(entry->render_frame_id());
    base::AtomicRefCountDec(&num_playing_streams_);
  }

  if (should_alert_resource_scheduler && ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RenderFrameIdToRenderViewId, render_process_id_,
                   entry->render_frame_id()),
        base::Bind(&NotifyResourceDispatcherOfAudioStateChange,
                   render_process_id_, is_playing));
  }
}

bool AudioRendererHost::HasActiveAudio() {
  return !base::AtomicRefCountIsZero(&num_playing_streams_);
}

bool AudioRendererHost::RenderFrameHasActiveAudio(int render_frame_id) const {
  for (AudioEntryMap::const_iterator it = audio_entries_.begin();
       it != audio_entries_.end();
       ++it) {
    AudioEntry* entry = it->second;
    if (entry->render_frame_id() == render_frame_id && entry->playing())
      return true;
  }
  return false;
}

}  // namespace content
