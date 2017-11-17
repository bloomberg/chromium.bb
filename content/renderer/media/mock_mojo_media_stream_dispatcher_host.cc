// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_mojo_media_stream_dispatcher_host.h"

namespace content {

MockMojoMediaStreamDispatcherHost::MockMojoMediaStreamDispatcherHost() {}

MockMojoMediaStreamDispatcherHost::~MockMojoMediaStreamDispatcherHost() {}

mojom::MediaStreamDispatcherHostPtr
MockMojoMediaStreamDispatcherHost::CreateInterfacePtrAndBind() {
  mojom::MediaStreamDispatcherHostPtr dispatcher_host;
  bindings_.AddBinding(this, mojo::MakeRequest(&dispatcher_host));
  return dispatcher_host;
}

void MockMojoMediaStreamDispatcherHost::OpenDevice(
    int32_t render_frame_id,
    int32_t request_id,
    const std::string& device_id,
    MediaStreamType type,
    OpenDeviceCallback callback) {
  std::move(callback).Run(true /* success */, std::string(),
                          MediaStreamDevice());
}

}  // namespace content
