// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include "content/common/view_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebScreenInfoFactory.h"

using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

// TODO(shess): Provide a mapping from reply_msg->routing_id() to HWND
// so that we can eliminate the NativeViewId parameter.

void RenderMessageFilter::OnGetWindowRect(gfx::NativeViewId window_id,
                                          gfx::Rect* rect) {
  HWND window = gfx::NativeViewFromId(window_id);
  RECT window_rect = {0};
  GetWindowRect(window, &window_rect);
  *rect = window_rect;
}

void RenderMessageFilter::OnGetRootWindowRect(gfx::NativeViewId window_id,
                                              gfx::Rect* rect) {
  HWND window = gfx::NativeViewFromId(window_id);
  RECT window_rect = {0};
  HWND root_window = ::GetAncestor(window, GA_ROOT);
  GetWindowRect(root_window, &window_rect);
  *rect = window_rect;
}

void RenderMessageFilter::OnGetScreenInfo(gfx::NativeViewId view,
                                          WebKit::WebScreenInfo* results) {
  *results = WebScreenInfoFactory::screenInfo(gfx::NativeViewFromId(view));
}
