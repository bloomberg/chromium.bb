// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/user_media_processor.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/media/media_stream_controls.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/local_media_stream_audio_source.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_constraints_util_audio.h"
#include "content/renderer/media/media_stream_constraints_util_video_content.h"
#include "content/renderer/media/media_stream_constraints_util_video_device.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/user_media_client_impl.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/processed_local_audio_source.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "media/base/audio_parameters.h"
#include "media/capture/video_capture_types.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "url/origin.h"

namespace content {
namespace {

void CopyFirstString(const blink::StringConstraint& constraint,
                     std::string* destination) {
  if (!constraint.Exact().IsEmpty())
    *destination = constraint.Exact()[0].Utf8();
}

bool IsDeviceSource(const std::string& source) {
  return source.empty();
}

void InitializeTrackControls(const blink::WebMediaConstraints& constraints,
                             TrackControls* track_controls) {
  DCHECK(!constraints.IsNull());
  track_controls->requested = true;
  CopyFirstString(constraints.Basic().media_stream_source,
                  &track_controls->stream_source);
}

bool IsSameDevice(const MediaStreamDevice& device,
                  const MediaStreamDevice& other_device) {
  return device.id == other_device.id && device.type == other_device.type &&
         device.session_id == other_device.session_id;
}

bool IsSameSource(const blink::WebMediaStreamSource& source,
                  const blink::WebMediaStreamSource& other_source) {
  MediaStreamSource* const source_extra_data =
      static_cast<MediaStreamSource*>(source.GetExtraData());
  const MediaStreamDevice& device = source_extra_data->device();

  MediaStreamSource* const other_source_extra_data =
      static_cast<MediaStreamSource*>(other_source.GetExtraData());
  const MediaStreamDevice& other_device = other_source_extra_data->device();

  return IsSameDevice(device, other_device);
}

bool IsValidAudioContentSource(const std::string& source) {
  return source == kMediaStreamSourceTab ||
         source == kMediaStreamSourceDesktop ||
         source == kMediaStreamSourceSystem;
}

bool IsValidVideoContentSource(const std::string& source) {
  return source == kMediaStreamSourceTab ||
         source == kMediaStreamSourceDesktop ||
         source == kMediaStreamSourceScreen;
}

void SurfaceHardwareEchoCancellationSetting(
    blink::WebMediaStreamSource* source) {
  MediaStreamAudioSource* source_impl =
      static_cast<MediaStreamAudioSource*>(source->GetExtraData());
  media::AudioParameters params = source_impl->GetAudioParameters();
  if (params.IsValid() &&
      (params.effects() & media::AudioParameters::ECHO_CANCELLER))
    source->SetEchoCancellation(true);
}

}  // namespace

UserMediaRequest::UserMediaRequest(
    int request_id,
    const blink::WebUserMediaRequest& web_request,
    bool is_processing_user_gesture)
    : request_id(request_id),
      web_request(web_request),
      is_processing_user_gesture(is_processing_user_gesture) {}

// Class for storing state of the the processing of getUserMedia requests.
class UserMediaProcessor::RequestInfo
    : public base::SupportsWeakPtr<RequestInfo> {
 public:
  using ResourcesReady =
      base::Callback<void(RequestInfo* request_info,
                          MediaStreamRequestResult result,
                          const blink::WebString& result_name)>;
  enum class State {
    NOT_SENT_FOR_GENERATION,
    SENT_FOR_GENERATION,
    GENERATED,
  };

  explicit RequestInfo(std::unique_ptr<UserMediaRequest> request);

  void StartAudioTrack(const blink::WebMediaStreamTrack& track,
                       bool is_pending);
  blink::WebMediaStreamTrack CreateAndStartVideoTrack(
      const blink::WebMediaStreamSource& source);

  // Triggers |callback| when all sources used in this request have either
  // successfully started, or a source has failed to start.
  void CallbackOnTracksStarted(const ResourcesReady& callback);

  bool HasPendingSources() const;

  // Called when a local audio source has finished (or failed) initializing.
  void OnAudioSourceStarted(MediaStreamSource* source,
                            MediaStreamRequestResult result,
                            const blink::WebString& result_name);

  UserMediaRequest* request() { return request_.get(); }
  int request_id() const { return request_->request_id; }

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }

  const AudioCaptureSettings& audio_capture_settings() const {
    return audio_capture_settings_;
  }
  bool is_audio_content_capture() const {
    return audio_capture_settings_.HasValue() && is_audio_content_capture_;
  }
  bool is_audio_device_capture() const {
    return audio_capture_settings_.HasValue() && !is_audio_content_capture_;
  }
  void SetAudioCaptureSettings(const AudioCaptureSettings& settings,
                               bool is_content_capture) {
    DCHECK(settings.HasValue());
    is_audio_content_capture_ = is_content_capture;
    audio_capture_settings_ = settings;
  }
  const VideoCaptureSettings& video_capture_settings() const {
    return video_capture_settings_;
  }
  void SetVideoCaptureSettings(const VideoCaptureSettings& settings,
                               bool is_content_capture) {
    DCHECK(settings.HasValue());
    is_video_content_capture_ = is_content_capture;
    video_capture_settings_ = settings;
  }

  blink::WebMediaStream* web_stream() { return &web_stream_; }

  const blink::WebUserMediaRequest& web_request() const {
    return request_->web_request;
  }

  StreamControls* stream_controls() { return &stream_controls_; }

  bool is_processing_user_gesture() const {
    return request_->is_processing_user_gesture;
  }

  const url::Origin& security_origin() const { return security_origin_; }

 private:
  void OnTrackStarted(MediaStreamSource* source,
                      MediaStreamRequestResult result,
                      const blink::WebString& result_name);

  // Cheks if the sources for all tracks have been started and if so,
  // invoke the |ready_callback_|.  Note that the caller should expect
  // that |this| might be deleted when the function returns.
  void CheckAllTracksStarted();

  std::unique_ptr<UserMediaRequest> request_;
  State state_ = State::NOT_SENT_FOR_GENERATION;
  AudioCaptureSettings audio_capture_settings_;
  bool is_audio_content_capture_ = false;
  VideoCaptureSettings video_capture_settings_;
  bool is_video_content_capture_ = false;
  blink::WebMediaStream web_stream_;
  StreamControls stream_controls_;
  const url::Origin security_origin_;
  ResourcesReady ready_callback_;
  MediaStreamRequestResult request_result_ = MEDIA_DEVICE_OK;
  blink::WebString request_result_name_;
  // Sources used in this request.
  std::vector<blink::WebMediaStreamSource> sources_;
  std::vector<MediaStreamSource*> sources_waiting_for_callback_;
};

// TODO(guidou): Initialize request_result_name_ as a null blink::WebString.
// http://crbug.com/764293
UserMediaProcessor::RequestInfo::RequestInfo(
    std::unique_ptr<UserMediaRequest> request)
    : request_(std::move(request)),
      security_origin_(url::Origin(request_->web_request.GetSecurityOrigin())),
      request_result_name_("") {}

void UserMediaProcessor::RequestInfo::StartAudioTrack(
    const blink::WebMediaStreamTrack& track,
    bool is_pending) {
  DCHECK(track.Source().GetType() == blink::WebMediaStreamSource::kTypeAudio);
  DCHECK(web_request().Audio());
#if DCHECK_IS_ON()
  DCHECK(audio_capture_settings_.HasValue());
#endif
  MediaStreamAudioSource* native_source =
      MediaStreamAudioSource::From(track.Source());
  // Add the source as pending since OnTrackStarted will expect it to be there.
  sources_waiting_for_callback_.push_back(native_source);

  sources_.push_back(track.Source());
  bool connected = native_source->ConnectToTrack(track);
  if (!is_pending) {
    OnTrackStarted(
        native_source,
        connected ? MEDIA_DEVICE_OK : MEDIA_DEVICE_TRACK_START_FAILURE, "");
  }
}

blink::WebMediaStreamTrack
UserMediaProcessor::RequestInfo::CreateAndStartVideoTrack(
    const blink::WebMediaStreamSource& source) {
  DCHECK(source.GetType() == blink::WebMediaStreamSource::kTypeVideo);
  DCHECK(web_request().Video());
  DCHECK(video_capture_settings_.HasValue());
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  sources_.push_back(source);
  sources_waiting_for_callback_.push_back(native_source);
  return MediaStreamVideoTrack::CreateVideoTrack(
      native_source, video_capture_settings_.track_adapter_settings(),
      video_capture_settings_.noise_reduction(), is_video_content_capture_,
      video_capture_settings_.min_frame_rate(),
      base::Bind(&UserMediaProcessor::RequestInfo::OnTrackStarted, AsWeakPtr()),
      true);
}

void UserMediaProcessor::RequestInfo::CallbackOnTracksStarted(
    const ResourcesReady& callback) {
  DCHECK(ready_callback_.is_null());
  ready_callback_ = callback;
  CheckAllTracksStarted();
}

void UserMediaProcessor::RequestInfo::OnTrackStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DVLOG(1) << "OnTrackStarted result " << result;
  auto it = std::find(sources_waiting_for_callback_.begin(),
                      sources_waiting_for_callback_.end(), source);
  DCHECK(it != sources_waiting_for_callback_.end());
  sources_waiting_for_callback_.erase(it);
  // All tracks must be started successfully. Otherwise the request is a
  // failure.
  if (result != MEDIA_DEVICE_OK) {
    request_result_ = result;
    request_result_name_ = result_name;
  }

  CheckAllTracksStarted();
}

void UserMediaProcessor::RequestInfo::CheckAllTracksStarted() {
  if (!ready_callback_.is_null() && sources_waiting_for_callback_.empty()) {
    ready_callback_.Run(this, request_result_, request_result_name_);
    // NOTE: |this| might now be deleted.
  }
}

bool UserMediaProcessor::RequestInfo::HasPendingSources() const {
  return !sources_waiting_for_callback_.empty();
}

void UserMediaProcessor::RequestInfo::OnAudioSourceStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  // Check if we're waiting to be notified of this source.  If not, then we'll
  // ignore the notification.
  auto found = std::find(sources_waiting_for_callback_.begin(),
                         sources_waiting_for_callback_.end(), source);
  if (found != sources_waiting_for_callback_.end())
    OnTrackStarted(source, result, result_name);
}

UserMediaProcessor::UserMediaProcessor(
    RenderFrame* render_frame,
    PeerConnectionDependencyFactory* dependency_factory,
    std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher,
    MediaDevicesDispatcherCallback media_devices_dispatcher_cb,
    const scoped_refptr<base::TaskRunner>& worker_task_runner)
    : dependency_factory_(dependency_factory),
      media_stream_dispatcher_(std::move(media_stream_dispatcher)),
      media_devices_dispatcher_cb_(std::move(media_devices_dispatcher_cb)),
      worker_task_runner_(worker_task_runner),
      render_frame_(render_frame),
      weak_factory_(this) {
  DCHECK(dependency_factory_);
  DCHECK(media_stream_dispatcher_.get());
}

UserMediaProcessor::~UserMediaProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Force-close all outstanding user media requests and local sources here,
  // before the outstanding WeakPtrs are invalidated, to ensure a clean
  // shutdown.
  StopAllProcessing();
}

UserMediaRequest* UserMediaProcessor::CurrentRequest() {
  return current_request_info_ ? current_request_info_->request() : nullptr;
}

void UserMediaProcessor::ProcessRequest(
    std::unique_ptr<UserMediaRequest> request,
    base::OnceClosure callback) {
  DCHECK(!request_completed_cb_);
  DCHECK(!current_request_info_);
  request_completed_cb_ = std::move(callback);
  current_request_info_ = base::MakeUnique<RequestInfo>(std::move(request));
  // TODO(guidou): Set up audio and video in parallel.
  if (current_request_info_->web_request().Audio()) {
    SetupAudioInput();
    return;
  }
  SetupVideoInput();
}

void UserMediaProcessor::SetupAudioInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);
  DCHECK(current_request_info_->web_request().Audio());

  auto& audio_controls = current_request_info_->stream_controls()->audio;
  InitializeTrackControls(
      current_request_info_->web_request().AudioConstraints(), &audio_controls);
  if (IsDeviceSource(audio_controls.stream_source)) {
    GetMediaDevicesDispatcher()->GetAudioInputCapabilities(base::BindOnce(
        &UserMediaProcessor::SelectAudioSettings, weak_factory_.GetWeakPtr(),
        current_request_info_->web_request()));
  } else {
    if (!IsValidAudioContentSource(audio_controls.stream_source)) {
      blink::WebString failed_constraint_name =
          blink::WebString::FromASCII(current_request_info_->web_request()
                                          .AudioConstraints()
                                          .Basic()
                                          .media_stream_source.GetName());
      MediaStreamRequestResult result = MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
      GetUserMediaRequestFailed(result, failed_constraint_name);
      return;
    }
    SelectAudioSettings(current_request_info_->web_request(),
                        AudioDeviceCaptureCapabilities());
  }
}

void UserMediaProcessor::SelectAudioSettings(
    const blink::WebUserMediaRequest& web_request,
    std::vector<::mojom::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The frame might reload or |web_request| might be cancelled while
  // capabilities are queried. Do nothing if a different request is being
  // processed at this point.
  if (!IsCurrentRequestInfo(web_request))
    return;

  DCHECK(current_request_info_->stream_controls()->audio.requested);
  auto settings = SelectSettingsAudioCapture(
      std::move(audio_input_capabilities), web_request.AudioConstraints());
  if (!settings.HasValue()) {
    blink::WebString failed_constraint_name =
        blink::WebString::FromASCII(settings.failed_constraint_name());
    MediaStreamRequestResult result =
        failed_constraint_name.IsEmpty()
            ? MEDIA_DEVICE_NO_HARDWARE
            : MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
    GetUserMediaRequestFailed(result, failed_constraint_name);
    return;
  }
  current_request_info_->stream_controls()->audio.device_id =
      settings.device_id();
  current_request_info_->stream_controls()->disable_local_echo =
      settings.disable_local_echo();
  current_request_info_->stream_controls()->hotword_enabled =
      settings.hotword_enabled();
  current_request_info_->SetAudioCaptureSettings(
      settings,
      !IsDeviceSource(
          current_request_info_->stream_controls()->audio.stream_source));

  // No further audio setup required. Continue with video.
  SetupVideoInput();
}

void UserMediaProcessor::SetupVideoInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);

  if (!current_request_info_->web_request().Video()) {
    GenerateStreamForCurrentRequestInfo();
    return;
  }
  auto& video_controls = current_request_info_->stream_controls()->video;
  InitializeTrackControls(
      current_request_info_->web_request().VideoConstraints(), &video_controls);
  if (IsDeviceSource(video_controls.stream_source)) {
    GetMediaDevicesDispatcher()->GetVideoInputCapabilities(base::BindOnce(
        &UserMediaProcessor::SelectVideoDeviceSettings,
        weak_factory_.GetWeakPtr(), current_request_info_->web_request()));
  } else {
    if (!IsValidVideoContentSource(video_controls.stream_source)) {
      blink::WebString failed_constraint_name =
          blink::WebString::FromASCII(current_request_info_->web_request()
                                          .VideoConstraints()
                                          .Basic()
                                          .media_stream_source.GetName());
      MediaStreamRequestResult result = MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
      GetUserMediaRequestFailed(result, failed_constraint_name);
      return;
    }
    base::PostTaskAndReplyWithResult(
        worker_task_runner_.get(), FROM_HERE,
        base::Bind(&SelectSettingsVideoContentCapture,
                   current_request_info_->web_request().VideoConstraints(),
                   video_controls.stream_source),
        base::Bind(&UserMediaProcessor::FinalizeSelectVideoContentSettings,
                   weak_factory_.GetWeakPtr(),
                   current_request_info_->web_request()));
  }
}

void UserMediaProcessor::SelectVideoDeviceSettings(
    const blink::WebUserMediaRequest& web_request,
    std::vector<::mojom::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The frame might reload or |web_request| might be cancelled while
  // capabilities are queried. Do nothing if a different request is being
  // processed at this point.
  if (!IsCurrentRequestInfo(web_request))
    return;

  DCHECK(current_request_info_->stream_controls()->video.requested);
  DCHECK(IsDeviceSource(
      current_request_info_->stream_controls()->video.stream_source));

  VideoDeviceCaptureCapabilities capabilities;
  capabilities.device_capabilities = std::move(video_input_capabilities);
  capabilities.power_line_capabilities = {
      media::PowerLineFrequency::FREQUENCY_DEFAULT,
      media::PowerLineFrequency::FREQUENCY_50HZ,
      media::PowerLineFrequency::FREQUENCY_60HZ};
  capabilities.noise_reduction_capabilities = {base::Optional<bool>(),
                                               base::Optional<bool>(true),
                                               base::Optional<bool>(false)};
  base::PostTaskAndReplyWithResult(
      worker_task_runner_.get(), FROM_HERE,
      base::Bind(&SelectSettingsVideoDeviceCapture, std::move(capabilities),
                 web_request.VideoConstraints(),
                 MediaStreamVideoSource::kDefaultWidth,
                 MediaStreamVideoSource::kDefaultHeight,
                 MediaStreamVideoSource::kDefaultFrameRate),
      base::Bind(&UserMediaProcessor::FinalizeSelectVideoDeviceSettings,
                 weak_factory_.GetWeakPtr(), web_request));
}

void UserMediaProcessor::FinalizeSelectVideoDeviceSettings(
    const blink::WebUserMediaRequest& web_request,
    const VideoCaptureSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsCurrentRequestInfo(web_request))
    return;

  if (!settings.HasValue()) {
    blink::WebString failed_constraint_name =
        blink::WebString::FromASCII(settings.failed_constraint_name());
    MediaStreamRequestResult result =
        failed_constraint_name.IsEmpty()
            ? MEDIA_DEVICE_NO_HARDWARE
            : MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
    GetUserMediaRequestFailed(result, failed_constraint_name);
    return;
  }
  current_request_info_->stream_controls()->video.device_id =
      settings.device_id();
  current_request_info_->SetVideoCaptureSettings(
      settings, false /* is_content_capture */);
  GenerateStreamForCurrentRequestInfo();
}

void UserMediaProcessor::FinalizeSelectVideoContentSettings(
    const blink::WebUserMediaRequest& web_request,
    const VideoCaptureSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsCurrentRequestInfo(web_request))
    return;

  if (!settings.HasValue()) {
    blink::WebString failed_constraint_name =
        blink::WebString::FromASCII(settings.failed_constraint_name());
    DCHECK(!failed_constraint_name.IsEmpty());
    GetUserMediaRequestFailed(MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED,
                              failed_constraint_name);
    return;
  }
  current_request_info_->stream_controls()->video.device_id =
      settings.device_id();
  current_request_info_->SetVideoCaptureSettings(settings,
                                                 true /* is_content_capture */);
  GenerateStreamForCurrentRequestInfo();
}

void UserMediaProcessor::GenerateStreamForCurrentRequestInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);
  WebRtcLogMessage(base::StringPrintf(
      "UMCI::GenerateStreamForCurrentRequestInfo. request_id=%d, "
      "audio device id=\"%s\", video device id=\"%s\"",
      current_request_info_->request_id(),
      current_request_info_->stream_controls()->audio.device_id.c_str(),
      current_request_info_->stream_controls()->video.device_id.c_str()));
  current_request_info_->set_state(RequestInfo::State::SENT_FOR_GENERATION);

  // The browser replies to this request by invoking OnStreamGenerated() if
  // successful, or OnStreamGenerationFailed() if not.
  media_stream_dispatcher_->GenerateStream(
      current_request_info_->request_id(), weak_factory_.GetWeakPtr(),
      *current_request_info_->stream_controls(),
      current_request_info_->security_origin(),
      current_request_info_->is_processing_user_gesture());
}

// Callback from MediaStreamDispatcher.
// The requested stream have been generated by the MediaStreamDispatcher.
void UserMediaProcessor::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const MediaStreamDevices& audio_devices,
    const MediaStreamDevices& video_devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsCurrentRequestInfo(request_id)) {
    // This can happen if the request is cancelled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    DVLOG(1) << "Request ID not found";
    OnStreamGeneratedForCancelledRequest(audio_devices, video_devices);
    return;
  }
  current_request_info_->set_state(RequestInfo::State::GENERATED);

  for (const auto* devices : {&audio_devices, &video_devices}) {
    for (const auto& device : *devices) {
      WebRtcLogMessage(base::StringPrintf(
          "UMCI::OnStreamGenerated. request_id=%d, device id=\"%s\", "
          "device name=\"%s\"",
          request_id, device.id.c_str(), device.name.c_str()));
    }
  }

  DCHECK(!current_request_info_->web_request().IsNull());
  blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
      audio_devices.size());
  CreateAudioTracks(audio_devices,
                    current_request_info_->web_request().AudioConstraints(),
                    &audio_track_vector);

  blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
      video_devices.size());
  CreateVideoTracks(video_devices, &video_track_vector);

  blink::WebString blink_id = blink::WebString::FromUTF8(label);
  current_request_info_->web_stream()->Initialize(blink_id, audio_track_vector,
                                                  video_track_vector);

  // Wait for the tracks to be started successfully or to fail.
  current_request_info_->CallbackOnTracksStarted(
      base::Bind(&UserMediaProcessor::OnCreateNativeTracksCompleted,
                 weak_factory_.GetWeakPtr(), label));
}

void UserMediaProcessor::OnStreamGeneratedForCancelledRequest(
    const MediaStreamDevices& audio_devices,
    const MediaStreamDevices& video_devices) {
  // Only stop the device if the device is not used in another MediaStream.
  for (auto it = audio_devices.begin(); it != audio_devices.end(); ++it) {
    if (!FindLocalSource(*it))
      media_stream_dispatcher_->StopStreamDevice(*it);
  }

  for (auto it = video_devices.begin(); it != video_devices.end(); ++it) {
    if (!FindLocalSource(*it))
      media_stream_dispatcher_->StopStreamDevice(*it);
  }
}

// static
void UserMediaProcessor::OnAudioSourceStartedOnAudioThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<UserMediaProcessor> weak_ptr,
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&UserMediaProcessor::OnAudioSourceStarted,
                                weak_ptr, source, result, result_name));
}

void UserMediaProcessor::OnAudioSourceStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto it = pending_local_sources_.begin();
       it != pending_local_sources_.end(); ++it) {
    MediaStreamSource* const source_extra_data =
        static_cast<MediaStreamSource*>((*it).GetExtraData());
    if (source_extra_data != source)
      continue;
    if (result == MEDIA_DEVICE_OK)
      local_sources_.push_back((*it));
    pending_local_sources_.erase(it);

    NotifyCurrentRequestInfoOfAudioSourceStarted(source, result, result_name);
    return;
  }
  NOTREACHED();
}

void UserMediaProcessor::NotifyCurrentRequestInfoOfAudioSourceStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  // The only request possibly being processed is |current_request_info_|.
  if (current_request_info_)
    current_request_info_->OnAudioSourceStarted(source, result, result_name);
}

// Callback from MediaStreamDispatcher.
// The requested stream failed to be generated.
void UserMediaProcessor::OnStreamGenerationFailed(
    int request_id,
    MediaStreamRequestResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsCurrentRequestInfo(request_id)) {
    // This can happen if the request is cancelled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    return;
  }

  GetUserMediaRequestFailed(result, "");
  DeleteWebRequest(current_request_info_->web_request());
}

// Callback from MediaStreamDispatcher.
// The browser process has stopped a device used by a MediaStream.
void UserMediaProcessor::OnDeviceStopped(const std::string& label,
                                         const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UserMediaClientImpl::OnDeviceStopped("
           << "{device_id = " << device.id << "})";

  const blink::WebMediaStreamSource* source_ptr = FindLocalSource(device);
  if (!source_ptr) {
    // This happens if the same device is used in several guM requests or
    // if a user happen stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    return;
  }
  // By creating |source| it is guaranteed that the blink::WebMediaStreamSource
  // object is valid during the cleanup.
  blink::WebMediaStreamSource source(*source_ptr);
  StopLocalSource(source, false);
  RemoveLocalSource(source);
}

blink::WebMediaStreamSource UserMediaProcessor::InitializeVideoSourceObject(
    const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);

  blink::WebMediaStreamSource source = FindOrInitializeSourceObject(device);
  if (!source.GetExtraData()) {
    source.SetExtraData(CreateVideoSource(
        device, base::Bind(&UserMediaProcessor::OnLocalSourceStopped,
                           weak_factory_.GetWeakPtr())));
    local_sources_.push_back(source);
  }
  return source;
}

blink::WebMediaStreamSource UserMediaProcessor::InitializeAudioSourceObject(
    const MediaStreamDevice& device,
    const blink::WebMediaConstraints& constraints,
    bool* is_pending) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  *is_pending = true;

  // See if the source is already being initialized.
  auto* pending = FindPendingLocalSource(device);
  if (pending)
    return *pending;

  blink::WebMediaStreamSource source = FindOrInitializeSourceObject(device);
  if (source.GetExtraData()) {
    // The only return point for non-pending sources.
    *is_pending = false;
    return source;
  }

  // While sources are being initialized, keep them in a separate array.
  // Once they've finished initialized, they'll be moved over to local_sources_.
  // See OnAudioSourceStarted for more details.
  pending_local_sources_.push_back(source);

  MediaStreamSource::ConstraintsCallback source_ready = base::Bind(
      &UserMediaProcessor::OnAudioSourceStartedOnAudioThread,
      base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr());

  bool has_sw_echo_cancellation = false;
  MediaStreamAudioSource* const audio_source = CreateAudioSource(
      device, constraints, source_ready, &has_sw_echo_cancellation);
  audio_source->SetStopCallback(base::Bind(
      &UserMediaProcessor::OnLocalSourceStopped, weak_factory_.GetWeakPtr()));
  source.SetExtraData(audio_source);  // Takes ownership.
  // At this point it is known if software echo cancellation will be used, but
  // final audio parameters for the source are not set yet, so it is not yet
  // known if hardware echo cancellation will actually be used. That information
  // is known and surfaced in CreateAudioTracks(), after the track is connected
  // to the source.
  source.SetEchoCancellation(has_sw_echo_cancellation);
  return source;
}

MediaStreamAudioSource* UserMediaProcessor::CreateAudioSource(
    const MediaStreamDevice& device,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamSource::ConstraintsCallback& source_ready,
    bool* has_sw_echo_cancellation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);

  // If the audio device is a loopback device (for screen capture), or if the
  // constraints/effects parameters indicate no audio processing is needed,
  // create an efficient, direct-path MediaStreamAudioSource instance.
  AudioProcessingProperties audio_processing_properties =
      current_request_info_->audio_capture_settings()
          .audio_processing_properties();
  if (IsScreenCaptureMediaType(device.type) ||
      !MediaStreamAudioProcessor::WouldModifyAudio(
          audio_processing_properties)) {
    *has_sw_echo_cancellation = false;
    return new LocalMediaStreamAudioSource(render_frame_->GetRoutingID(),
                                           device, source_ready);
  }

  // The audio device is not associated with screen capture and also requires
  // processing.
  ProcessedLocalAudioSource* source = new ProcessedLocalAudioSource(
      render_frame_->GetRoutingID(), device, audio_processing_properties,
      source_ready, dependency_factory_);
  *has_sw_echo_cancellation =
      audio_processing_properties.enable_sw_echo_cancellation;
  return source;
}

MediaStreamVideoSource* UserMediaProcessor::CreateVideoSource(
    const MediaStreamDevice& device,
    const MediaStreamSource::SourceStoppedCallback& stop_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);
  DCHECK(current_request_info_->video_capture_settings().HasValue());

  return new MediaStreamVideoCapturerSource(
      stop_callback, device,
      current_request_info_->video_capture_settings().capture_params(),
      render_frame_);
}

void UserMediaProcessor::CreateVideoTracks(
    const MediaStreamDevices& devices,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  for (size_t i = 0; i < devices.size(); ++i) {
    blink::WebMediaStreamSource source =
        InitializeVideoSourceObject(devices[i]);
    (*webkit_tracks)[i] =
        current_request_info_->CreateAndStartVideoTrack(source);
  }
}

void UserMediaProcessor::CreateAudioTracks(
    const MediaStreamDevices& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  MediaStreamDevices overridden_audio_devices = devices;
  bool render_to_associated_sink =
      current_request_info_->audio_capture_settings().HasValue() &&
      current_request_info_->audio_capture_settings()
          .render_to_associated_sink();
  if (!render_to_associated_sink) {
    // If the GetUserMedia request did not explicitly set the constraint
    // kMediaStreamRenderToAssociatedSink, the output device parameters must
    // be removed.
    for (auto& device : overridden_audio_devices) {
      device.matched_output_device_id = "";
      device.matched_output = media::AudioParameters::UnavailableDeviceParams();
    }
  }

  for (size_t i = 0; i < overridden_audio_devices.size(); ++i) {
    bool is_pending = false;
    blink::WebMediaStreamSource source = InitializeAudioSourceObject(
        overridden_audio_devices[i], constraints, &is_pending);
    (*webkit_tracks)[i].Initialize(source);
    current_request_info_->StartAudioTrack((*webkit_tracks)[i], is_pending);
    // At this point the source has started, and its audio parameters have been
    // set. From the parameters, it is known if hardware echo cancellation is
    // being used. If this is the case, let |source| know.
    SurfaceHardwareEchoCancellationSetting(&source);
  }
}

void UserMediaProcessor::OnCreateNativeTracksCompleted(
    const std::string& label,
    RequestInfo* request_info,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == MEDIA_DEVICE_OK) {
    GetUserMediaRequestSucceeded(*request_info->web_stream(),
                                 request_info->web_request());
    media_stream_dispatcher_->OnStreamStarted(label);
  } else {
    GetUserMediaRequestFailed(result, result_name);

    blink::WebVector<blink::WebMediaStreamTrack> tracks;
    request_info->web_stream()->AudioTracks(tracks);
    for (auto& web_track : tracks) {
      MediaStreamTrack* track = MediaStreamTrack::GetTrack(web_track);
      if (track)
        track->Stop();
    }
    request_info->web_stream()->VideoTracks(tracks);
    for (auto& web_track : tracks) {
      MediaStreamTrack* track = MediaStreamTrack::GetTrack(web_track);
      if (track)
        track->Stop();
    }
  }

  DeleteWebRequest(request_info->web_request());
}

void UserMediaProcessor::OnDeviceOpened(int request_id,
                                        const std::string& label,
                                        const MediaStreamDevice& device) {
  DVLOG(1) << "UserMediaClientImpl::OnDeviceOpened(" << request_id << ", "
           << label << ")";
  NOTIMPLEMENTED();
}

void UserMediaProcessor::OnDeviceOpenFailed(int request_id) {
  DVLOG(1) << "UserMediaProcessor::VideoDeviceOpenFailed(" << request_id << ")";
  NOTIMPLEMENTED();
}

void UserMediaProcessor::GetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest web_request) {
  DCHECK(IsCurrentRequestInfo(web_request));
  WebRtcLogMessage(
      base::StringPrintf("UMCI::GetUserMediaRequestSucceeded. request_id=%d",
                         current_request_info_->request_id()));

  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl/UserMediaProcessor are destroyed if the JavaScript
  // code request the frame to be destroyed within the scope of the callback.
  // Therefore, post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&UserMediaProcessor::DelayedGetUserMediaRequestSucceeded,
                     weak_factory_.GetWeakPtr(), stream, web_request));
}

void UserMediaProcessor::DelayedGetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest web_request) {
  DVLOG(1) << "UserMediaProcessor::DelayedGetUserMediaRequestSucceeded";
  LogUserMediaRequestResult(MEDIA_DEVICE_OK);
  DeleteWebRequest(web_request);
  web_request.RequestSucceeded(stream);
}

void UserMediaProcessor::GetUserMediaRequestFailed(
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DCHECK(current_request_info_);
  WebRtcLogMessage(
      base::StringPrintf("UMCI::GetUserMediaRequestFailed. request_id=%d",
                         current_request_info_->request_id()));

  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl/UserMediaProcessor are destroyed if the JavaScript
  // code request the frame to be destroyed within the scope of the callback.
  // Therefore, post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&UserMediaProcessor::DelayedGetUserMediaRequestFailed,
                     weak_factory_.GetWeakPtr(),
                     current_request_info_->web_request(), result,
                     result_name));
}

void UserMediaProcessor::DelayedGetUserMediaRequestFailed(
    blink::WebUserMediaRequest web_request,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  LogUserMediaRequestResult(result);
  DeleteWebRequest(web_request);
  switch (result) {
    case MEDIA_DEVICE_OK:
    case NUM_MEDIA_REQUEST_RESULTS:
      NOTREACHED();
      return;
    case MEDIA_DEVICE_PERMISSION_DENIED:
      web_request.RequestDenied();
      return;
    case MEDIA_DEVICE_PERMISSION_DISMISSED:
      web_request.RequestFailedUASpecific("PermissionDismissedError");
      return;
    case MEDIA_DEVICE_INVALID_STATE:
      web_request.RequestFailedUASpecific("InvalidStateError");
      return;
    case MEDIA_DEVICE_NO_HARDWARE:
      web_request.RequestFailedUASpecific("DevicesNotFoundError");
      return;
    case MEDIA_DEVICE_INVALID_SECURITY_ORIGIN_DEPRECATED:
      NOTREACHED();
      return;
    case MEDIA_DEVICE_TAB_CAPTURE_FAILURE:
      web_request.RequestFailedUASpecific("TabCaptureError");
      return;
    case MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE:
      web_request.RequestFailedUASpecific("ScreenCaptureError");
      return;
    case MEDIA_DEVICE_CAPTURE_FAILURE:
      web_request.RequestFailedUASpecific("DeviceCaptureError");
      return;
    case MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED:
      web_request.RequestFailedConstraint(result_name);
      return;
    case MEDIA_DEVICE_TRACK_START_FAILURE:
      web_request.RequestFailedUASpecific("TrackStartError");
      return;
    case MEDIA_DEVICE_NOT_SUPPORTED:
      web_request.RequestFailedUASpecific("MediaDeviceNotSupported");
      return;
    case MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN:
      web_request.RequestFailedUASpecific("MediaDeviceFailedDueToShutdown");
      return;
    case MEDIA_DEVICE_KILL_SWITCH_ON:
      web_request.RequestFailedUASpecific("MediaDeviceKillSwitchOn");
      return;
  }
  NOTREACHED();
  web_request.RequestFailed();
}

const blink::WebMediaStreamSource* UserMediaProcessor::FindLocalSource(
    const LocalStreamSources& sources,
    const MediaStreamDevice& device) const {
  for (const auto& local_source : sources) {
    MediaStreamSource* const source =
        static_cast<MediaStreamSource*>(local_source.GetExtraData());
    const MediaStreamDevice& active_device = source->device();
    if (IsSameDevice(active_device, device))
      return &local_source;
  }
  return nullptr;
}

blink::WebMediaStreamSource UserMediaProcessor::FindOrInitializeSourceObject(
    const MediaStreamDevice& device) {
  const blink::WebMediaStreamSource* existing_source = FindLocalSource(device);
  if (existing_source) {
    DVLOG(1) << "Source already exists. Reusing source with id "
             << existing_source->Id().Utf8();
    return *existing_source;
  }

  blink::WebMediaStreamSource::Type type =
      IsAudioInputMediaType(device.type)
          ? blink::WebMediaStreamSource::kTypeAudio
          : blink::WebMediaStreamSource::kTypeVideo;

  blink::WebMediaStreamSource source;
  source.Initialize(blink::WebString::FromUTF8(device.id), type,
                    blink::WebString::FromUTF8(device.name),
                    false /* remote */);

  DVLOG(1) << "Initialize source object :"
           << "id = " << source.Id().Utf8()
           << ", name = " << source.GetName().Utf8();
  return source;
}

bool UserMediaProcessor::RemoveLocalSource(
    const blink::WebMediaStreamSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (LocalStreamSources::iterator device_it = local_sources_.begin();
       device_it != local_sources_.end(); ++device_it) {
    if (IsSameSource(*device_it, source)) {
      local_sources_.erase(device_it);
      return true;
    }
  }

  // Check if the source was pending.
  for (LocalStreamSources::iterator device_it = pending_local_sources_.begin();
       device_it != pending_local_sources_.end(); ++device_it) {
    if (IsSameSource(*device_it, source)) {
      MediaStreamSource* const source_extra_data =
          static_cast<MediaStreamSource*>(source.GetExtraData());
      NotifyCurrentRequestInfoOfAudioSourceStarted(
          source_extra_data, MEDIA_DEVICE_TRACK_START_FAILURE,
          "Failed to access audio capture device");
      pending_local_sources_.erase(device_it);
      return true;
    }
  }

  return false;
}

bool UserMediaProcessor::IsCurrentRequestInfo(int request_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_request_info_ &&
         current_request_info_->request_id() == request_id;
}

bool UserMediaProcessor::IsCurrentRequestInfo(
    const blink::WebUserMediaRequest& web_request) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_request_info_ &&
         current_request_info_->web_request() == web_request;
}

bool UserMediaProcessor::DeleteWebRequest(
    const blink::WebUserMediaRequest& web_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_request_info_ &&
      current_request_info_->web_request() == web_request) {
    current_request_info_.reset();
    base::ResetAndReturn(&request_completed_cb_).Run();
    return true;
  }
  return false;
}

void UserMediaProcessor::StopAllProcessing() {
  if (current_request_info_) {
    // If the request is not generated, it means that a request has been sent to
    // the MediaStreamDispatcher to generate a stream but MediaStreamDispatcher
    // has not yet responded and we need to cancel the request.
    if (current_request_info_->state() == RequestInfo::State::GENERATED) {
      DCHECK(current_request_info_->HasPendingSources());
      LogUserMediaRequestWithNoResult(
          MEDIA_STREAM_REQUEST_PENDING_MEDIA_TRACKS);
    } else {
      DCHECK(!current_request_info_->HasPendingSources());
      if (current_request_info_->state() ==
          RequestInfo::State::SENT_FOR_GENERATION) {
        media_stream_dispatcher_->CancelGenerateStream(
            current_request_info_->request_id(), weak_factory_.GetWeakPtr());
      }
      LogUserMediaRequestWithNoResult(MEDIA_STREAM_REQUEST_NOT_GENERATED);
    }
    current_request_info_.reset();
  }
  request_completed_cb_.Reset();

  // Loop through all current local sources and stop the sources.
  auto it = local_sources_.begin();
  while (it != local_sources_.end()) {
    StopLocalSource(*it, true);
    it = local_sources_.erase(it);
  }
}

void UserMediaProcessor::OnLocalSourceStopped(
    const blink::WebMediaStreamSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UserMediaProcessor::OnLocalSourceStopped";

  const bool some_source_removed = RemoveLocalSource(source);
  CHECK(some_source_removed);

  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.GetExtraData());
  media_stream_dispatcher_->StopStreamDevice(source_impl->device());
}

void UserMediaProcessor::StopLocalSource(
    const blink::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.GetExtraData());
  DVLOG(1) << "UserMediaProcessor::StopLocalSource("
           << "{device_id = " << source_impl->device().id << "})";

  if (notify_dispatcher) {
    media_stream_dispatcher_->StopStreamDevice(source_impl->device());
  }

  source_impl->ResetSourceStoppedCallback();
  source_impl->StopSource();
}

const ::mojom::MediaDevicesDispatcherHostPtr&
UserMediaProcessor::GetMediaDevicesDispatcher() {
  return media_devices_dispatcher_cb_.Run();
}

const AudioCaptureSettings& UserMediaProcessor::AudioCaptureSettingsForTesting()
    const {
  DCHECK(current_request_info_);
  return current_request_info_->audio_capture_settings();
}

const VideoCaptureSettings& UserMediaProcessor::VideoCaptureSettingsForTesting()
    const {
  DCHECK(current_request_info_);
  return current_request_info_->video_capture_settings();
}

}  // namespace content
