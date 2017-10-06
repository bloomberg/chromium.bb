// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/user_media_client_impl.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/apply_constraints_processor.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"

namespace content {
namespace {

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

static int g_next_request_id = 0;

}  // namespace

UserMediaClientImpl::Request::Request(std::unique_ptr<UserMediaRequest> request)
    : user_media_request_(std::move(request)) {
  DCHECK(user_media_request_);
  DCHECK(apply_constraints_request_.IsNull());
}

UserMediaClientImpl::Request::Request(
    const blink::WebApplyConstraintsRequest& request)
    : apply_constraints_request_(request) {
  DCHECK(!apply_constraints_request_.IsNull());
  DCHECK(!user_media_request_);
}

UserMediaClientImpl::Request::Request(Request&& other)
    : user_media_request_(std::move(other.user_media_request_)),
      apply_constraints_request_(other.apply_constraints_request_) {
  DCHECK(!IsApplyConstraints() || !IsUserMedia());
}

UserMediaClientImpl::Request& UserMediaClientImpl::Request::operator=(
    Request&& other) = default;
UserMediaClientImpl::Request::~Request() = default;

std::unique_ptr<UserMediaRequest>
UserMediaClientImpl::Request::MoveUserMediaRequest() {
  return std::move(user_media_request_);
}

UserMediaClientImpl::UserMediaClientImpl(
    RenderFrame* render_frame,
    std::unique_ptr<UserMediaProcessor> user_media_processor)
    : RenderFrameObserver(render_frame),
      user_media_processor_(std::move(user_media_processor)),
      apply_constraints_processor_(new ApplyConstraintsProcessor(
          base::BindRepeating(&UserMediaClientImpl::GetMediaDevicesDispatcher,
                              base::Unretained(this)))),
      weak_factory_(this) {}

// base::Unretained(this) is safe here because |this| owns
// |user_media_processor_|.
UserMediaClientImpl::UserMediaClientImpl(
    RenderFrame* render_frame,
    PeerConnectionDependencyFactory* dependency_factory,
    std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher,
    const scoped_refptr<base::TaskRunner>& worker_task_runner)
    : UserMediaClientImpl(
          render_frame,
          base::MakeUnique<UserMediaProcessor>(
              render_frame,
              dependency_factory,
              std::move(media_stream_dispatcher),
              base::BindRepeating(
                  &UserMediaClientImpl::GetMediaDevicesDispatcher,
                  base::Unretained(this)),
              worker_task_runner)) {}

UserMediaClientImpl::~UserMediaClientImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Force-close all outstanding user media requests and local sources here,
  // before the outstanding WeakPtrs are invalidated, to ensure a clean
  // shutdown.
  WillCommitProvisionalLoad();
}

void UserMediaClientImpl::RequestUserMedia(
    const blink::WebUserMediaRequest& web_request) {
  // Save histogram data so we can see how much GetUserMedia is used.
  // The histogram counts the number of calls to the JS API
  // webGetUserMedia.
  UpdateWebRTCMethodCount(WEBKIT_GET_USER_MEDIA);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!web_request.IsNull());
  DCHECK(web_request.Audio() || web_request.Video());
  // ownerDocument may be null if we are in a test.
  // In that case, it's OK to not check frame().
  DCHECK(web_request.OwnerDocument().IsNull() ||
         render_frame()->GetWebFrame() ==
             static_cast<blink::WebFrame*>(
                 web_request.OwnerDocument().GetFrame()));

  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->peer_connection_tracker()->TrackGetUserMedia(
        web_request);
  }

  int request_id = g_next_request_id++;
  WebRtcLogMessage(base::StringPrintf(
      "UMCI::RequestUserMedia. request_id=%d, audio constraints=%s, "
      "video constraints=%s",
      request_id, web_request.AudioConstraints().ToString().Utf8().c_str(),
      web_request.VideoConstraints().ToString().Utf8().c_str()));

  // The value returned by isProcessingUserGesture() is used by the browser to
  // make decisions about the permissions UI. Its value can be lost while
  // switching threads, so saving its value here.
  std::unique_ptr<UserMediaRequest> request_info =
      base::MakeUnique<UserMediaRequest>(
          request_id, web_request,
          blink::WebUserGestureIndicator::IsProcessingUserGesture());
  pending_request_infos_.push_back(Request(std::move(request_info)));
  if (!is_processing_request_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UserMediaClientImpl::MaybeProcessNextRequestInfo,
                       weak_factory_.GetWeakPtr()));
  }
}

void UserMediaClientImpl::ApplyConstraints(
    const blink::WebApplyConstraintsRequest& web_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(guidou): Implement applyConstraints(). http://crbug.com/338503
  pending_request_infos_.push_back(Request(web_request));
  if (!is_processing_request_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UserMediaClientImpl::MaybeProcessNextRequestInfo,
                       weak_factory_.GetWeakPtr()));
  }
}

void UserMediaClientImpl::MaybeProcessNextRequestInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_processing_request_ || pending_request_infos_.empty())
    return;

  Request current_request = std::move(pending_request_infos_.front());
  pending_request_infos_.pop_front();
  is_processing_request_ = true;

  // base::Unretained() is safe here because |this| owns
  // |user_media_processor_|.
  if (current_request.IsUserMedia()) {
    user_media_processor_->ProcessRequest(
        current_request.MoveUserMediaRequest(),
        base::BindOnce(&UserMediaClientImpl::CurrentRequestCompleted,
                       base::Unretained(this)));
  } else {
    DCHECK(current_request.IsApplyConstraints());
    apply_constraints_processor_->ProcessRequest(
        current_request.apply_constraints_request(),
        base::BindOnce(&UserMediaClientImpl::CurrentRequestCompleted,
                       base::Unretained(this)));
  }
}

void UserMediaClientImpl::CurrentRequestCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_processing_request_ = false;
  if (!pending_request_infos_.empty()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UserMediaClientImpl::MaybeProcessNextRequestInfo,
                       weak_factory_.GetWeakPtr()));
  }
}

void UserMediaClientImpl::CancelUserMediaRequest(
    const blink::WebUserMediaRequest& web_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    // TODO(guidou): Remove this conditional logging. http://crbug.com/764293
    UserMediaRequest* request = user_media_processor_->CurrentRequest();
    if (request && request->web_request == web_request) {
      WebRtcLogMessage(base::StringPrintf(
          "UMCI::CancelUserMediaRequest. request_id=%d", request->request_id));
    }
  }

  bool did_remove_request = false;
  if (user_media_processor_->DeleteWebRequest(web_request)) {
    did_remove_request = true;
  } else {
    for (auto it = pending_request_infos_.begin();
         it != pending_request_infos_.end(); ++it) {
      if (it->IsUserMedia() &&
          it->user_media_request()->web_request == web_request) {
        pending_request_infos_.erase(it);
        did_remove_request = true;
        break;
      }
    }
  }

  if (did_remove_request) {
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

void UserMediaClientImpl::DevicesChanged(
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  if (!media_device_change_observer_.IsNull())
    media_device_change_observer_.DidChangeMediaDevices();
}

void UserMediaClientImpl::EnumerateDevicesSucceded(
    blink::WebMediaDevicesRequest* request,
    blink::WebVector<blink::WebMediaDeviceInfo>& devices) {
  request->RequestSucceeded(devices);
}

void UserMediaClientImpl::DeleteAllUserMediaRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_media_processor_->StopAllProcessing();
  is_processing_request_ = false;
  pending_request_infos_.clear();
}

void UserMediaClientImpl::WillCommitProvisionalLoad() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel all outstanding UserMediaRequests.
  DeleteAllUserMediaRequests();
}

void UserMediaClientImpl::SetMediaDevicesDispatcherForTesting(
    ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher) {
  media_devices_dispatcher_ = std::move(media_devices_dispatcher);
}

const ::mojom::MediaDevicesDispatcherHostPtr&
UserMediaClientImpl::GetMediaDevicesDispatcher() {
  if (!media_devices_dispatcher_) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&media_devices_dispatcher_));
  }

  return media_devices_dispatcher_;
}

void UserMediaClientImpl::OnDestruct() {
  delete this;
}

}  // namespace content
