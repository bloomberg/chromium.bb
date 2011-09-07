// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

// TODO(shess): Provide a mapping from reply_msg->routing_id() to HWND
// so that we can eliminate the NativeViewId parameter.

void RenderMessageFilter::OnGetWindowRect(gfx::NativeViewId window_id,
                                          gfx::Rect* rect) {
  // TODO(beng):
  NOTIMPLEMENTED();
  *rect = gfx::Rect();
}

void RenderMessageFilter::OnGetRootWindowRect(gfx::NativeViewId window_id,
                                              gfx::Rect* rect) {
  // TODO(beng):
  NOTIMPLEMENTED();
  *rect = gfx::Rect();
}

void RenderMessageFilter::OnGetScreenInfo(gfx::NativeViewId view,
                                          WebKit::WebScreenInfo* results) {
  // TODO(beng):
  NOTIMPLEMENTED();
}
