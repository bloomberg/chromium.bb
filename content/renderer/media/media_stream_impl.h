// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_IMPL_H_

#include <list>
#include <map>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastream.h"
#include "third_party/libjingle/source/talk/base/scoped_ref_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebUserMediaClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebUserMediaRequest.h"
#include "webkit/media/media_stream_client.h"

namespace base {
class WaitableEvent;
}

namespace content {
class IpcNetworkManager;
class IpcPacketSocketFactory;
class P2PSocketDispatcher;
}

namespace talk_base {
class Thread;
}

namespace WebKit {
class WebFrame;
class WebMediaStreamComponent;
class WebMediaStreamDescriptor;
class WebPeerConnection00Handler;
class WebPeerConnection00HandlerClient;
class WebPeerConnectionHandler;
class WebPeerConnectionHandlerClient;
}

class MediaStreamDispatcher;
class MediaStreamDependencyFactory;
class PeerConnectionHandlerBase;
class VideoCaptureImplManager;
class RTCVideoDecoder;

// MediaStreamImpl is a delegate for the Media Stream API messages used by
// WebKit. It ties together WebKit, native PeerConnection in libjingle and
// MediaStreamManager (via MediaStreamDispatcher and MediaStreamDispatcherHost)
// in the browser process. It must be created, called and destroyed on the
// render thread.
// MediaStreamImpl have weak pointers to a P2PSocketDispatcher and a
// MediaStreamDispatcher. These objects are also RenderViewObservers.
// MediaStreamImpl must be deleted before the P2PSocketDispatcher.
class CONTENT_EXPORT MediaStreamImpl
    : public content::RenderViewObserver,
      NON_EXPORTED_BASE(public WebKit::WebUserMediaClient),
      NON_EXPORTED_BASE(public webkit_media::MediaStreamClient),
      public MediaStreamDispatcherEventHandler,
      public base::SupportsWeakPtr<MediaStreamImpl>,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  MediaStreamImpl(
      content::RenderView* render_view,
      MediaStreamDispatcher* media_stream_dispatcher,
      content::P2PSocketDispatcher* p2p_socket_dispatcher,
      VideoCaptureImplManager* vc_manager,
      MediaStreamDependencyFactory* dependency_factory);
  virtual ~MediaStreamImpl();

  virtual WebKit::WebPeerConnectionHandler* CreatePeerConnectionHandler(
      WebKit::WebPeerConnectionHandlerClient* client);
  virtual WebKit::WebPeerConnection00Handler* CreatePeerConnectionHandlerJsep(
      WebKit::WebPeerConnection00HandlerClient* client);
  // Stops a local MediaStream by notifying the MediaStreamDispatcher that the
  // stream no longer may be used.
  virtual void StopLocalMediaStream(
      const WebKit::WebMediaStreamDescriptor& stream);
  // A new MediaStream have been created based on existing tracks.
  virtual void CreateMediaStream(
      WebKit::WebFrame* frame,
      WebKit::WebMediaStreamDescriptor* stream);

  // WebKit::WebUserMediaClient implementation
  virtual void requestUserMedia(
      const WebKit::WebUserMediaRequest& user_media_request,
      const WebKit::WebVector<WebKit::WebMediaStreamSource>& audio_sources,
      const WebKit::WebVector<WebKit::WebMediaStreamSource>& video_sources)
      OVERRIDE;
  virtual void cancelUserMediaRequest(
      const WebKit::WebUserMediaRequest& user_media_request) OVERRIDE;

  // webkit_media::MediaStreamClient implementation.
  virtual scoped_refptr<media::VideoDecoder> GetVideoDecoder(
      const GURL& url,
      media::MessageLoopFactory* message_loop_factory) OVERRIDE;

  // MediaStreamDispatcherEventHandler implementation.
  virtual void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const media_stream::StreamDeviceInfoArray& audio_array,
      const media_stream::StreamDeviceInfoArray& video_array) OVERRIDE;
  virtual void OnStreamGenerationFailed(int request_id) OVERRIDE;
  virtual void OnVideoDeviceFailed(
      const std::string& label,
      int index) OVERRIDE;
  virtual void OnAudioDeviceFailed(
      const std::string& label,
      int index) OVERRIDE;
  virtual void OnDevicesEnumerated(
      int request_id,
      const media_stream::StreamDeviceInfoArray& device_array) OVERRIDE;
  virtual void OnDevicesEnumerationFailed(int request_id) OVERRIDE;
  virtual void OnDeviceOpened(
      int request_id,
      const std::string& label,
      const media_stream::StreamDeviceInfo& device_info) OVERRIDE;
  virtual void OnDeviceOpenFailed(int request_id) OVERRIDE;

  // content::RenderViewObserver OVERRIDE
  virtual void FrameWillClose(WebKit::WebFrame* frame) OVERRIDE;

 protected:
  // This function is virtual for test purposes. A test can override this to
  // test requesting local media streams. The function notifies WebKit that the
  // |request| have completed and generated the MediaStream |stream|.
  virtual void CompleteGetUserMediaRequest(
      const WebKit::WebMediaStreamDescriptor& stream,
      WebKit::WebUserMediaRequest* request);
  // This function is virtual for test purposes.
  // Returns the WebKit representation of a MediaStream given an URL.
  virtual WebKit::WebMediaStreamDescriptor GetMediaStream(const GURL& url);

 private:
  class VideoRendererWrapper : public webrtc::VideoRendererWrapperInterface {
   public:
    explicit VideoRendererWrapper(RTCVideoDecoder* decoder);
    virtual cricket::VideoRenderer* renderer() OVERRIDE {
      return rtc_video_decoder_.get();
    }

   protected:
    virtual ~VideoRendererWrapper();

   private:
    scoped_refptr<RTCVideoDecoder> rtc_video_decoder_;
  };

  // Structure for storing information about a WebKit request to create a
  // MediaStream.
  struct UserMediaRequestInfo {
    UserMediaRequestInfo() : frame_(NULL), request_() {}
    UserMediaRequestInfo(WebKit::WebFrame* frame,
                         const WebKit::WebUserMediaRequest& request)
        : frame_(frame), request_(request) {}
    WebKit::WebFrame* frame_;  // WebFrame that requested the MediaStream.
    WebKit::WebUserMediaRequest request_;
  };
  typedef std::map<int, UserMediaRequestInfo> MediaRequestMap;

  // We keep a list of the label and WebFrame of generated local media streams,
  // so that we can stop them when needed.
  typedef std::map<std::string, WebKit::WebFrame*> LocalNativeStreamMap;
  typedef talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface>
      LocalNativeStreamPtr;

  void InitializeWorkerThread(talk_base::Thread** thread,
                              base::WaitableEvent* event);

  void CreateIpcNetworkManagerOnWorkerThread(base::WaitableEvent* event);
  void DeleteIpcNetworkManager();

  bool EnsurePeerConnectionFactory();

  PeerConnectionHandlerBase* FindPeerConnectionByStream(
      const WebKit::WebMediaStreamDescriptor& stream);
  scoped_refptr<media::VideoDecoder> CreateLocalVideoDecoder(
      webrtc::MediaStreamInterface* stream,
      media::MessageLoopFactory* message_loop_factory);
  scoped_refptr<media::VideoDecoder> CreateRemoteVideoDecoder(
      webrtc::MediaStreamInterface* stream,
      const GURL& url,
      media::MessageLoopFactory* message_loop_factory);
  LocalNativeStreamPtr CreateNativeLocalMediaStream(
      const std::string& label,
      WebKit::WebFrame* frame,
      const WebKit::WebVector<WebKit::WebMediaStreamSource>& audio_sources,
      const WebKit::WebVector<WebKit::WebMediaStreamSource>& video_sources);

  scoped_ptr<MediaStreamDependencyFactory> dependency_factory_;

  // media_stream_dispatcher_ is a weak reference, owned by RenderView. It's
  // valid for the lifetime of RenderView.
  MediaStreamDispatcher* media_stream_dispatcher_;

  // p2p_socket_dispatcher_ is a weak reference, owned by RenderView. It's valid
  // for the lifetime of RenderView.
  content::P2PSocketDispatcher* p2p_socket_dispatcher_;

  // We own network_manager_, must be deleted on the worker thread.
  // The network manager uses |p2p_socket_dispatcher_|.
  content::IpcNetworkManager* network_manager_;
  scoped_ptr<content::IpcPacketSocketFactory> socket_factory_;
  scoped_refptr<VideoCaptureImplManager> vc_manager_;

  MediaRequestMap user_media_requests_;
  LocalNativeStreamMap local_media_streams_;

  // PeerConnection threads. signaling_thread_ is created from the
  // "current" chrome thread.
  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  base::Thread chrome_worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamImpl);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_IMPL_H_
