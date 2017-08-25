// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/remote_media_stream_impl.h"

namespace content {

// Called on the signaling thread.
RemoteMediaStreamImpl::RemoteMediaStreamImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    const scoped_refptr<WebRtcMediaStreamTrackAdapterMap>& track_adapter_map,
    webrtc::MediaStreamInterface* webrtc_stream)
    : adapter_(
          WebRtcMediaStreamAdapter::CreateRemoteStreamAdapter(main_thread,
                                                              track_adapter_map,
                                                              webrtc_stream)) {}

// Called on the main thread.
RemoteMediaStreamImpl::~RemoteMediaStreamImpl() {
}

}  // namespace content
