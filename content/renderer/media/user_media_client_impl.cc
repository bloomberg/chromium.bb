// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/user_media_client_impl.h"

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
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/local_media_stream_audio_source.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_constraints_util_audio.h"
#include "content/renderer/media/media_stream_constraints_util_video_content.h"
#include "content/renderer/media/media_stream_constraints_util_video_device.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/webrtc/processed_local_audio_source.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "media/capture/video_capture_types.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

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

blink::WebMediaDeviceInfo::MediaDeviceKind ToMediaDeviceKind(
    MediaDeviceType type) {
  switch (type) {
    case MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return blink::WebMediaDeviceInfo::kMediaDeviceKindAudioInput;
    case MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return blink::WebMediaDeviceInfo::kMediaDeviceKindVideoInput;
    case MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      return blink::WebMediaDeviceInfo::kMediaDeviceKindAudioOutput;
    default:
      NOTREACHED();
      return blink::WebMediaDeviceInfo::kMediaDeviceKindAudioInput;
  }
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

static int g_next_request_id = 0;

}  // namespace

// Class for storing information about a Blink request to create a
// MediaStream.
class UserMediaClientImpl::UserMediaRequestInfo
    : public base::SupportsWeakPtr<UserMediaRequestInfo> {
 public:
  using ResourcesReady =
      base::Callback<void(UserMediaRequestInfo* request_info,
                          MediaStreamRequestResult result,
                          const blink::WebString& result_name)>;
  enum class State {
    NOT_SENT_FOR_GENERATION,
    SENT_FOR_GENERATION,
    GENERATED,
  };

  UserMediaRequestInfo(int request_id,
                       const blink::WebUserMediaRequest& request,
                       bool is_processing_user_gesture,
                       const url::Origin& security_origin);

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

  int request_id() const { return request_id_; }

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

  const blink::WebUserMediaRequest& request() const { return request_; }

  StreamControls* stream_controls() { return &stream_controls_; }

  bool is_processing_user_gesture() const {
    return is_processing_user_gesture_;
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

  const int request_id_;
  State state_;
  AudioCaptureSettings audio_capture_settings_;
  bool is_audio_content_capture_;
  VideoCaptureSettings video_capture_settings_;
  bool is_video_content_capture_;
  blink::WebMediaStream web_stream_;
  const blink::WebUserMediaRequest request_;
  StreamControls stream_controls_;
  const bool is_processing_user_gesture_;
  const url::Origin security_origin_;
  ResourcesReady ready_callback_;
  MediaStreamRequestResult request_result_;
  blink::WebString request_result_name_;
  // Sources used in this request.
  std::vector<blink::WebMediaStreamSource> sources_;
  std::vector<MediaStreamSource*> sources_waiting_for_callback_;
};

UserMediaClientImpl::UserMediaClientImpl(
    RenderFrame* render_frame,
    PeerConnectionDependencyFactory* dependency_factory,
    std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher,
    const scoped_refptr<base::TaskRunner>& worker_task_runner)
    : RenderFrameObserver(render_frame),
      dependency_factory_(dependency_factory),
      media_stream_dispatcher_(std::move(media_stream_dispatcher)),
      worker_task_runner_(worker_task_runner),
      weak_factory_(this) {
  DCHECK(dependency_factory_);
  DCHECK(media_stream_dispatcher_.get());
}

UserMediaClientImpl::~UserMediaClientImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Force-close all outstanding user media requests and local sources here,
  // before the outstanding WeakPtrs are invalidated, to ensure a clean
  // shutdown.
  WillCommitProvisionalLoad();
}

void UserMediaClientImpl::RequestUserMedia(
    const blink::WebUserMediaRequest& user_media_request) {
  // Save histogram data so we can see how much GetUserMedia is used.
  // The histogram counts the number of calls to the JS API
  // webGetUserMedia.
  UpdateWebRTCMethodCount(WEBKIT_GET_USER_MEDIA);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!user_media_request.IsNull());
  DCHECK(user_media_request.Audio() || user_media_request.Video());
  // ownerDocument may be null if we are in a test.
  // In that case, it's OK to not check frame().
  DCHECK(user_media_request.OwnerDocument().IsNull() ||
         render_frame()->GetWebFrame() ==
             static_cast<blink::WebFrame*>(
                 user_media_request.OwnerDocument().GetFrame()));

  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->peer_connection_tracker()->TrackGetUserMedia(
        user_media_request);
  }

  int request_id = g_next_request_id++;
  WebRtcLogMessage(base::StringPrintf(
      "UMCI::RequestUserMedia. request_id=%d, audio constraints=%s, "
      "video constraints=%s",
      request_id,
      user_media_request.AudioConstraints().ToString().Utf8().c_str(),
      user_media_request.VideoConstraints().ToString().Utf8().c_str()));

  // The value returned by isProcessingUserGesture() is used by the browser to
  // make decisions about the permissions UI. Its value can be lost while
  // switching threads, so saving its value here.
  std::unique_ptr<UserMediaRequestInfo> request_info =
      base::MakeUnique<UserMediaRequestInfo>(
          request_id, user_media_request,
          blink::WebUserGestureIndicator::IsProcessingUserGesture(),
          static_cast<url::Origin>(user_media_request.GetSecurityOrigin()));
  pending_request_infos_.push_back(std::move(request_info));
  if (!current_request_info_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UserMediaClientImpl::MaybeProcessNextRequestInfo,
                       weak_factory_.GetWeakPtr()));
  }
}

void UserMediaClientImpl::MaybeProcessNextRequestInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_request_info_ || pending_request_infos_.empty())
    return;

  current_request_info_ = std::move(pending_request_infos_.front());
  pending_request_infos_.pop_front();

  // TODO(guidou): Set up audio and video in parallel.
  if (current_request_info_->request().Audio()) {
    SetupAudioInput();
    return;
  }
  SetupVideoInput();
}

void UserMediaClientImpl::SetupAudioInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);
  DCHECK(current_request_info_->request().Audio());

  auto& audio_controls = current_request_info_->stream_controls()->audio;
  InitializeTrackControls(current_request_info_->request().AudioConstraints(),
                          &audio_controls);
  if (IsDeviceSource(audio_controls.stream_source)) {
    GetMediaDevicesDispatcher()->GetAudioInputCapabilities(base::BindOnce(
        &UserMediaClientImpl::SelectAudioSettings, weak_factory_.GetWeakPtr(),
        current_request_info_->request()));
  } else {
    if (!IsValidAudioContentSource(audio_controls.stream_source)) {
      blink::WebString failed_constraint_name =
          blink::WebString::FromASCII(current_request_info_->request()
                                          .AudioConstraints()
                                          .Basic()
                                          .media_stream_source.GetName());
      MediaStreamRequestResult result = MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
      GetUserMediaRequestFailed(result, failed_constraint_name);
      return;
    }
    SelectAudioSettings(current_request_info_->request(),
                        AudioDeviceCaptureCapabilities());
  }
}

void UserMediaClientImpl::SelectAudioSettings(
    const blink::WebUserMediaRequest& user_media_request,
    std::vector<::mojom::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The frame might reload or |user_media_request| might be cancelled while
  // capabilities are queried. Do nothing if a different request is being
  // processed at this point.
  if (!IsCurrentRequestInfo(user_media_request))
    return;

  DCHECK(current_request_info_->stream_controls()->audio.requested);
  auto settings =
      SelectSettingsAudioCapture(std::move(audio_input_capabilities),
                                 user_media_request.AudioConstraints());
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

void UserMediaClientImpl::SetupVideoInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);

  if (!current_request_info_->request().Video()) {
    GenerateStreamForCurrentRequestInfo();
    return;
  }
  auto& video_controls = current_request_info_->stream_controls()->video;
  InitializeTrackControls(current_request_info_->request().VideoConstraints(),
                          &video_controls);
  if (IsDeviceSource(video_controls.stream_source)) {
    GetMediaDevicesDispatcher()->GetVideoInputCapabilities(base::BindOnce(
        &UserMediaClientImpl::SelectVideoDeviceSettings,
        weak_factory_.GetWeakPtr(), current_request_info_->request()));
  } else {
    if (!IsValidVideoContentSource(video_controls.stream_source)) {
      blink::WebString failed_constraint_name =
          blink::WebString::FromASCII(current_request_info_->request()
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
                   current_request_info_->request().VideoConstraints(),
                   video_controls.stream_source),
        base::Bind(&UserMediaClientImpl::FinalizeSelectVideoContentSettings,
                   weak_factory_.GetWeakPtr(),
                   current_request_info_->request()));
  }
}

void UserMediaClientImpl::SelectVideoDeviceSettings(
    const blink::WebUserMediaRequest& user_media_request,
    std::vector<::mojom::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The frame might reload or |user_media_request| might be cancelled while
  // capabilities are queried. Do nothing if a different request is being
  // processed at this point.
  if (!IsCurrentRequestInfo(user_media_request))
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
                 user_media_request.VideoConstraints()),
      base::Bind(&UserMediaClientImpl::FinalizeSelectVideoDeviceSettings,
                 weak_factory_.GetWeakPtr(), user_media_request));
}

void UserMediaClientImpl::FinalizeSelectVideoDeviceSettings(
    const blink::WebUserMediaRequest& user_media_request,
    const VideoCaptureSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsCurrentRequestInfo(user_media_request))
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

void UserMediaClientImpl::FinalizeSelectVideoContentSettings(
    const blink::WebUserMediaRequest& user_media_request,
    const VideoCaptureSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsCurrentRequestInfo(user_media_request))
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

void UserMediaClientImpl::GenerateStreamForCurrentRequestInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);
  WebRtcLogMessage(base::StringPrintf(
      "UMCI::GenerateStreamForCurrentRequestInfo. request_id=%d, "
      "audio device id=\"%s\", video device id=\"%s\"",
      current_request_info_->request_id(),
      current_request_info_->stream_controls()->audio.device_id.c_str(),
      current_request_info_->stream_controls()->video.device_id.c_str()));

  current_request_info_->set_state(
      UserMediaRequestInfo::State::SENT_FOR_GENERATION);
  media_stream_dispatcher_->GenerateStream(
      current_request_info_->request_id(), weak_factory_.GetWeakPtr(),
      *current_request_info_->stream_controls(),
      current_request_info_->security_origin(),
      current_request_info_->is_processing_user_gesture());
}

void UserMediaClientImpl::CancelUserMediaRequest(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsCurrentRequestInfo(user_media_request)) {
    WebRtcLogMessage(
        base::StringPrintf("UMCI::CancelUserMediaRequest. request_id=%d",
                           current_request_info_->request_id()));
  }
  if (DeleteRequestInfo(user_media_request)) {
    // We can't abort the stream generation process.
    // Instead, erase the request. Once the stream is generated we will stop the
    // stream if the request does not exist.
    LogUserMediaRequestWithNoResult(MEDIA_STREAM_REQUEST_EXPLICITLY_CANCELLED);
  }
}

void UserMediaClientImpl::RequestMediaDevices(
    const blink::WebMediaDevicesRequest& media_devices_request) {
  UpdateWebRTCMethodCount(WEBKIT_GET_MEDIA_DEVICES);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetMediaDevicesDispatcher()->EnumerateDevices(
      true /* audio input */, true /* video input */, true /* audio output */,
      base::BindOnce(&UserMediaClientImpl::FinalizeEnumerateDevices,
                     weak_factory_.GetWeakPtr(), media_devices_request));
}

void UserMediaClientImpl::SetMediaDeviceChangeObserver(
    const blink::WebMediaDeviceChangeObserver& observer) {
  media_device_change_observer_ = observer;

  // Do nothing if setting a valid observer while already subscribed or setting
  // no observer while unsubscribed.
  if (media_device_change_observer_.IsNull() ==
      device_change_subscription_ids_.empty())
    return;

  base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher =
      MediaDevicesEventDispatcher::GetForRenderFrame(render_frame());
  if (media_device_change_observer_.IsNull()) {
    event_dispatcher->UnsubscribeDeviceChangeNotifications(
        device_change_subscription_ids_);
    device_change_subscription_ids_.clear();
  } else {
    DCHECK(device_change_subscription_ids_.empty());
    device_change_subscription_ids_ =
        event_dispatcher->SubscribeDeviceChangeNotifications(base::Bind(
            &UserMediaClientImpl::DevicesChanged, weak_factory_.GetWeakPtr()));
  }
}

// Callback from MediaStreamDispatcher.
// The requested stream have been generated by the MediaStreamDispatcher.
void UserMediaClientImpl::OnStreamGenerated(
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
  current_request_info_->set_state(UserMediaRequestInfo::State::GENERATED);

  for (const auto* devices : {&audio_devices, &video_devices}) {
    for (const auto& device : *devices) {
      WebRtcLogMessage(base::StringPrintf(
          "UMCI::OnStreamGenerated. request_id=%d, device id=\"%s\", "
          "device name=\"%s\"",
          request_id, device.id.c_str(), device.name.c_str()));
    }
  }

  DCHECK(!current_request_info_->request().IsNull());
  blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
      audio_devices.size());
  CreateAudioTracks(audio_devices,
                    current_request_info_->request().AudioConstraints(),
                    &audio_track_vector);

  blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
      video_devices.size());
  CreateVideoTracks(video_devices, &video_track_vector);

  blink::WebString blink_id = blink::WebString::FromUTF8(label);
  current_request_info_->web_stream()->Initialize(blink_id, audio_track_vector,
                                                  video_track_vector);

  // Wait for the tracks to be started successfully or to fail.
  current_request_info_->CallbackOnTracksStarted(
      base::Bind(&UserMediaClientImpl::OnCreateNativeTracksCompleted,
                 weak_factory_.GetWeakPtr(), label));
}

void UserMediaClientImpl::OnStreamGeneratedForCancelledRequest(
    const MediaStreamDevices& audio_devices,
    const MediaStreamDevices& video_devices) {
  // Only stop the device if the device is not used in another MediaStream.
  for (MediaStreamDevices::const_iterator device_it = audio_devices.begin();
       device_it != audio_devices.end(); ++device_it) {
    if (!FindLocalSource(*device_it))
      media_stream_dispatcher_->StopStreamDevice(*device_it);
  }

  for (MediaStreamDevices::const_iterator device_it = video_devices.begin();
       device_it != video_devices.end(); ++device_it) {
    if (!FindLocalSource(*device_it))
      media_stream_dispatcher_->StopStreamDevice(*device_it);
  }
}

// static
void UserMediaClientImpl::OnAudioSourceStartedOnAudioThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<UserMediaClientImpl> weak_ptr,
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&UserMediaClientImpl::OnAudioSourceStarted,
                                weak_ptr, source, result, result_name));
}

void UserMediaClientImpl::OnAudioSourceStarted(
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

void UserMediaClientImpl::NotifyCurrentRequestInfoOfAudioSourceStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  // The only request possibly being processed is |current_request_info_|.
  if (current_request_info_)
    current_request_info_->OnAudioSourceStarted(source, result, result_name);
}

void UserMediaClientImpl::FinalizeEnumerateDevices(
    blink::WebMediaDevicesRequest request,
    const EnumerationResult& result) {
  DCHECK_EQ(static_cast<size_t>(NUM_MEDIA_DEVICE_TYPES), result.size());

  blink::WebVector<blink::WebMediaDeviceInfo> devices(
      result[MEDIA_DEVICE_TYPE_AUDIO_INPUT].size() +
      result[MEDIA_DEVICE_TYPE_VIDEO_INPUT].size() +
      result[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].size());
  size_t index = 0;
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    blink::WebMediaDeviceInfo::MediaDeviceKind device_kind =
        ToMediaDeviceKind(static_cast<MediaDeviceType>(i));
    for (const auto& device_info : result[i]) {
      devices[index++].Initialize(
          blink::WebString::FromUTF8(device_info.device_id), device_kind,
          blink::WebString::FromUTF8(device_info.label),
          blink::WebString::FromUTF8(device_info.group_id));
    }
  }

  EnumerateDevicesSucceded(&request, devices);
}

// Callback from MediaStreamDispatcher.
// The requested stream failed to be generated.
void UserMediaClientImpl::OnStreamGenerationFailed(
    int request_id,
    MediaStreamRequestResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsCurrentRequestInfo(request_id)) {
    // This can happen if the request is cancelled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    return;
  }

  GetUserMediaRequestFailed(result, "");
  DeleteRequestInfo(current_request_info_->request());
}

// Callback from MediaStreamDispatcher.
// The browser process has stopped a device used by a MediaStream.
void UserMediaClientImpl::OnDeviceStopped(const std::string& label,
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

blink::WebMediaStreamSource UserMediaClientImpl::InitializeVideoSourceObject(
    const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);

  blink::WebMediaStreamSource source = FindOrInitializeSourceObject(device);
  if (!source.GetExtraData()) {
    source.SetExtraData(CreateVideoSource(
        device, base::Bind(&UserMediaClientImpl::OnLocalSourceStopped,
                           weak_factory_.GetWeakPtr())));
    local_sources_.push_back(source);
  }
  return source;
}

blink::WebMediaStreamSource UserMediaClientImpl::InitializeAudioSourceObject(
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
      &UserMediaClientImpl::OnAudioSourceStartedOnAudioThread,
      base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr());

  bool has_sw_echo_cancellation = false;
  MediaStreamAudioSource* const audio_source = CreateAudioSource(
      device, constraints, source_ready, &has_sw_echo_cancellation);
  audio_source->SetStopCallback(base::Bind(
      &UserMediaClientImpl::OnLocalSourceStopped, weak_factory_.GetWeakPtr()));
  source.SetExtraData(audio_source);  // Takes ownership.
  // At this point it is known if software echo cancellation will be used, but
  // final audio parameters for the source are not set yet, so it is not yet
  // known if hardware echo cancellation will actually be used. That information
  // is known and surfaced in CreateAudioTracks(), after the track is connected
  // to the source.
  source.SetEchoCancellation(has_sw_echo_cancellation);
  return source;
}

MediaStreamAudioSource* UserMediaClientImpl::CreateAudioSource(
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
    return new LocalMediaStreamAudioSource(RenderFrameObserver::routing_id(),
                                           device, source_ready);
  }

  // The audio device is not associated with screen capture and also requires
  // processing.
  ProcessedLocalAudioSource* source = new ProcessedLocalAudioSource(
      RenderFrameObserver::routing_id(), device, audio_processing_properties,
      source_ready, dependency_factory_);
  *has_sw_echo_cancellation =
      audio_processing_properties.enable_sw_echo_cancellation;
  return source;
}

MediaStreamVideoSource* UserMediaClientImpl::CreateVideoSource(
    const MediaStreamDevice& device,
    const MediaStreamSource::SourceStoppedCallback& stop_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_request_info_);
  DCHECK(current_request_info_->video_capture_settings().HasValue());

  return new MediaStreamVideoCapturerSource(
      stop_callback, device,
      current_request_info_->video_capture_settings().capture_params(),
      render_frame());
}

void UserMediaClientImpl::CreateVideoTracks(
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

void UserMediaClientImpl::CreateAudioTracks(
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

void UserMediaClientImpl::OnCreateNativeTracksCompleted(
    const std::string& label,
    UserMediaRequestInfo* request_info,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == MEDIA_DEVICE_OK) {
    GetUserMediaRequestSucceeded(*request_info->web_stream(),
                                 request_info->request());
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

  DeleteRequestInfo(request_info->request());
}

void UserMediaClientImpl::OnDeviceOpened(int request_id,
                                         const std::string& label,
                                         const MediaStreamDevice& device) {
  DVLOG(1) << "UserMediaClientImpl::OnDeviceOpened("
           << request_id << ", " << label << ")";
  NOTIMPLEMENTED();
}

void UserMediaClientImpl::OnDeviceOpenFailed(int request_id) {
  DVLOG(1) << "UserMediaClientImpl::VideoDeviceOpenFailed("
           << request_id << ")";
  NOTIMPLEMENTED();
}

void UserMediaClientImpl::DevicesChanged(
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  if (!media_device_change_observer_.IsNull())
    media_device_change_observer_.DidChangeMediaDevices();
}

void UserMediaClientImpl::GetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest request) {
  DCHECK(IsCurrentRequestInfo(request));
  WebRtcLogMessage(
      base::StringPrintf("UMCI::GetUserMediaRequestSucceeded. request_id=%d",
                         current_request_info_->request_id()));

  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl is destroyed if the JavaScript code request the
  // frame to be destroyed within the scope of the callback. Therefore,
  // post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded,
                     weak_factory_.GetWeakPtr(), stream, request));
}

void UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest request) {
  DVLOG(1) << "UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded";
  LogUserMediaRequestResult(MEDIA_DEVICE_OK);
  DeleteRequestInfo(request);
  request.RequestSucceeded(stream);
}

void UserMediaClientImpl::GetUserMediaRequestFailed(
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DCHECK(current_request_info_);
  WebRtcLogMessage(
      base::StringPrintf("UMCI::GetUserMediaRequestFailed. request_id=%d",
                         current_request_info_->request_id()));

  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl is destroyed if the JavaScript code request the
  // frame to be destroyed within the scope of the callback. Therefore,
  // post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&UserMediaClientImpl::DelayedGetUserMediaRequestFailed,
                     weak_factory_.GetWeakPtr(),
                     current_request_info_->request(), result, result_name));
}

void UserMediaClientImpl::DelayedGetUserMediaRequestFailed(
    blink::WebUserMediaRequest request,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  LogUserMediaRequestResult(result);
  DeleteRequestInfo(request);
  switch (result) {
    case MEDIA_DEVICE_OK:
    case NUM_MEDIA_REQUEST_RESULTS:
      NOTREACHED();
      return;
    case MEDIA_DEVICE_PERMISSION_DENIED:
      request.RequestDenied();
      return;
    case MEDIA_DEVICE_PERMISSION_DISMISSED:
      request.RequestFailedUASpecific("PermissionDismissedError");
      return;
    case MEDIA_DEVICE_INVALID_STATE:
      request.RequestFailedUASpecific("InvalidStateError");
      return;
    case MEDIA_DEVICE_NO_HARDWARE:
      request.RequestFailedUASpecific("DevicesNotFoundError");
      return;
    case MEDIA_DEVICE_INVALID_SECURITY_ORIGIN_DEPRECATED:
      NOTREACHED();
      return;
    case MEDIA_DEVICE_TAB_CAPTURE_FAILURE:
      request.RequestFailedUASpecific("TabCaptureError");
      return;
    case MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE:
      request.RequestFailedUASpecific("ScreenCaptureError");
      return;
    case MEDIA_DEVICE_CAPTURE_FAILURE:
      request.RequestFailedUASpecific("DeviceCaptureError");
      return;
    case MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED:
      request.RequestFailedConstraint(result_name);
      return;
    case MEDIA_DEVICE_TRACK_START_FAILURE:
      request.RequestFailedUASpecific("TrackStartError");
      return;
    case MEDIA_DEVICE_NOT_SUPPORTED:
      request.RequestFailedUASpecific("MediaDeviceNotSupported");
      return;
    case MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN:
      request.RequestFailedUASpecific("MediaDeviceFailedDueToShutdown");
      return;
    case MEDIA_DEVICE_KILL_SWITCH_ON:
      request.RequestFailedUASpecific("MediaDeviceKillSwitchOn");
      return;
  }
  NOTREACHED();
  request.RequestFailed();
}

void UserMediaClientImpl::EnumerateDevicesSucceded(
    blink::WebMediaDevicesRequest* request,
    blink::WebVector<blink::WebMediaDeviceInfo>& devices) {
  request->RequestSucceeded(devices);
}

const blink::WebMediaStreamSource* UserMediaClientImpl::FindLocalSource(
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

blink::WebMediaStreamSource UserMediaClientImpl::FindOrInitializeSourceObject(
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

bool UserMediaClientImpl::RemoveLocalSource(
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

bool UserMediaClientImpl::IsCurrentRequestInfo(int request_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_request_info_ &&
         current_request_info_->request_id() == request_id;
}

bool UserMediaClientImpl::IsCurrentRequestInfo(
    const blink::WebUserMediaRequest& request) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_request_info_ && current_request_info_->request() == request;
}

bool UserMediaClientImpl::DeleteRequestInfo(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_request_info_ &&
      current_request_info_->request() == user_media_request) {
    current_request_info_.reset();
    if (!pending_request_infos_.empty()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&UserMediaClientImpl::MaybeProcessNextRequestInfo,
                         weak_factory_.GetWeakPtr()));
    }
    return true;
  }

  for (auto it = pending_request_infos_.begin();
       it != pending_request_infos_.end(); ++it) {
    if ((*it)->request() == user_media_request) {
      pending_request_infos_.erase(it);
      return true;
    }
  }

  return false;
}

void UserMediaClientImpl::DeleteAllUserMediaRequests() {
  if (current_request_info_) {
    // If the request is not generated, it means that a request
    // has been sent to the MediaStreamDispatcher to generate a stream
    // but MediaStreamDispatcher has not yet responded and we need to cancel
    // the request.
    if (current_request_info_->state() ==
        UserMediaRequestInfo::State::GENERATED) {
      DCHECK(current_request_info_->HasPendingSources());
      LogUserMediaRequestWithNoResult(
          MEDIA_STREAM_REQUEST_PENDING_MEDIA_TRACKS);
    } else {
      DCHECK(!current_request_info_->HasPendingSources());
      if (current_request_info_->state() ==
          UserMediaRequestInfo::State::SENT_FOR_GENERATION) {
        media_stream_dispatcher_->CancelGenerateStream(
            current_request_info_->request_id(), weak_factory_.GetWeakPtr());
      }
      LogUserMediaRequestWithNoResult(MEDIA_STREAM_REQUEST_NOT_GENERATED);
    }
    current_request_info_.reset();
    pending_request_infos_.clear();
  }
}

void UserMediaClientImpl::WillCommitProvisionalLoad() {
  // Cancel all outstanding UserMediaRequests.
  DeleteAllUserMediaRequests();

  // Loop through all current local sources and stop the sources.
  LocalStreamSources::iterator sources_it = local_sources_.begin();
  while (sources_it != local_sources_.end()) {
    StopLocalSource(*sources_it, true);
    sources_it = local_sources_.erase(sources_it);
  }
}

void UserMediaClientImpl::SetMediaDevicesDispatcherForTesting(
    ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher) {
  media_devices_dispatcher_ = std::move(media_devices_dispatcher);
}

void UserMediaClientImpl::OnLocalSourceStopped(
    const blink::WebMediaStreamSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UserMediaClientImpl::OnLocalSourceStopped";

  const bool some_source_removed = RemoveLocalSource(source);
  CHECK(some_source_removed);

  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.GetExtraData());
  media_stream_dispatcher_->StopStreamDevice(source_impl->device());
}

void UserMediaClientImpl::StopLocalSource(
    const blink::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.GetExtraData());
  DVLOG(1) << "UserMediaClientImpl::StopLocalSource("
           << "{device_id = " << source_impl->device().id << "})";

  if (notify_dispatcher) {
    media_stream_dispatcher_->StopStreamDevice(source_impl->device());
  }

  source_impl->ResetSourceStoppedCallback();
  source_impl->StopSource();
}

const ::mojom::MediaDevicesDispatcherHostPtr&
UserMediaClientImpl::GetMediaDevicesDispatcher() {
  if (!media_devices_dispatcher_) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&media_devices_dispatcher_));
  }

  return media_devices_dispatcher_;
}

const AudioCaptureSettings&
UserMediaClientImpl::AudioCaptureSettingsForTesting() const {
  DCHECK(current_request_info_);
  return current_request_info_->audio_capture_settings();
}

const VideoCaptureSettings&
UserMediaClientImpl::VideoCaptureSettingsForTesting() const {
  DCHECK(current_request_info_);
  return current_request_info_->video_capture_settings();
}

void UserMediaClientImpl::OnDestruct() {
  delete this;
}

UserMediaClientImpl::UserMediaRequestInfo::UserMediaRequestInfo(
    int request_id,
    const blink::WebUserMediaRequest& request,
    bool is_processing_user_gesture,
    const url::Origin& security_origin)
    : request_id_(request_id),
      state_(State::NOT_SENT_FOR_GENERATION),
      is_audio_content_capture_(false),
      is_video_content_capture_(false),
      request_(request),
      is_processing_user_gesture_(is_processing_user_gesture),
      security_origin_(security_origin),
      request_result_(MEDIA_DEVICE_OK),
      request_result_name_("") {}

void UserMediaClientImpl::UserMediaRequestInfo::StartAudioTrack(
    const blink::WebMediaStreamTrack& track,
    bool is_pending) {
  DCHECK(track.Source().GetType() == blink::WebMediaStreamSource::kTypeAudio);
  DCHECK(request_.Audio());
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
UserMediaClientImpl::UserMediaRequestInfo::CreateAndStartVideoTrack(
    const blink::WebMediaStreamSource& source) {
  DCHECK(source.GetType() == blink::WebMediaStreamSource::kTypeVideo);
  DCHECK(request_.Video());
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
      base::Bind(&UserMediaClientImpl::UserMediaRequestInfo::OnTrackStarted,
                 AsWeakPtr()),
      true);
}

void UserMediaClientImpl::UserMediaRequestInfo::CallbackOnTracksStarted(
    const ResourcesReady& callback) {
  DCHECK(ready_callback_.is_null());
  ready_callback_ = callback;
  CheckAllTracksStarted();
}

void UserMediaClientImpl::UserMediaRequestInfo::OnTrackStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DVLOG(1) << "OnTrackStarted result " << result;
  std::vector<MediaStreamSource*>::iterator it =
      std::find(sources_waiting_for_callback_.begin(),
                sources_waiting_for_callback_.end(),
                source);
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

void UserMediaClientImpl::UserMediaRequestInfo::CheckAllTracksStarted() {
  if (!ready_callback_.is_null() && sources_waiting_for_callback_.empty()) {
    ready_callback_.Run(this, request_result_, request_result_name_);
    // NOTE: |this| might now be deleted.
  }
}

bool UserMediaClientImpl::UserMediaRequestInfo::HasPendingSources() const {
  return !sources_waiting_for_callback_.empty();
}

void UserMediaClientImpl::UserMediaRequestInfo::OnAudioSourceStarted(
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

}  // namespace content
