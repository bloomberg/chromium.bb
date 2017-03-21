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
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_constraints_util.h"
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
  if (!constraint.exact().isEmpty())
    *destination = constraint.exact()[0].utf8();
}

bool FindDeviceId(const blink::WebVector<blink::WebString> candidates,
                  const MediaDeviceInfoArray& device_infos,
                  std::string* device_id) {
  for (const auto& candidate : candidates) {
    auto it = std::find_if(device_infos.begin(), device_infos.end(),
                           [&candidate](const MediaDeviceInfo& info) {
                             return info.device_id == candidate.utf8();
                           });

    if (it != device_infos.end()) {
      *device_id = it->device_id;
      return true;
    }
  }

  return false;
}

// If a device ID requested as exact basic constraint in |constraints| is found
// in |device_infos|, it is copied to |*device_id|, and the function returns
// false. If such a device is not found in |device_infos|, the function returns
// false and |*device_id| is left unmodified.
// If more than one device ID is requested as an exact basic constraint in
// |constraint|, the function returns false and |*device_id| is left unmodified.
// If no device ID is requested as an exact basic constraint, and at least one
// device ID requested as an ideal basic constraint or as an exact or ideal
// advanced constraint in |constraints| is found in |device_infos|, the first
// such device ID is copied to |*device_id| and the function returns true.
// If no such device ID is found, |*device_id| is left unmodified and the
// function returns true.
// TODO(guidou): Replace with a spec-compliant selection algorithm. See
// http://crbug.com/657733.
bool PickDeviceId(const blink::WebMediaConstraints& constraints,
                  const MediaDeviceInfoArray& device_infos,
                  std::string* device_id) {
  DCHECK(!constraints.isNull());
  DCHECK(device_id->empty());

  if (constraints.basic().deviceId.exact().size() > 1) {
    LOG(ERROR) << "Only one required device ID is supported";
    return false;
  }

  if (constraints.basic().deviceId.exact().size() == 1 &&
      !FindDeviceId(constraints.basic().deviceId.exact(), device_infos,
                    device_id)) {
    LOG(ERROR) << "Invalid mandatory device ID = "
               << constraints.basic().deviceId.exact()[0].utf8();
    return false;
  }

  // There is no required device ID. Look at the alternates.
  if (FindDeviceId(constraints.basic().deviceId.ideal(), device_infos,
                   device_id)) {
    return true;
  }

  for (const auto& advanced : constraints.advanced()) {
    if (FindDeviceId(advanced.deviceId.exact(), device_infos, device_id)) {
      return true;
    }
    if (FindDeviceId(advanced.deviceId.ideal(), device_infos, device_id)) {
      return true;
    }
  }

  // No valid alternate device ID found. Select default device.
  return true;
}

bool IsDeviceSource(const std::string& source) {
  return source.empty();
}

// TODO(guidou): Remove once audio constraints are processed with spec-compliant
// algorithm. See http://crbug.com/657733.
void CopyConstraintsToTrackControls(
    const blink::WebMediaConstraints& constraints,
    TrackControls* track_controls,
    bool* request_devices) {
  DCHECK(!constraints.isNull());
  track_controls->requested = true;
  CopyFirstString(constraints.basic().mediaStreamSource,
                  &track_controls->stream_source);
  if (IsDeviceSource(track_controls->stream_source)) {
    bool request_devices_advanced = false;
    for (const auto& advanced : constraints.advanced()) {
      if (!advanced.deviceId.isEmpty()) {
        request_devices_advanced = true;
        break;
      }
    }
    *request_devices =
        request_devices_advanced || !constraints.basic().deviceId.isEmpty();
  } else {
    CopyFirstString(constraints.basic().deviceId, &track_controls->device_id);
    *request_devices = false;
  }
}

void InitializeTrackControls(const blink::WebMediaConstraints& constraints,
                             TrackControls* track_controls) {
  DCHECK(!constraints.isNull());
  track_controls->requested = true;
  CopyFirstString(constraints.basic().mediaStreamSource,
                  &track_controls->stream_source);
}

void CopyHotwordAndLocalEchoToStreamControls(
    const blink::WebMediaConstraints& audio_constraints,
    StreamControls* controls) {
  if (audio_constraints.isNull())
    return;

  if (audio_constraints.basic().hotwordEnabled.hasExact()) {
    controls->hotword_enabled =
        audio_constraints.basic().hotwordEnabled.exact();
  } else {
    for (const auto& audio_advanced : audio_constraints.advanced()) {
      if (audio_advanced.hotwordEnabled.hasExact()) {
        controls->hotword_enabled = audio_advanced.hotwordEnabled.exact();
        break;
      }
    }
  }

  if (audio_constraints.basic().disableLocalEcho.hasExact()) {
    controls->disable_local_echo =
        audio_constraints.basic().disableLocalEcho.exact();
  } else {
    controls->disable_local_echo =
        controls->audio.stream_source != kMediaStreamSourceDesktop;
  }
}

bool IsSameDevice(const StreamDeviceInfo& device,
                  const StreamDeviceInfo& other_device) {
  return device.device.id == other_device.device.id &&
         device.device.type == other_device.device.type &&
         device.session_id == other_device.session_id;
}

bool IsSameSource(const blink::WebMediaStreamSource& source,
                  const blink::WebMediaStreamSource& other_source) {
  MediaStreamSource* const source_extra_data =
      static_cast<MediaStreamSource*>(source.getExtraData());
  const StreamDeviceInfo& device = source_extra_data->device_info();

  MediaStreamSource* const other_source_extra_data =
      static_cast<MediaStreamSource*>(other_source.getExtraData());
  const StreamDeviceInfo& other_device = other_source_extra_data->device_info();

  return IsSameDevice(device, other_device);
}

blink::WebMediaDeviceInfo::MediaDeviceKind ToMediaDeviceKind(
    MediaDeviceType type) {
  switch (type) {
    case MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput;
    case MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return blink::WebMediaDeviceInfo::MediaDeviceKindVideoInput;
    case MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      return blink::WebMediaDeviceInfo::MediaDeviceKindAudioOutput;
    default:
      NOTREACHED();
      return blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput;
  }
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
      const blink::WebMediaStreamSource& source,
      const blink::WebMediaConstraints& constraints);

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

  bool enable_automatic_output_device_selection() const {
    return enable_automatic_output_device_selection_;
  }
  void set_enable_automatic_output_device_selection(bool value) {
    enable_automatic_output_device_selection_ = value;
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
  bool enable_automatic_output_device_selection_;
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
  // Force-close all outstanding user media requests and local sources here,
  // before the outstanding WeakPtrs are invalidated, to ensure a clean
  // shutdown.
  WillCommitProvisionalLoad();
}

void UserMediaClientImpl::requestUserMedia(
    const blink::WebUserMediaRequest& user_media_request) {
  // Save histogram data so we can see how much GetUserMedia is used.
  // The histogram counts the number of calls to the JS API
  // webGetUserMedia.
  UpdateWebRTCMethodCount(WEBKIT_GET_USER_MEDIA);
  DCHECK(CalledOnValidThread());
  DCHECK(!user_media_request.isNull());
  DCHECK(user_media_request.audio() || user_media_request.video());
  // ownerDocument may be null if we are in a test.
  // In that case, it's OK to not check frame().
  DCHECK(user_media_request.ownerDocument().isNull() ||
         render_frame()->GetWebFrame() ==
             static_cast<blink::WebFrame*>(
                 user_media_request.ownerDocument().frame()));

  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->peer_connection_tracker()->TrackGetUserMedia(
        user_media_request);
  }

  int request_id = g_next_request_id++;

  // The value returned by isProcessingUserGesture() is used by the browser to
  // make decisions about the permissions UI. Its value can be lost while
  // switching threads, so saving its value here.
  std::unique_ptr<UserMediaRequestInfo> request_info =
      base::MakeUnique<UserMediaRequestInfo>(
          request_id, user_media_request,
          blink::WebUserGestureIndicator::isProcessingUserGesture(),
          static_cast<url::Origin>(user_media_request.getSecurityOrigin()));
  pending_request_infos_.push_back(std::move(request_info));
  if (!current_request_info_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&UserMediaClientImpl::MaybeProcessNextRequestInfo,
                              weak_factory_.GetWeakPtr()));
  }
}

void UserMediaClientImpl::MaybeProcessNextRequestInfo() {
  DCHECK(CalledOnValidThread());
  if (current_request_info_ || pending_request_infos_.empty())
    return;

  current_request_info_ = std::move(pending_request_infos_.front());
  pending_request_infos_.pop_front();

  // TODO(guidou): Request audio and video capabilities in parallel.
  if (current_request_info_->request().audio()) {
    bool request_audio_input_devices = false;
    // TODO(guidou): Implement spec-compliant device selection for audio. See
    // http://crbug.com/623104.
    CopyConstraintsToTrackControls(
        current_request_info_->request().audioConstraints(),
        &current_request_info_->stream_controls()->audio,
        &request_audio_input_devices);
    CopyHotwordAndLocalEchoToStreamControls(
        current_request_info_->request().audioConstraints(),
        current_request_info_->stream_controls());
    // Check if this input device should be used to select a matching output
    // device for audio rendering.
    bool enable_automatic_output_device_selection = false;
    GetConstraintValueAsBoolean(
        current_request_info_->request().audioConstraints(),
        &blink::WebMediaTrackConstraintSet::renderToAssociatedSink,
        &enable_automatic_output_device_selection);
    current_request_info_->set_enable_automatic_output_device_selection(
        enable_automatic_output_device_selection);

    if (request_audio_input_devices) {
      GetMediaDevicesDispatcher()->EnumerateDevices(
          true /* audio_input */, false /* video_input */,
          false /* audio_output */, current_request_info_->security_origin(),
          base::Bind(&UserMediaClientImpl::SelectAudioInputDevice,
                     weak_factory_.GetWeakPtr(),
                     current_request_info_->request()));
      return;
    }
  }

  SetupVideoInput(current_request_info_->request());
}

void UserMediaClientImpl::SelectAudioInputDevice(
    const blink::WebUserMediaRequest& user_media_request,
    const EnumerationResult& device_enumeration) {
  DCHECK(CalledOnValidThread());
  if (!IsCurrentRequestInfo(user_media_request))
    return;

  auto& audio_controls = current_request_info_->stream_controls()->audio;
  DCHECK(audio_controls.requested);
  DCHECK(IsDeviceSource(audio_controls.stream_source));

  if (!PickDeviceId(user_media_request.audioConstraints(),
                    device_enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT],
                    &audio_controls.device_id)) {
    GetUserMediaRequestFailed(user_media_request, MEDIA_DEVICE_NO_HARDWARE, "");
    return;
  }

  SetupVideoInput(user_media_request);
}

void UserMediaClientImpl::SetupVideoInput(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK(CalledOnValidThread());
  if (!IsCurrentRequestInfo(user_media_request))
    return;

  if (!user_media_request.video()) {
    GenerateStreamForCurrentRequestInfo();
    return;
  }
  auto& video_controls = current_request_info_->stream_controls()->video;
  InitializeTrackControls(user_media_request.videoConstraints(),
                          &video_controls);
  if (IsDeviceSource(video_controls.stream_source)) {
    GetMediaDevicesDispatcher()->GetVideoInputCapabilities(
        current_request_info_->security_origin(),
        base::Bind(&UserMediaClientImpl::SelectVideoDeviceSourceSettings,
                   weak_factory_.GetWeakPtr(), user_media_request));
  } else {
    base::PostTaskAndReplyWithResult(
        worker_task_runner_.get(), FROM_HERE,
        base::Bind(&SelectVideoContentCaptureSourceSettings,
                   user_media_request.videoConstraints()),
        base::Bind(
            &UserMediaClientImpl::FinalizeSelectVideoContentSourceSettings,
            weak_factory_.GetWeakPtr(), user_media_request));
  }
}

void UserMediaClientImpl::SelectVideoDeviceSourceSettings(
    const blink::WebUserMediaRequest& user_media_request,
    std::vector<::mojom::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities) {
  DCHECK(CalledOnValidThread());
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
  capabilities.noise_reduction_capabilities = {rtc::Optional<bool>(),
                                               rtc::Optional<bool>(true),
                                               rtc::Optional<bool>(false)};
  base::PostTaskAndReplyWithResult(
      worker_task_runner_.get(), FROM_HERE,
      base::Bind(&SelectVideoDeviceCaptureSourceSettings,
                 std::move(capabilities),
                 user_media_request.videoConstraints()),
      base::Bind(&UserMediaClientImpl::FinalizeSelectVideoDeviceSourceSettings,
                 weak_factory_.GetWeakPtr(), user_media_request));
}

void UserMediaClientImpl::FinalizeSelectVideoDeviceSourceSettings(
    const blink::WebUserMediaRequest& user_media_request,
    const VideoDeviceCaptureSourceSelectionResult& selection_result) {
  DCHECK(CalledOnValidThread());
  if (!IsCurrentRequestInfo(user_media_request))
    return;

  if (!selection_result.HasValue()) {
    blink::WebString failed_constraint_name =
        blink::WebString::fromASCII(selection_result.failed_constraint_name());
    MediaStreamRequestResult result =
        failed_constraint_name.isEmpty()
            ? MEDIA_DEVICE_NO_HARDWARE
            : MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
    GetUserMediaRequestFailed(user_media_request, result,
                              failed_constraint_name);
    return;
  }
  current_request_info_->stream_controls()->video.device_id =
      selection_result.device_id();
  GenerateStreamForCurrentRequestInfo();
}

void UserMediaClientImpl::FinalizeSelectVideoContentSourceSettings(
    const blink::WebUserMediaRequest& user_media_request,
    const VideoContentCaptureSourceSelectionResult& selection_result) {
  DCHECK(CalledOnValidThread());
  if (!IsCurrentRequestInfo(user_media_request))
    return;

  if (!selection_result.HasValue()) {
    blink::WebString failed_constraint_name =
        blink::WebString::fromASCII(selection_result.failed_constraint_name());
    DCHECK(!failed_constraint_name.isEmpty());
    blink::WebString device_id_constraint_name = blink::WebString::fromASCII(
        user_media_request.videoConstraints().basic().deviceId.name());
    GetUserMediaRequestFailed(user_media_request,
                              MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED,
                              failed_constraint_name);
    return;
  }
  current_request_info_->stream_controls()->video.device_id =
      selection_result.device_id();
  GenerateStreamForCurrentRequestInfo();
}

void UserMediaClientImpl::GenerateStreamForCurrentRequestInfo() {
  DCHECK(CalledOnValidThread());
  DCHECK(current_request_info_);

  WebRtcLogMessage(base::StringPrintf(
      "MSI::requestUserMedia. request_id=%d"
      ", audio source id=%s"
      ", video source id=%s",
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

void UserMediaClientImpl::cancelUserMediaRequest(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK(CalledOnValidThread());
  if (DeleteRequestInfo(user_media_request)) {
    // We can't abort the stream generation process.
    // Instead, erase the request. Once the stream is generated we will stop the
    // stream if the request does not exist.
    LogUserMediaRequestWithNoResult(MEDIA_STREAM_REQUEST_EXPLICITLY_CANCELLED);
  }
}

void UserMediaClientImpl::requestMediaDevices(
    const blink::WebMediaDevicesRequest& media_devices_request) {
  UpdateWebRTCMethodCount(WEBKIT_GET_MEDIA_DEVICES);
  DCHECK(CalledOnValidThread());

  // |media_devices_request| can't be mocked, so in tests it will be empty (the
  // underlying pointer is null). In order to use this function in a test we
  // need to check if it isNull.
  url::Origin security_origin;
  if (!media_devices_request.isNull())
    security_origin = media_devices_request.getSecurityOrigin();

  GetMediaDevicesDispatcher()->EnumerateDevices(
      true /* audio input */, true /* video input */, true /* audio output */,
      security_origin,
      base::Bind(&UserMediaClientImpl::FinalizeEnumerateDevices,
                 weak_factory_.GetWeakPtr(), media_devices_request));
}

void UserMediaClientImpl::setMediaDeviceChangeObserver(
    const blink::WebMediaDeviceChangeObserver& observer) {
  media_device_change_observer_ = observer;

  // Do nothing if setting a valid observer while already subscribed or setting
  // no observer while unsubscribed.
  if (media_device_change_observer_.isNull() ==
      device_change_subscription_ids_.empty())
    return;

  base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher =
      MediaDevicesEventDispatcher::GetForRenderFrame(render_frame());
  if (media_device_change_observer_.isNull()) {
    event_dispatcher->UnsubscribeDeviceChangeNotifications(
        device_change_subscription_ids_);
    device_change_subscription_ids_.clear();
  } else {
    DCHECK(device_change_subscription_ids_.empty());
    url::Origin security_origin =
        media_device_change_observer_.getSecurityOrigin();
    device_change_subscription_ids_ =
        event_dispatcher->SubscribeDeviceChangeNotifications(
            security_origin, base::Bind(&UserMediaClientImpl::DevicesChanged,
                                        weak_factory_.GetWeakPtr()));
  }
}

// Callback from MediaStreamDispatcher.
// The requested stream have been generated by the MediaStreamDispatcher.
void UserMediaClientImpl::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  DCHECK(CalledOnValidThread());
  if (!IsCurrentRequestInfo(request_id)) {
    // This can happen if the request is cancelled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    DVLOG(1) << "Request ID not found";
    OnStreamGeneratedForCancelledRequest(audio_array, video_array);
    return;
  }
  current_request_info_->set_state(UserMediaRequestInfo::State::GENERATED);

  for (const auto* array : {&audio_array, &video_array}) {
    for (const auto& info : *array) {
      WebRtcLogMessage(base::StringPrintf("Request %d for device \"%s\"",
                                          request_id,
                                          info.device.name.c_str()));
    }
  }

  DCHECK(!current_request_info_->request().isNull());
  blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
      audio_array.size());
  CreateAudioTracks(audio_array,
                    current_request_info_->request().audioConstraints(),
                    &audio_track_vector);

  blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
      video_array.size());
  CreateVideoTracks(video_array,
                    current_request_info_->request().videoConstraints(),
                    &video_track_vector);

  blink::WebString blink_id = blink::WebString::fromUTF8(label);
  current_request_info_->web_stream()->initialize(blink_id, audio_track_vector,
                                                  video_track_vector);
  current_request_info_->web_stream()->setExtraData(new MediaStream());

  // Wait for the tracks to be started successfully or to fail.
  current_request_info_->CallbackOnTracksStarted(
      base::Bind(&UserMediaClientImpl::OnCreateNativeTracksCompleted,
                 weak_factory_.GetWeakPtr(), label));
}

void UserMediaClientImpl::OnStreamGeneratedForCancelledRequest(
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  // Only stop the device if the device is not used in another MediaStream.
  for (StreamDeviceInfoArray::const_iterator device_it = audio_array.begin();
       device_it != audio_array.end(); ++device_it) {
    if (!FindLocalSource(*device_it))
      media_stream_dispatcher_->StopStreamDevice(*device_it);
  }

  for (StreamDeviceInfoArray::const_iterator device_it = video_array.begin();
       device_it != video_array.end(); ++device_it) {
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
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&UserMediaClientImpl::OnAudioSourceStarted,
                                   weak_ptr, source, result, result_name));
}

void UserMediaClientImpl::OnAudioSourceStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DCHECK(CalledOnValidThread());

  for (auto it = pending_local_sources_.begin();
       it != pending_local_sources_.end(); ++it) {
    MediaStreamSource* const source_extra_data =
        static_cast<MediaStreamSource*>((*it).getExtraData());
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
      devices[index++].initialize(
          blink::WebString::fromUTF8(device_info.device_id), device_kind,
          blink::WebString::fromUTF8(device_info.label),
          blink::WebString::fromUTF8(device_info.group_id));
    }
  }

  EnumerateDevicesSucceded(&request, devices);
}

// Callback from MediaStreamDispatcher.
// The requested stream failed to be generated.
void UserMediaClientImpl::OnStreamGenerationFailed(
    int request_id,
    MediaStreamRequestResult result) {
  DCHECK(CalledOnValidThread());
  if (!IsCurrentRequestInfo(request_id)) {
    // This can happen if the request is cancelled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    return;
  }

  GetUserMediaRequestFailed(current_request_info_->request(), result, "");
  DeleteRequestInfo(current_request_info_->request());
}

// Callback from MediaStreamDispatcher.
// The browser process has stopped a device used by a MediaStream.
void UserMediaClientImpl::OnDeviceStopped(
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnDeviceStopped("
           << "{device_id = " << device_info.device.id << "})";

  const blink::WebMediaStreamSource* source_ptr = FindLocalSource(device_info);
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
    const StreamDeviceInfo& device,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(CalledOnValidThread());

  blink::WebMediaStreamSource source = FindOrInitializeSourceObject(device);
  if (!source.getExtraData()) {
    source.setExtraData(CreateVideoSource(
        device, base::Bind(&UserMediaClientImpl::OnLocalSourceStopped,
                           weak_factory_.GetWeakPtr())));
    local_sources_.push_back(source);
  }
  return source;
}

blink::WebMediaStreamSource UserMediaClientImpl::InitializeAudioSourceObject(
    const StreamDeviceInfo& device,
    const blink::WebMediaConstraints& constraints,
    bool* is_pending) {
  DCHECK(CalledOnValidThread());

  *is_pending = true;

  // See if the source is already being initialized.
  auto* pending = FindPendingLocalSource(device);
  if (pending)
    return *pending;

  blink::WebMediaStreamSource source = FindOrInitializeSourceObject(device);
  if (source.getExtraData()) {
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

  MediaStreamAudioSource* const audio_source =
      CreateAudioSource(device, constraints, source_ready);
  audio_source->SetStopCallback(base::Bind(
      &UserMediaClientImpl::OnLocalSourceStopped, weak_factory_.GetWeakPtr()));
  source.setExtraData(audio_source);  // Takes ownership.
  return source;
}

MediaStreamAudioSource* UserMediaClientImpl::CreateAudioSource(
    const StreamDeviceInfo& device,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamSource::ConstraintsCallback& source_ready) {
  DCHECK(CalledOnValidThread());
  // If the audio device is a loopback device (for screen capture), or if the
  // constraints/effects parameters indicate no audio processing is needed,
  // create an efficient, direct-path MediaStreamAudioSource instance.
  if (IsScreenCaptureMediaType(device.device.type) ||
      !MediaStreamAudioProcessor::WouldModifyAudio(
          constraints, device.device.input.effects)) {
    return new LocalMediaStreamAudioSource(RenderFrameObserver::routing_id(),
                                           device, source_ready);
  }

  // The audio device is not associated with screen capture and also requires
  // processing.
  ProcessedLocalAudioSource* source = new ProcessedLocalAudioSource(
      RenderFrameObserver::routing_id(), device, constraints, source_ready,
      dependency_factory_);
  return source;
}

MediaStreamVideoSource* UserMediaClientImpl::CreateVideoSource(
    const StreamDeviceInfo& device,
    const MediaStreamSource::SourceStoppedCallback& stop_callback) {
  DCHECK(CalledOnValidThread());
  content::MediaStreamVideoCapturerSource* ret =
      new content::MediaStreamVideoCapturerSource(stop_callback, device,
                                                  render_frame());
  return ret;
}

void UserMediaClientImpl::CreateVideoTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks) {
  DCHECK(CalledOnValidThread());
  DCHECK(current_request_info_);
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  for (size_t i = 0; i < devices.size(); ++i) {
    blink::WebMediaStreamSource source =
        InitializeVideoSourceObject(devices[i], constraints);
    (*webkit_tracks)[i] =
        current_request_info_->CreateAndStartVideoTrack(source, constraints);
  }
}

void UserMediaClientImpl::CreateAudioTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks) {
  DCHECK(CalledOnValidThread());
  DCHECK(current_request_info_);
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  StreamDeviceInfoArray overridden_audio_array = devices;
  if (!current_request_info_->enable_automatic_output_device_selection()) {
    // If the GetUserMedia request did not explicitly set the constraint
    // kMediaStreamRenderToAssociatedSink, the output device parameters must
    // be removed.
    for (auto& device_info : overridden_audio_array) {
      device_info.device.matched_output_device_id = "";
      device_info.device.matched_output =
          MediaStreamDevice::AudioDeviceParameters();
    }
  }

  for (size_t i = 0; i < overridden_audio_array.size(); ++i) {
    bool is_pending = false;
    blink::WebMediaStreamSource source = InitializeAudioSourceObject(
        overridden_audio_array[i], constraints, &is_pending);
    (*webkit_tracks)[i].initialize(source);
    current_request_info_->StartAudioTrack((*webkit_tracks)[i], is_pending);
  }
}

void UserMediaClientImpl::OnCreateNativeTracksCompleted(
    const std::string& label,
    UserMediaRequestInfo* request_info,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DCHECK(CalledOnValidThread());
  if (result == content::MEDIA_DEVICE_OK) {
    GetUserMediaRequestSucceeded(*request_info->web_stream(),
                                 request_info->request());
    media_stream_dispatcher_->OnStreamStarted(label);
  } else {
    GetUserMediaRequestFailed(request_info->request(), result, result_name);

    blink::WebVector<blink::WebMediaStreamTrack> tracks;
    request_info->web_stream()->audioTracks(tracks);
    for (auto& web_track : tracks) {
      MediaStreamTrack* track = MediaStreamTrack::GetTrack(web_track);
      if (track)
        track->Stop();
    }
    request_info->web_stream()->videoTracks(tracks);
    for (auto& web_track : tracks) {
      MediaStreamTrack* track = MediaStreamTrack::GetTrack(web_track);
      if (track)
        track->Stop();
    }
  }

  DeleteRequestInfo(request_info->request());
}

void UserMediaClientImpl::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const StreamDeviceInfo& video_device) {
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
  if (!media_device_change_observer_.isNull())
    media_device_change_observer_.didChangeMediaDevices();
}

void UserMediaClientImpl::GetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest request) {
  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl is destroyed if the JavaScript code request the
  // frame to be destroyed within the scope of the callback. Therefore,
  // post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded,
                 weak_factory_.GetWeakPtr(), stream, request));
}

void UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest request) {
  DVLOG(1) << "UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded";
  LogUserMediaRequestResult(MEDIA_DEVICE_OK);
  DeleteRequestInfo(request);
  request.requestSucceeded(stream);
}

void UserMediaClientImpl::GetUserMediaRequestFailed(
    blink::WebUserMediaRequest request,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl is destroyed if the JavaScript code request the
  // frame to be destroyed within the scope of the callback. Therefore,
  // post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&UserMediaClientImpl::DelayedGetUserMediaRequestFailed,
                 weak_factory_.GetWeakPtr(), request, result, result_name));
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
      request.requestDenied();
      return;
    case MEDIA_DEVICE_PERMISSION_DISMISSED:
      request.requestFailedUASpecific("PermissionDismissedError");
      return;
    case MEDIA_DEVICE_INVALID_STATE:
      request.requestFailedUASpecific("InvalidStateError");
      return;
    case MEDIA_DEVICE_NO_HARDWARE:
      request.requestFailedUASpecific("DevicesNotFoundError");
      return;
    case MEDIA_DEVICE_INVALID_SECURITY_ORIGIN:
      request.requestFailedUASpecific("InvalidSecurityOriginError");
      return;
    case MEDIA_DEVICE_TAB_CAPTURE_FAILURE:
      request.requestFailedUASpecific("TabCaptureError");
      return;
    case MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE:
      request.requestFailedUASpecific("ScreenCaptureError");
      return;
    case MEDIA_DEVICE_CAPTURE_FAILURE:
      request.requestFailedUASpecific("DeviceCaptureError");
      return;
    case MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED:
      request.requestFailedConstraint(result_name);
      return;
    case MEDIA_DEVICE_TRACK_START_FAILURE:
      request.requestFailedUASpecific("TrackStartError");
      return;
    case MEDIA_DEVICE_NOT_SUPPORTED:
      request.requestFailedUASpecific("MediaDeviceNotSupported");
      return;
    case MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN:
      request.requestFailedUASpecific("MediaDeviceFailedDueToShutdown");
      return;
    case MEDIA_DEVICE_KILL_SWITCH_ON:
      request.requestFailedUASpecific("MediaDeviceKillSwitchOn");
      return;
  }
  NOTREACHED();
  request.requestFailed();
}

void UserMediaClientImpl::EnumerateDevicesSucceded(
    blink::WebMediaDevicesRequest* request,
    blink::WebVector<blink::WebMediaDeviceInfo>& devices) {
  request->requestSucceeded(devices);
}

const blink::WebMediaStreamSource* UserMediaClientImpl::FindLocalSource(
    const LocalStreamSources& sources,
    const StreamDeviceInfo& device) const {
  for (const auto& local_source : sources) {
    MediaStreamSource* const source =
        static_cast<MediaStreamSource*>(local_source.getExtraData());
    const StreamDeviceInfo& active_device = source->device_info();
    if (IsSameDevice(active_device, device))
      return &local_source;
  }
  return nullptr;
}

blink::WebMediaStreamSource UserMediaClientImpl::FindOrInitializeSourceObject(
    const StreamDeviceInfo& device) {
  const blink::WebMediaStreamSource* existing_source = FindLocalSource(device);
  if (existing_source) {
    DVLOG(1) << "Source already exists. Reusing source with id "
             << existing_source->id().utf8();
    return *existing_source;
  }

  blink::WebMediaStreamSource::Type type =
      IsAudioInputMediaType(device.device.type)
          ? blink::WebMediaStreamSource::TypeAudio
          : blink::WebMediaStreamSource::TypeVideo;

  blink::WebMediaStreamSource source;
  source.initialize(blink::WebString::fromUTF8(device.device.id), type,
                    blink::WebString::fromUTF8(device.device.name));

  DVLOG(1) << "Initialize source object :"
           << "id = " << source.id().utf8()
           << ", name = " << source.name().utf8();
  return source;
}

bool UserMediaClientImpl::RemoveLocalSource(
    const blink::WebMediaStreamSource& source) {
  DCHECK(CalledOnValidThread());

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
          static_cast<MediaStreamSource*>(source.getExtraData());
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
  DCHECK(CalledOnValidThread());
  return current_request_info_ &&
         current_request_info_->request_id() == request_id;
}

bool UserMediaClientImpl::IsCurrentRequestInfo(
    const blink::WebUserMediaRequest& request) const {
  DCHECK(CalledOnValidThread());
  return current_request_info_ && current_request_info_->request() == request;
}

bool UserMediaClientImpl::DeleteRequestInfo(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK(CalledOnValidThread());
  if (current_request_info_ &&
      current_request_info_->request() == user_media_request) {
    current_request_info_.reset();
    if (!pending_request_infos_.empty()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&UserMediaClientImpl::MaybeProcessNextRequestInfo,
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
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnLocalSourceStopped";

  const bool some_source_removed = RemoveLocalSource(source);
  CHECK(some_source_removed);

  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.getExtraData());
  media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());
}

void UserMediaClientImpl::StopLocalSource(
    const blink::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.getExtraData());
  DVLOG(1) << "UserMediaClientImpl::StopLocalSource("
           << "{device_id = " << source_impl->device_info().device.id << "})";

  if (notify_dispatcher)
    media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());

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

base::Optional<bool>
UserMediaClientImpl::AutomaticOutputDeviceSelectionEnabledForCurrentRequest() {
  if (!current_request_info_)
    return base::Optional<bool>();

  return base::Optional<bool>(
      current_request_info_->enable_automatic_output_device_selection());
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
      enable_automatic_output_device_selection_(false),
      request_(request),
      is_processing_user_gesture_(is_processing_user_gesture),
      security_origin_(security_origin),
      request_result_(MEDIA_DEVICE_OK),
      request_result_name_("") {}

void UserMediaClientImpl::UserMediaRequestInfo::StartAudioTrack(
    const blink::WebMediaStreamTrack& track,
    bool is_pending) {
  DCHECK(track.source().getType() == blink::WebMediaStreamSource::TypeAudio);
  MediaStreamAudioSource* native_source =
      MediaStreamAudioSource::From(track.source());
  // Add the source as pending since OnTrackStarted will expect it to be there.
  sources_waiting_for_callback_.push_back(native_source);

  sources_.push_back(track.source());
  bool connected = native_source->ConnectToTrack(track);
  if (!is_pending) {
    OnTrackStarted(
        native_source,
        connected ? MEDIA_DEVICE_OK : MEDIA_DEVICE_TRACK_START_FAILURE, "");
  }
}

blink::WebMediaStreamTrack
UserMediaClientImpl::UserMediaRequestInfo::CreateAndStartVideoTrack(
    const blink::WebMediaStreamSource& source,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(source.getType() == blink::WebMediaStreamSource::TypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  sources_.push_back(source);
  sources_waiting_for_callback_.push_back(native_source);
  return MediaStreamVideoTrack::CreateVideoTrack(
      native_source, constraints, base::Bind(
          &UserMediaClientImpl::UserMediaRequestInfo::OnTrackStarted,
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
