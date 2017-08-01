// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mus.h"
#include "ui/aura/env.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#endif

namespace chrome {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
#if defined(USE_AURA)
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS) {
    BrowserNonClientFrameViewMus* frame_view =
        new BrowserNonClientFrameViewMus(frame, browser_view);
    frame_view->Init();
    return frame_view;
  }
#endif
#if defined(OS_WIN)
  if (frame->ShouldUseNativeFrame())
    return new GlassBrowserFrameView(frame, browser_view);
#endif
  return new OpaqueBrowserFrameView(frame, browser_view);
}

}  // namespace chrome
