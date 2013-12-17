// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_delegate.h"

#include <stddef.h>

namespace content {

bool RenderFrameHostDelegate::OnMessageReceived(
    RenderFrameHost* render_view_host,
    const IPC::Message& message) {
  return false;
}

WebContents* RenderFrameHostDelegate::GetAsWebContents() {
  return NULL;
}

}  // namespace content
