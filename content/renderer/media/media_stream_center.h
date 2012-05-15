// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamCenter.h"

namespace WebKit {
class WebMediaStreamCenterClient;
}

namespace content {

class CONTENT_EXPORT MediaStreamCenter
    : NON_EXPORTED_BASE(public WebKit::WebMediaStreamCenter) {
 public:
  explicit MediaStreamCenter(WebKit::WebMediaStreamCenterClient* client);

  virtual void queryMediaStreamSources(
      const WebKit::WebMediaStreamSourcesRequest& request) OVERRIDE;

  virtual void didEnableMediaStreamTrack(
      const WebKit::WebMediaStreamDescriptor& stream,
      const WebKit::WebMediaStreamComponent& component) OVERRIDE;

  virtual void didDisableMediaStreamTrack(
      const WebKit::WebMediaStreamDescriptor& stream,
      const WebKit::WebMediaStreamComponent& component) OVERRIDE;

  virtual void didStopLocalMediaStream(
      const WebKit::WebMediaStreamDescriptor& stream) OVERRIDE;

  virtual void didCreateMediaStream(
      WebKit::WebMediaStreamDescriptor& stream) OVERRIDE;

  virtual WebKit::WebString constructSDP(
      const WebKit::WebICECandidateDescriptor& candidate) OVERRIDE;

  virtual WebKit::WebString constructSDP(
      const WebKit::WebSessionDescriptionDescriptor& description) OVERRIDE;

 private:
  // Weak pointer, owned by WebKit.
  WebKit::WebMediaStreamCenterClient* client_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamCenter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CENTER_H_
