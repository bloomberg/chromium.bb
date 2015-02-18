// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include "ipc/message_filter.h"

namespace chromecast {
namespace shell {

void PlatformAddRendererNativeBindings(blink::WebLocalFrame* frame) {
}

std::vector<scoped_refptr<IPC::MessageFilter>>
CastContentRendererClient::PlatformGetRendererMessageFilters() {
  return std::vector<scoped_refptr<IPC::MessageFilter>>();
}

}  // namespace shell
}  // namespace chromecast
