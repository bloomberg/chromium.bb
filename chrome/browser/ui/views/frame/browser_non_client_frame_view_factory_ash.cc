// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include "chrome/browser/ui/views/frame/browser_view.h"

#if defined(MOJO_SHELL_CLIENT)
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mus.h"
#include "content/public/common/mojo_shell_connection.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#endif

namespace chrome {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
#if defined(MOJO_SHELL_CLIENT)
  if (content::MojoShellConnection::Get()) {
    BrowserNonClientFrameViewMus* frame_view =
        new BrowserNonClientFrameViewMus(frame, browser_view);
    frame_view->Init();
    return frame_view;
  }
#endif

#if !defined(OS_CHROMEOS)
  if (browser_view->browser()->host_desktop_type() ==
      chrome::HOST_DESKTOP_TYPE_NATIVE) {
#if defined(OS_WIN)
    if (frame->ShouldUseNativeFrame())
      return new GlassBrowserFrameView(frame, browser_view);
#endif
    return new OpaqueBrowserFrameView(frame, browser_view);
  }
#endif

  BrowserNonClientFrameViewAsh* frame_view =
      new BrowserNonClientFrameViewAsh(frame, browser_view);
  frame_view->Init();
  return frame_view;
}

}  // namespace chrome
