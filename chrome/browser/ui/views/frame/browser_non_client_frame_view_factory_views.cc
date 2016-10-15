// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mus.h"
#include "services/service_manager/runner/common/client_util.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#endif

namespace chrome {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
#if defined(USE_AURA)
  if (service_manager::ServiceManagerIsRemote()) {
    BrowserNonClientFrameViewMus* frame_view =
        new BrowserNonClientFrameViewMus(frame, browser_view);
    frame_view->Init();
    return frame_view;
  }
#endif
#if defined(USE_ASH)
  BrowserNonClientFrameViewAsh* frame_view =
      new BrowserNonClientFrameViewAsh(frame, browser_view);
  frame_view->Init();
  return frame_view;
#else
#if defined(OS_WIN)
  if (frame->ShouldUseNativeFrame())
    return new GlassBrowserFrameView(frame, browser_view);
#endif
  return new OpaqueBrowserFrameView(frame, browser_view);
#endif  // USE_ASH
}

}  // namespace chrome
