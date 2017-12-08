// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_

#include <list>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/media/media_devices_event_dispatcher.h"
#include "content/renderer/media/user_media_processor.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/mediastream/media_devices.mojom.h"
#include "third_party/WebKit/public/web/WebApplyConstraintsRequest.h"
#include "third_party/WebKit/public/web/WebMediaDeviceChangeObserver.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"

namespace content {

class ApplyConstraintsProcessor;
class MediaStreamDeviceObserver;
class PeerConnectionDependencyFactory;

// UserMediaClientImpl handles requests coming from the Blink MediaDevices
// object. This includes getUserMedia and enumerateDevices. It must be created,
// called and destroyed on the render thread.
class CONTENT_EXPORT UserMediaClientImpl : public RenderFrameObserver,
                                           public blink::WebUserMediaClient {
 public:
  // TODO(guidou): Make all constructors private and replace with Create methods
  // that return a std::unique_ptr. This class is intended for instantiation on
  // the free store. http://crbug.com/764293
  // |render_frame| and |dependency_factory| must outlive this instance.
  UserMediaClientImpl(
      RenderFrame* render_frame,
      PeerConnectionDependencyFactory* dependency_factory,
      std::unique_ptr<MediaStreamDeviceObserver> media_stream_device_observer);
  UserMediaClientImpl(RenderFrame* render_frame,
                      std::unique_ptr<UserMediaProcessor> user_media_processor);
  ~UserMediaClientImpl() override;

  MediaStreamDeviceObserver* media_stream_device_observer() const {
    return user_media_processor_->media_stream_device_observer();
  }

  // blink::WebUserMediaClient implementation
  void RequestUserMedia(const blink::WebUserMediaRequest& web_request) override;
  void CancelUserMediaRequest(
      const blink::WebUserMediaRequest& web_request) override;
  void SetMediaDeviceChangeObserver(
      const blink::WebMediaDeviceChangeObserver& observer) override;
  void ApplyConstraints(
      const blink::WebApplyConstraintsRequest& web_request) override;
  void StopTrack(const blink::WebMediaStreamTrack& web_track) override;

  // RenderFrameObserver override
  void WillCommitProvisionalLoad() override;

  void SetMediaDevicesDispatcherForTesting(
      blink::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher);

 private:
  class Request {
   public:
    explicit Request(std::unique_ptr<UserMediaRequest> request);
    explicit Request(const blink::WebApplyConstraintsRequest& request);
    explicit Request(const blink::WebMediaStreamTrack& request);
    Request(Request&& other);
    Request& operator=(Request&& other);
    ~Request();

    std::unique_ptr<UserMediaRequest> MoveUserMediaRequest();

    UserMediaRequest* user_media_request() const {
      return user_media_request_.get();
    }
    const blink::WebApplyConstraintsRequest& apply_constraints_request() const {
      return apply_constraints_request_;
    }
    const blink::WebMediaStreamTrack& web_track_to_stop() const {
      return web_track_to_stop_;
    }

    bool IsUserMedia() const { return !!user_media_request_; }
    bool IsApplyConstraints() const {
      return !apply_constraints_request_.IsNull();
    }
    bool IsStopTrack() const { return !web_track_to_stop_.IsNull(); }

   private:
    std::unique_ptr<UserMediaRequest> user_media_request_;
    blink::WebApplyConstraintsRequest apply_constraints_request_;
    blink::WebMediaStreamTrack web_track_to_stop_;
  };

  void MaybeProcessNextRequestInfo();
  void CurrentRequestCompleted();

  void DeleteAllUserMediaRequests();

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Callback invoked by MediaDevicesEventDispatcher when a device-change
  // notification arrives.
  void DevicesChanged(MediaDeviceType device_type,
                      const MediaDeviceInfoArray& device_infos);

  const blink::mojom::MediaDevicesDispatcherHostPtr&
  GetMediaDevicesDispatcher();

  // |user_media_processor_| is a unique_ptr for testing purposes.
  std::unique_ptr<UserMediaProcessor> user_media_processor_;
  // |user_media_processor_| is a unique_ptr in order to avoid compilation
  // problems in builds that do not include WebRTC.
  std::unique_ptr<ApplyConstraintsProcessor> apply_constraints_processor_;

  blink::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher_;

  // UserMedia requests are processed sequentially. |is_processing_request_|
  // is a flag that indicates if a request is being processed at a given time,
  // and |pending_request_infos_| is a list of queued requests.
  bool is_processing_request_ = false;
  std::list<Request> pending_request_infos_;

  MediaDevicesEventDispatcher::SubscriptionIdList
      device_change_subscription_ids_;

  blink::WebMediaDeviceChangeObserver media_device_change_observer_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Note: This member must be the last to ensure all outstanding weak pointers
  // are invalidated first.
  base::WeakPtrFactory<UserMediaClientImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserMediaClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_
