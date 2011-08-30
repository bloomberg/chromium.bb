// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include "content/common/view_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebScreenInfoFactory.h"

namespace {

// Returns the "visible window rect" for |window|, defined roughly as "what the
// user thinks of as the window".  See comments below regarding the difference
// between this and the true window rect when the window is maximized.
gfx::Rect GetVisibleWindowRect(HWND window) {
  // We get null |window_id|s passed into the two functions below; see bug 9060.
  // Probably this should be removed and we should prevent this case.
  if (!IsWindow(window))
    return gfx::Rect();

  RECT window_rect = {0};
  GetWindowRect(window, &window_rect);
  gfx::Rect rect(window_rect);

  // Maximized windows are outdented from the work area by the frame thickness
  // even though this "frame" is not painted.  This confuses code (and people)
  // that think of a maximized window as corresponding exactly to the work area.
  // Correct for this by subtracting the frame thickness back off.
  if (IsZoomed(window)) {
    rect.Inset(GetSystemMetrics(SM_CXSIZEFRAME),
               GetSystemMetrics(SM_CYSIZEFRAME));
  }
  return rect;
}

}

// TODO(shess): Provide a mapping from reply_msg->routing_id() to HWND
// so that we can eliminate the NativeViewId parameter.

void RenderMessageFilter::OnGetWindowRect(gfx::NativeViewId window_id,
                                          gfx::Rect* rect) {
  *rect = GetVisibleWindowRect(gfx::NativeViewFromId(window_id));
}

void RenderMessageFilter::OnGetRootWindowRect(gfx::NativeViewId window_id,
                                              gfx::Rect* rect) {
  *rect = GetVisibleWindowRect(GetAncestor(gfx::NativeViewFromId(window_id),
                               GA_ROOT));
}

void RenderMessageFilter::OnGetScreenInfo(gfx::NativeViewId view,
                                          WebKit::WebScreenInfo* results) {
  *results =
      WebKit::WebScreenInfoFactory::screenInfo(gfx::NativeViewFromId(view));
}
