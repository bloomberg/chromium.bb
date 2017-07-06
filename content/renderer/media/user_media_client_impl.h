// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.h"
#include "content/common/media/media_devices.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/media/media_devices_event_dispatcher.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/media/media_stream_source.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebMediaDeviceChangeObserver.h"
#include "third_party/WebKit/public/web/WebMediaDevicesRequest.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"

namespace base {
class TaskRunner;
}

namespace content {
class AudioCaptureSettings;
class MediaStreamAudioSource;
class MediaStreamDispatcher;
class MediaStreamVideoSource;
class PeerConnectionDependencyFactory;
class VideoCaptureSettings;

// UserMediaClientImpl is a delegate for the Media Stream GetUserMedia API.
// It ties together WebKit and MediaStreamManager
// (via MediaStreamDispatcher and MediaStreamDispatcherHost)
// in the browser process. It must be created, called and destroyed on the
// render thread.
class CONTENT_EXPORT UserMediaClientImpl
    : public RenderFrameObserver,
      NON_EXPORTED_BASE(public blink::WebUserMediaClient),
      public MediaStreamDispatcherEventHandler {
 public:
  // |render_frame| and |dependency_factory| must outlive this instance.
  UserMediaClientImpl(
      RenderFrame* render_frame,
      PeerConnectionDependencyFactory* dependency_factory,
      std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher,
      const scoped_refptr<base::TaskRunner>& worker_task_runner);
  ~UserMediaClientImpl() override;

  MediaStreamDispatcher* media_stream_dispatcher() const {
    return media_stream_dispatcher_.get();
  }

  // blink::WebUserMediaClient implementation
  void RequestUserMedia(
      const blink::WebUserMediaRequest& user_media_request) override;
  void CancelUserMediaRequest(
      const blink::WebUserMediaRequest& user_media_request) override;
  void RequestMediaDevices(
      const blink::WebMediaDevicesRequest& media_devices_request) override;
  void SetMediaDeviceChangeObserver(
      const blink::WebMediaDeviceChangeObserver& observer) override;

  // MediaStreamDispatcherEventHandler implementation.
  void OnStreamGenerated(int request_id,
                         const std::string& label,
                         const StreamDeviceInfoArray& audio_array,
                         const StreamDeviceInfoArray& video_array) override;
  void OnStreamGenerationFailed(int request_id,
                                MediaStreamRequestResult result) override;
  void OnDeviceStopped(const std::string& label,
                       const StreamDeviceInfo& device_info) override;
  void OnDeviceOpened(int request_id,
                      const std::string& label,
                      const StreamDeviceInfo& device_info) override;
  void OnDeviceOpenFailed(int request_id) override;

  // RenderFrameObserver override
  void WillCommitProvisionalLoad() override;

  void SetMediaDevicesDispatcherForTesting(
      ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher);

 protected:
  // These methods are virtual for test purposes. A test can override them to
  // test requesting local media streams. The function notifies WebKit that the
  // |request| have completed.
  virtual void GetUserMediaRequestSucceeded(const blink::WebMediaStream& stream,
                                            blink::WebUserMediaRequest request);
  virtual void GetUserMediaRequestFailed(MediaStreamRequestResult result,
                                         const blink::WebString& result_name);

  virtual void EnumerateDevicesSucceded(
      blink::WebMediaDevicesRequest* request,
      blink::WebVector<blink::WebMediaDeviceInfo>& devices);

  // Creates a MediaStreamAudioSource/MediaStreamVideoSource objects.
  // These are virtual for test purposes.
  virtual MediaStreamAudioSource* CreateAudioSource(
      const StreamDeviceInfo& device,
      const blink::WebMediaConstraints& constraints,
      const MediaStreamSource::ConstraintsCallback& source_ready,
      bool* has_sw_echo_cancellation);
  virtual MediaStreamVideoSource* CreateVideoSource(
      const StreamDeviceInfo& device,
      const MediaStreamSource::SourceStoppedCallback& stop_callback);

  // Returns no value if there is no request being processed. Use only for
  // testing.
  // TODO(guidou): Remove this function. http://crbug.com/706408
  base::Optional<bool> AutomaticOutputDeviceSelectionEnabledForCurrentRequest();

  // Intended to be used only for testing.
  const AudioCaptureSettings& AudioCaptureSettingsForTesting() const;
  const VideoCaptureSettings& VideoCaptureSettingsForTesting() const;

 private:
  class UserMediaRequestInfo;
  typedef std::vector<blink::WebMediaStreamSource> LocalStreamSources;

  void MaybeProcessNextRequestInfo();
  bool IsCurrentRequestInfo(int request_id) const;
  bool IsCurrentRequestInfo(const blink::WebUserMediaRequest& request) const;
  bool DeleteRequestInfo(const blink::WebUserMediaRequest& request);
  void DelayedGetUserMediaRequestSucceeded(const blink::WebMediaStream& stream,
                                           blink::WebUserMediaRequest request);
  void DelayedGetUserMediaRequestFailed(blink::WebUserMediaRequest request,
                                        MediaStreamRequestResult result,
                                        const blink::WebString& result_name);

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Called when |source| has been stopped from JavaScript.
  void OnLocalSourceStopped(const blink::WebMediaStreamSource& source);

  // Creates a WebKit representation of stream sources based on
  // |devices| from the MediaStreamDispatcher.
  blink::WebMediaStreamSource InitializeVideoSourceObject(
      const StreamDeviceInfo& device);

  blink::WebMediaStreamSource InitializeAudioSourceObject(
      const StreamDeviceInfo& device,
      const blink::WebMediaConstraints& constraints,
      bool* is_pending);

  void CreateVideoTracks(
      const StreamDeviceInfoArray& devices,
      blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks);

  void CreateAudioTracks(
      const StreamDeviceInfoArray& devices,
      const blink::WebMediaConstraints& constraints,
      blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks);

  // Callback function triggered when all native versions of the
  // underlying media sources and tracks have been created and started.
  void OnCreateNativeTracksCompleted(const std::string& label,
                                     UserMediaRequestInfo* request,
                                     MediaStreamRequestResult result,
                                     const blink::WebString& result_name);

  void OnStreamGeneratedForCancelledRequest(
      const StreamDeviceInfoArray& audio_array,
      const StreamDeviceInfoArray& video_array);

  static void OnAudioSourceStartedOnAudioThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<UserMediaClientImpl> weak_ptr,
      MediaStreamSource* source,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);

  void OnAudioSourceStarted(MediaStreamSource* source,
                            MediaStreamRequestResult result,
                            const blink::WebString& result_name);

  void NotifyCurrentRequestInfoOfAudioSourceStarted(
      MediaStreamSource* source,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);

  using EnumerationResult = std::vector<MediaDeviceInfoArray>;
  void FinalizeEnumerateDevices(blink::WebMediaDevicesRequest request,
                                const EnumerationResult& result);

  void DeleteAllUserMediaRequests();

  // Returns the source that use a device with |device.session_id|
  // and |device.device.id|. NULL if such source doesn't exist.
  const blink::WebMediaStreamSource* FindLocalSource(
      const StreamDeviceInfo& device) const {
    return FindLocalSource(local_sources_, device);
  }
  const blink::WebMediaStreamSource* FindPendingLocalSource(
      const StreamDeviceInfo& device) const {
    return FindLocalSource(pending_local_sources_, device);
  }
  const blink::WebMediaStreamSource* FindLocalSource(
      const LocalStreamSources& sources,
      const StreamDeviceInfo& device) const;

  // Looks up a local source and returns it if found. If not found, prepares
  // a new WebMediaStreamSource with a NULL extraData pointer.
  blink::WebMediaStreamSource FindOrInitializeSourceObject(
      const StreamDeviceInfo& device);

  // Returns true if we do find and remove the |source|.
  // Otherwise returns false.
  bool RemoveLocalSource(const blink::WebMediaStreamSource& source);

  void StopLocalSource(const blink::WebMediaStreamSource& source,
                       bool notify_dispatcher);

  const ::mojom::MediaDevicesDispatcherHostPtr& GetMediaDevicesDispatcher();

  // TODO(guidou): Remove these functions. http://crbug.com/706408
  void LegacySetupAudioInput();
  void LegacySelectAudioInputDevice(
      const blink::WebUserMediaRequest& user_media_request,
      const EnumerationResult& device_enumeration);

  void SetupAudioInput();
  void SelectAudioSettings(const blink::WebUserMediaRequest& user_media_request,
                           std::vector<::mojom::AudioInputDeviceCapabilitiesPtr>
                               audio_input_capabilities);

  void SetupVideoInput();
  void SelectVideoDeviceSettings(
      const blink::WebUserMediaRequest& user_media_request,
      std::vector<::mojom::VideoInputDeviceCapabilitiesPtr>
          video_input_capabilities);
  void FinalizeSelectVideoDeviceSettings(
      const blink::WebUserMediaRequest& user_media_request,
      const VideoCaptureSettings& settings);
  void FinalizeSelectVideoContentSettings(
      const blink::WebUserMediaRequest& user_media_request,
      const VideoCaptureSettings& settings);

  void GenerateStreamForCurrentRequestInfo();

  // Callback invoked by MediaDevicesEventDispatcher when a device-change
  // notification arrives.
  void DevicesChanged(MediaDeviceType device_type,
                      const MediaDeviceInfoArray& device_infos);

  // Weak ref to a PeerConnectionDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  // TODO(xians): Remove this dependency once audio do not need it for local
  // audio.
  PeerConnectionDependencyFactory* const dependency_factory_;

  // UserMediaClientImpl owns MediaStreamDispatcher instead of RenderFrameImpl
  // (or RenderFrameObserver) to ensure tear-down occurs in the right order.
  const std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher_;

  ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher_;

  LocalStreamSources local_sources_;
  LocalStreamSources pending_local_sources_;

  // UserMedia requests are processed sequentially. |current_request_info_|
  // contains the request currently being processed, if any, and
  // |pending_request_infos_| is a list of queued requests.
  std::unique_ptr<UserMediaRequestInfo> current_request_info_;
  std::list<std::unique_ptr<UserMediaRequestInfo>> pending_request_infos_;

  MediaDevicesEventDispatcher::SubscriptionIdList
      device_change_subscription_ids_;

  blink::WebMediaDeviceChangeObserver media_device_change_observer_;

  const scoped_refptr<base::TaskRunner> worker_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Note: This member must be the last to ensure all outstanding weak pointers
  // are invalidated first.
  base::WeakPtrFactory<UserMediaClientImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserMediaClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_
