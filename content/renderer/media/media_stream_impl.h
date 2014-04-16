// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/renderer/media/media_stream_client.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/media/media_stream_source.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {
class MediaStreamAudioRenderer;
class MediaStreamDependencyFactory;
class MediaStreamDispatcher;
class MediaStreamVideoSource;
class VideoCapturerDelegate;
class WebRtcAudioRenderer;
class WebRtcLocalAudioRenderer;

// MediaStreamImpl is a delegate for the Media Stream API messages used by
// WebKit. It ties together WebKit, native PeerConnection in libjingle and
// MediaStreamManager (via MediaStreamDispatcher and MediaStreamDispatcherHost)
// in the browser process. It must be created, called and destroyed on the
// render thread.
// MediaStreamImpl have weak pointers to a MediaStreamDispatcher.
class CONTENT_EXPORT MediaStreamImpl
    : public RenderViewObserver,
      NON_EXPORTED_BASE(public blink::WebUserMediaClient),
      NON_EXPORTED_BASE(public MediaStreamClient),
      public MediaStreamDispatcherEventHandler,
      public base::SupportsWeakPtr<MediaStreamImpl>,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  MediaStreamImpl(
      RenderView* render_view,
      MediaStreamDispatcher* media_stream_dispatcher,
      MediaStreamDependencyFactory* dependency_factory);
  virtual ~MediaStreamImpl();

  // blink::WebUserMediaClient implementation
  virtual void requestUserMedia(
      const blink::WebUserMediaRequest& user_media_request) OVERRIDE;
  virtual void cancelUserMediaRequest(
      const blink::WebUserMediaRequest& user_media_request) OVERRIDE;

  // MediaStreamClient implementation.
  virtual bool IsMediaStream(const GURL& url) OVERRIDE;
  virtual scoped_refptr<VideoFrameProvider> GetVideoFrameProvider(
      const GURL& url,
      const base::Closure& error_cb,
      const VideoFrameProvider::RepaintCB& repaint_cb) OVERRIDE;
  virtual scoped_refptr<MediaStreamAudioRenderer>
      GetAudioRenderer(const GURL& url, int render_frame_id) OVERRIDE;

  // MediaStreamDispatcherEventHandler implementation.
  virtual void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const StreamDeviceInfoArray& audio_array,
      const StreamDeviceInfoArray& video_array) OVERRIDE;
  virtual void OnStreamGenerationFailed(
      int request_id,
      content::MediaStreamRequestResult result) OVERRIDE;
  virtual void OnDeviceStopped(const std::string& label,
                               const StreamDeviceInfo& device_info) OVERRIDE;
  virtual void OnDevicesEnumerated(
      int request_id,
      const StreamDeviceInfoArray& device_array) OVERRIDE;
  virtual void OnDeviceOpened(
      int request_id,
      const std::string& label,
      const StreamDeviceInfo& device_info) OVERRIDE;
  virtual void OnDeviceOpenFailed(int request_id) OVERRIDE;

  // RenderViewObserver OVERRIDE
  virtual void FrameDetached(blink::WebFrame* frame) OVERRIDE;
  virtual void FrameWillClose(blink::WebFrame* frame) OVERRIDE;

 protected:
  // Called when |source| has been stopped from JavaScript.
  void OnLocalSourceStopped(const blink::WebMediaStreamSource& source);

  // These methods are virtual for test purposes. A test can override them to
  // test requesting local media streams. The function notifies WebKit that the
  // |request| have completed.
  virtual void GetUserMediaRequestSucceeded(
       const blink::WebMediaStream& stream,
       blink::WebUserMediaRequest* request_info);
  virtual void GetUserMediaRequestFailed(
      blink::WebUserMediaRequest* request_info,
      content::MediaStreamRequestResult result);


  // Returns the WebKit representation of a MediaStream given an URL.
  // This is virtual for test purposes.
  virtual blink::WebMediaStream GetMediaStream(const GURL& url);

  // Creates a MediaStreamVideoSource object.
  // This is virtual for test purposes.
  virtual MediaStreamVideoSource* CreateVideoSource(
      const StreamDeviceInfo& device,
      const MediaStreamSource::SourceStoppedCallback& stop_callback);

 private:
  // Class for storing information about a WebKit request to create a
  // MediaStream.
  class UserMediaRequestInfo
      : public base::SupportsWeakPtr<UserMediaRequestInfo> {
   public:
    typedef base::Callback<void(UserMediaRequestInfo* request_info,
                                content::MediaStreamRequestResult result)>
      ResourcesReady;

    UserMediaRequestInfo(int request_id,
                         blink::WebFrame* frame,
                         const blink::WebUserMediaRequest& request,
                         bool enable_automatic_output_device_selection);
    ~UserMediaRequestInfo();
    int request_id;
    // True if MediaStreamDispatcher has generated the stream, see
    // OnStreamGenerated.
    bool generated;
    const bool enable_automatic_output_device_selection;
    blink::WebFrame* frame;  // WebFrame that requested the MediaStream.
    blink::WebMediaStream web_stream;
    blink::WebUserMediaRequest request;

    void StartAudioTrack(const blink::WebMediaStreamTrack& track,
                         const blink::WebMediaConstraints& constraints);

    blink::WebMediaStreamTrack CreateAndStartVideoTrack(
        const blink::WebMediaStreamSource& source,
        const blink::WebMediaConstraints& constraints,
        MediaStreamDependencyFactory* factory);

    // Triggers |callback| when all sources used in this request have either
    // successfully started, or a source has failed to start.
    void CallbackOnTracksStarted(const ResourcesReady& callback);

    bool IsSourceUsed(const blink::WebMediaStreamSource& source) const;
    void RemoveSource(const blink::WebMediaStreamSource& source);

    bool AreAllSourcesRemoved() const { return sources_.empty(); };

   private:
    void OnTrackStarted(MediaStreamSource* source, bool success);
    void CheckAllTracksStarted();

    ResourcesReady ready_callback_;
    bool request_failed_;
    // Sources used in this request.
    std::vector<blink::WebMediaStreamSource> sources_;
    std::vector<MediaStreamSource*> sources_waiting_for_callback_;
  };
  typedef ScopedVector<UserMediaRequestInfo> UserMediaRequests;

  struct LocalStreamSource {
    LocalStreamSource(blink::WebFrame* frame,
                      const blink::WebMediaStreamSource& source)
        : frame(frame), source(source) {
    }
    // |frame| is the WebFrame that requested |source|. NULL in unit tests.
    // TODO(perkj): Change so that |frame| is not NULL in unit tests.
    blink::WebFrame* frame;
    blink::WebMediaStreamSource source;
  };
  typedef std::vector<LocalStreamSource> LocalStreamSources;

  // Creates a WebKit representation of stream sources based on
  // |devices| from the MediaStreamDispatcher.
  void InitializeSourceObject(
      const StreamDeviceInfo& device,
      blink::WebMediaStreamSource::Type type,
      const blink::WebMediaConstraints& constraints,
      blink::WebFrame* frame,
      blink::WebMediaStreamSource* webkit_source);

  void CreateVideoTracks(
      const StreamDeviceInfoArray& devices,
      const blink::WebMediaConstraints& constraints,
      blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
      UserMediaRequestInfo* request);

  void CreateAudioTracks(
      const StreamDeviceInfoArray& devices,
      const blink::WebMediaConstraints& constraints,
      blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
      UserMediaRequestInfo* request);

  // Callback function triggered when all native versions of the
  // underlying media sources and tracks have been created and started.
  void OnCreateNativeTracksCompleted(
      UserMediaRequestInfo* request,
      content::MediaStreamRequestResult result);

  UserMediaRequestInfo* FindUserMediaRequestInfo(int request_id);
  UserMediaRequestInfo* FindUserMediaRequestInfo(
      const blink::WebUserMediaRequest& request);
  void DeleteUserMediaRequestInfo(UserMediaRequestInfo* request);

  // Returns the source that use a device with |device.session_id|
  // and |device.device.id|. NULL if such source doesn't exist.
  const blink::WebMediaStreamSource* FindLocalSource(
      const StreamDeviceInfo& device) const;

  void StopLocalSource(const blink::WebMediaStreamSource& source,
                       bool notify_dispatcher);

  scoped_refptr<WebRtcAudioRenderer> CreateRemoteAudioRenderer(
      webrtc::MediaStreamInterface* stream, int render_frame_id);
  scoped_refptr<WebRtcLocalAudioRenderer> CreateLocalAudioRenderer(
      const blink::WebMediaStreamTrack& audio_track,
      int render_frame_id);

  // Returns a valid session id if a single capture device is currently open
  // (and then the matching session_id), otherwise -1.
  // This is used to pass on a session id to a webrtc audio renderer (either
  // local or remote), so that audio will be rendered to a matching output
  // device, should one exist.
  // Note that if there are more than one open capture devices the function
  // will not be able to pick an appropriate device and return false.
  bool GetAuthorizedDeviceInfoForAudioRenderer(
      int* session_id, int* output_sample_rate, int* output_buffer_size);

  // Weak ref to a MediaStreamDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  MediaStreamDependencyFactory* dependency_factory_;

  // media_stream_dispatcher_ is a weak reference, owned by RenderView. It's
  // valid for the lifetime of RenderView.
  MediaStreamDispatcher* media_stream_dispatcher_;

  LocalStreamSources local_sources_;

  UserMediaRequests user_media_requests_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_IMPL_H_
