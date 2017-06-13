// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_
#define CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

// RemoteMediaStreamImpl serves as a container and glue between remote webrtc
// MediaStreams and WebKit MediaStreams. For each remote MediaStream received
// on a PeerConnection a RemoteMediaStreamImpl instance is created and
// owned by RtcPeerConnection.
class CONTENT_EXPORT RemoteMediaStreamImpl {
 public:
  // A map between track IDs and references to track adapters.
  using AdapterRefMap =
      std::map<std::string,
               std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>>;

  RemoteMediaStreamImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const scoped_refptr<WebRtcMediaStreamTrackAdapterMap>& track_adapter_map,
      webrtc::MediaStreamInterface* webrtc_stream);
  ~RemoteMediaStreamImpl();

  const blink::WebMediaStream& webkit_stream() { return webkit_stream_; }
  const scoped_refptr<webrtc::MediaStreamInterface>& webrtc_stream();

 private:
  class Observer;

  void InitializeOnMainThread(const std::string& label,
                              AdapterRefMap track_adapter_refs,
                              size_t audio_track_count,
                              size_t video_track_count);

  void OnChanged(AdapterRefMap new_adapter_refs);

  const scoped_refptr<base::SingleThreadTaskRunner> signaling_thread_;
  scoped_refptr<Observer> observer_;

  // The map and owner of all track adapters for the associated peer connection.
  // When a track is added or removed from this stream, the map provides us with
  // a reference to the corresponding track adapter, creating a new one if
  // necessary.
  scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;
  blink::WebMediaStream webkit_stream_;
  // A map between track IDs and references to track adapters for any tracks
  // that belong to this stream. Keeping an adapter reference alive ensures the
  // adapter is not disposed by the |track_adapter_map_|, as is necessary for as
  // long as the webrtc layer track is in use by the webrtc layer stream.
  AdapterRefMap adapter_refs_;

  base::WeakPtrFactory<RemoteMediaStreamImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteMediaStreamImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_
