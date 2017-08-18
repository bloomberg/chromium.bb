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

}  // namespace content
