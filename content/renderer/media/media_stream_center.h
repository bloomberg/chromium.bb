// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamCenter.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"

namespace WebKit {
class WebMediaStreamCenterClient;
}

namespace content {
class MediaStreamDependencyFactory;

class CONTENT_EXPORT MediaStreamCenter
    : NON_EXPORTED_BASE(public WebKit::WebMediaStreamCenter) {
 public:
  MediaStreamCenter(WebKit::WebMediaStreamCenterClient* client,
                    MediaStreamDependencyFactory* factory);

  virtual void queryMediaStreamSources(
      const WebKit::WebMediaStreamSourcesRequest& request) OVERRIDE;

  virtual void didEnableMediaStreamTrack(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaStreamTrack& component) OVERRIDE;

  virtual void didDisableMediaStreamTrack(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaStreamTrack& component) OVERRIDE;

  virtual void didStopLocalMediaStream(
      const WebKit::WebMediaStream& stream) OVERRIDE;

  virtual void didCreateMediaStream(
      WebKit::WebMediaStream& stream) OVERRIDE;

 private:
  // |rtc_factory_| is a weak pointer and is owned by the RenderThreadImpl.
  // It is valid as long as  RenderThreadImpl exist.
  MediaStreamDependencyFactory* rtc_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamCenter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_
