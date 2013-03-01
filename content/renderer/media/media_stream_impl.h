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
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebUserMediaClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebUserMediaRequest.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "webkit/media/media_stream_client.h"

namespace webkit_media {
class MediaStreamAudioRenderer;
}

namespace content {
class MediaStreamDependencyFactory;
class MediaStreamDispatcher;
class VideoCaptureImplManager;
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
      NON_EXPORTED_BASE(public WebKit::WebUserMediaClient),
      NON_EXPORTED_BASE(public webkit_media::MediaStreamClient),
      public MediaStreamDispatcherEventHandler,
      public base::SupportsWeakPtr<MediaStreamImpl>,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  MediaStreamImpl(
      RenderView* render_view,
      MediaStreamDispatcher* media_stream_dispatcher,
      VideoCaptureImplManager* vc_manager,
      MediaStreamDependencyFactory* dependency_factory);
  virtual ~MediaStreamImpl();

  // Return true when the |url| is media stream.
  // This static function has the same functionalilty as IsMediaStream
  // except that it doesn't require an instance of this class.
  // This can save some overhead time when the |url| is not media stream.
  static bool CheckMediaStream(const GURL& url);

  // WebKit::WebUserMediaClient implementation
  virtual void requestUserMedia(
      const WebKit::WebUserMediaRequest& user_media_request,
      const WebKit::WebVector<WebKit::WebMediaStreamSource>& audio_sources,
      const WebKit::WebVector<WebKit::WebMediaStreamSource>& video_sources)
      OVERRIDE;
  virtual void cancelUserMediaRequest(
      const WebKit::WebUserMediaRequest& user_media_request) OVERRIDE;

  // webkit_media::MediaStreamClient implementation.
  virtual bool IsMediaStream(const GURL& url) OVERRIDE;
  virtual scoped_refptr<webkit_media::VideoFrameProvider> GetVideoFrameProvider(
      const GURL& url,
      const base::Closure& error_cb,
      const webkit_media::VideoFrameProvider::RepaintCB& repaint_cb) OVERRIDE;
  virtual scoped_refptr<webkit_media::MediaStreamAudioRenderer>
      GetAudioRenderer(const GURL& url) OVERRIDE;

  // MediaStreamDispatcherEventHandler implementation.
  virtual void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const StreamDeviceInfoArray& audio_array,
      const StreamDeviceInfoArray& video_array) OVERRIDE;
  virtual void OnStreamGenerationFailed(int request_id) OVERRIDE;
  virtual void OnDevicesEnumerated(
      int request_id,
      const StreamDeviceInfoArray& device_array) OVERRIDE;
  virtual void OnDevicesEnumerationFailed(int request_id) OVERRIDE;
  virtual void OnDeviceOpened(
      int request_id,
      const std::string& label,
      const StreamDeviceInfo& device_info) OVERRIDE;
  virtual void OnDeviceOpenFailed(int request_id) OVERRIDE;

  // RenderViewObserver OVERRIDE
  virtual void FrameWillClose(WebKit::WebFrame* frame) OVERRIDE;

 protected:
  // Stops a local MediaStream by notifying the MediaStreamDispatcher that the
  // stream no longer may be used.
  void OnLocalMediaStreamStop(const std::string& label);

  // Callback function triggered when all native (libjingle) versions of the
  // underlying media sources have been created and started.
  // |description| is a raw pointer to the description in
  // UserMediaRequests::description for which the underlying sources have been
  // created.
  void OnCreateNativeSourcesComplete(
      WebKit::WebMediaStream* description,
      bool request_succeeded);

  // This function is virtual for test purposes. A test can override this to
  // test requesting local media streams. The function notifies WebKit that the
  // |request| have completed and generated the MediaStream |stream|.
  virtual void CompleteGetUserMediaRequest(
      const WebKit::WebMediaStream& stream,
      WebKit::WebUserMediaRequest* request_info,
      bool request_succeeded);

  // Returns the WebKit representation of a MediaStream given an URL.
  // This is virtual for test purposes.
  virtual WebKit::WebMediaStream GetMediaStream(const GURL& url);

 private:
  // Structure for storing information about a WebKit request to create a
  // MediaStream.
  struct UserMediaRequestInfo {
    UserMediaRequestInfo()
        : request_id(0), generated(false), frame(NULL), request() {
    }
    UserMediaRequestInfo(int request_id,
                         WebKit::WebFrame* frame,
                         const WebKit::WebUserMediaRequest& request)
        : request_id(request_id), generated(false), frame(frame),
          request(request) {
    }
    int request_id;
    // True if MediaStreamDispatcher has generated the stream, see
    // OnStreamGenerated.
    bool generated;
    WebKit::WebFrame* frame;  // WebFrame that requested the MediaStream.
    WebKit::WebMediaStream descriptor;
    WebKit::WebUserMediaRequest request;
  };
  typedef ScopedVector<UserMediaRequestInfo> UserMediaRequests;

  UserMediaRequestInfo* FindUserMediaRequestInfo(int request_id);
  UserMediaRequestInfo* FindUserMediaRequestInfo(
      WebKit::WebMediaStream* descriptor);
  UserMediaRequestInfo* FindUserMediaRequestInfo(
      const WebKit::WebUserMediaRequest& request);
  UserMediaRequestInfo* FindUserMediaRequestInfo(const std::string& label);
  void DeleteUserMediaRequestInfo(UserMediaRequestInfo* request);

  scoped_refptr<webkit_media::VideoFrameProvider>
  CreateVideoFrameProvider(
      webrtc::MediaStreamInterface* stream,
      const base::Closure& error_cb,
      const webkit_media::VideoFrameProvider::RepaintCB& repaint_cb);
  scoped_refptr<WebRtcAudioRenderer> CreateRemoteAudioRenderer(
      webrtc::MediaStreamInterface* stream);
  scoped_refptr<WebRtcLocalAudioRenderer> CreateLocalAudioRenderer(
      webrtc::MediaStreamInterface* stream);

  // Weak ref to a MediaStreamDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  MediaStreamDependencyFactory* dependency_factory_;

  // media_stream_dispatcher_ is a weak reference, owned by RenderView. It's
  // valid for the lifetime of RenderView.
  MediaStreamDispatcher* media_stream_dispatcher_;

  scoped_refptr<VideoCaptureImplManager> vc_manager_;

  UserMediaRequests user_media_requests_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_IMPL_H_
