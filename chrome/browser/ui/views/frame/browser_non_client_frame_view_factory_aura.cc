// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/command_line.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

#if defined(USE_ASH)
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#endif

namespace chrome {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view) {
#if defined(OS_WIN)
  if (browser_view->browser()->
          host_desktop_type() == chrome::HOST_DESKTOP_TYPE_NATIVE) {
    if (frame->ShouldUseNativeFrame())
      return new GlassBrowserFrameView(frame, browser_view);
    return new OpaqueBrowserFrameView(frame, browser_view);
  }
#endif
#if defined(USE_ASH)
  BrowserNonClientFrameViewAsh* frame_view =
      new BrowserNonClientFrameViewAsh(frame, browser_view);
  frame_view->Init();
  return frame_view;
#else
  return new OpaqueBrowserFrameView(frame, browser_view);
#endif
}

}  // namespace chrome
