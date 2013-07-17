// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace WebKit {
class WebMediaStreamCenterClient;
}

namespace content {
class MediaStreamDependencyFactory;

class CONTENT_EXPORT MediaStreamCenter
    : NON_EXPORTED_BASE(public WebKit::WebMediaStreamCenter),
      public RenderProcessObserver {
 public:
  MediaStreamCenter(WebKit::WebMediaStreamCenterClient* client,
                    MediaStreamDependencyFactory* factory);
  virtual ~MediaStreamCenter();

  virtual bool getMediaStreamTrackSources(
      const WebKit::WebMediaStreamTrackSourcesRequest& request);

  virtual void didEnableMediaStreamTrack(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaStreamTrack& component);

  virtual void didDisableMediaStreamTrack(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaStreamTrack& component);

  virtual void didStopLocalMediaStream(
      const WebKit::WebMediaStream& stream);

  virtual void didCreateMediaStream(
      WebKit::WebMediaStream& stream);

  virtual bool didAddMediaStreamTrack(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaStreamTrack& track);

  virtual bool didRemoveMediaStreamTrack(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaStreamTrack& track);

 private:
  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnGetSourcesComplete(int request_id,
                            const content::StreamDeviceInfoArray& devices);

  // |rtc_factory_| is a weak pointer and is owned by the RenderThreadImpl.
  // It is valid as long as  RenderThreadImpl exist.
  MediaStreamDependencyFactory* rtc_factory_;

  // A strictly increasing id that's used to label incoming GetSources()
  // requests.
  int next_request_id_;

  typedef std::map<int, WebKit::WebMediaStreamTrackSourcesRequest> RequestMap;
  // Maps request ids to request objects.
  RequestMap requests_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamCenter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_
