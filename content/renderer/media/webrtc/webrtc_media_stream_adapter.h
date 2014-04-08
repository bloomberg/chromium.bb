// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

class MediaStreamDependencyFactory;

// WebRtcMediaStreamAdapter is an adapter between a blink::WebMediaStream
// object and a webrtc MediaStreams that is currently sent on a PeerConnection.
// The responsibility of the class is to create and own a representation of a
// webrtc MediaStream that can be added and removed from a RTCPeerConnection.
// An instance of WebRtcMediaStreamAdapter is created when a MediaStream is
// added to an RTCPeerConnection object
// Instances of this class is owned by the RTCPeerConnectionHandler object that
// created it.
class CONTENT_EXPORT WebRtcMediaStreamAdapter {
 public:
  WebRtcMediaStreamAdapter(const blink::WebMediaStream& web_stream,
                           MediaStreamDependencyFactory* factory);
  virtual ~WebRtcMediaStreamAdapter();

  bool IsEqual(const blink::WebMediaStream& web_stream) {
    return web_stream_.extraData() == web_stream.extraData();
  }

  webrtc::MediaStreamInterface* webrtc_media_stream() {
    return webrtc_media_stream_.get();
  }

 private:
  void CreateAudioTracks();
  void CreateVideoTracks();

  blink::WebMediaStream web_stream_;

  // Pointer to a MediaStreamDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  MediaStreamDependencyFactory* factory_;

  scoped_refptr<webrtc::MediaStreamInterface> webrtc_media_stream_;

  DISALLOW_COPY_AND_ASSIGN (WebRtcMediaStreamAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_
