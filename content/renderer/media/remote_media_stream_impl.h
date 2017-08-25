// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_
#define CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

// TODO(hbos): Remove this class and file, it has been replaced by
// |RemoteWebRtcMediaStreamAdapter|. What remains is a wrapper using the new
// class. https://crbug.com/705901

// RemoteMediaStreamImpl serves as a container and glue between remote webrtc
// MediaStreams and WebKit MediaStreams. For each remote MediaStream received
// on a PeerConnection a RemoteMediaStreamImpl instance is created and
// owned by RtcPeerConnection.
class CONTENT_EXPORT RemoteMediaStreamImpl {
 public:
  RemoteMediaStreamImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const scoped_refptr<WebRtcMediaStreamTrackAdapterMap>& track_adapter_map,
      webrtc::MediaStreamInterface* webrtc_stream);
  ~RemoteMediaStreamImpl();

  const blink::WebMediaStream& webkit_stream() {
    return adapter_->web_stream();
  }

  const scoped_refptr<webrtc::MediaStreamInterface>& webrtc_stream() {
    return adapter_->webrtc_stream();
  }

 private:
  std::unique_ptr<WebRtcMediaStreamAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(RemoteMediaStreamImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_
