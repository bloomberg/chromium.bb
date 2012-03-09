// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_center.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSourcesRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

namespace content {

MediaStreamCenter::MediaStreamCenter(
    WebKit::WebMediaStreamCenterClient* client)
    : client_(client) {
}

void MediaStreamCenter::queryMediaStreamSources(
    const WebKit::WebMediaStreamSourcesRequest& request) {
  WebKit::WebVector<WebKit::WebMediaStreamSource> audioSources, videoSources;
  request.didCompleteQuery(audioSources, videoSources);
}

void MediaStreamCenter::didEnableMediaStreamTrack(
    const WebKit::WebMediaStreamDescriptor& stream,
    const WebKit::WebMediaStreamComponent& component) {
}

void MediaStreamCenter::didDisableMediaStreamTrack(
    const WebKit::WebMediaStreamDescriptor& stream,
    const WebKit::WebMediaStreamComponent& component) {
}

void MediaStreamCenter::didStopLocalMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
}

void MediaStreamCenter::didConstructMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
}

}  // namespace content
