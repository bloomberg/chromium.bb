// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

class PeerConnectionDependencyFactory;

// WebRtcMediaStreamAdapter is an adapter between a blink::WebMediaStream
// object and a webrtc MediaStreams that is currently sent on a PeerConnection.
// The responsibility of the class is to create and own a representation of a
// webrtc MediaStream that can be added and removed from a RTCPeerConnection.
// An instance of WebRtcMediaStreamAdapter is created when a MediaStream is
// added to an RTCPeerConnection object
// Instances of this class is owned by the RTCPeerConnectionHandler object that
// created it.
class CONTENT_EXPORT WebRtcMediaStreamAdapter : public MediaStreamObserver {
 public:
  WebRtcMediaStreamAdapter(
      PeerConnectionDependencyFactory* factory,
      scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
      const blink::WebMediaStream& web_stream);
  ~WebRtcMediaStreamAdapter() override;

  bool IsEqual(const blink::WebMediaStream& web_stream) const {
    return web_stream_.GetExtraData() == web_stream.GetExtraData();
  }

  webrtc::MediaStreamInterface* webrtc_media_stream() const {
    return webrtc_media_stream_.get();
  }

  const blink::WebMediaStream& web_stream() const { return web_stream_; }

 protected:
  // MediaStreamObserver implementation. Also used as a helper functions when
  // adding/removing all tracks in the constructor/destructor.
  void TrackAdded(const blink::WebMediaStreamTrack& web_track) override;
  void TrackRemoved(const blink::WebMediaStreamTrack& web_track) override;

 private:
  // Pointer to a PeerConnectionDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  PeerConnectionDependencyFactory* const factory_;
  // The map and owner of all track adapters for the associated peer connection.
  // When a track is added or removed from this stream, the map provides us with
  // a reference to the corresponding track adapter, creating a new one if
  // necessary.
  scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;

  const blink::WebMediaStream web_stream_;
  scoped_refptr<webrtc::MediaStreamInterface> webrtc_media_stream_;
  // A map between track IDs and references to track adapters for any tracks
  // that belong to this stream. Keeping an adapter reference alive ensures the
  // adapter is not disposed by the |track_adapter_map_|, as is necessary for as
  // long as the webrtc layer track is in use by the webrtc layer stream.
  std::map<std::string,
           std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>>
      adapter_refs_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcMediaStreamAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_
