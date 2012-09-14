// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/command_line.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "ui/views/views_switches.h"

#if defined(USE_ASH)
#include "chrome/browser/ui/views/ash/browser_non_client_frame_view_ash.h"
#include "chrome/browser/ui/views/frame/app_non_client_frame_view_aura.h"
#endif

namespace chrome {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view) {

#if defined(USE_AURA)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
        views::switches::kDesktopAura))
    return new OpaqueBrowserFrameView(frame, browser_view);
#endif
#if defined(USE_ASH)
  // If this is an app window and it's maximized, use the special frame_view.
  if (browser_view->browser()->is_app() &&
      browser_view->browser()->app_type() != Browser::APP_TYPE_CHILD &&
      browser_view->IsMaximized())
    return new AppNonClientFrameViewAura(frame, browser_view);

  // Default is potentially translucent fancy frames.
  BrowserNonClientFrameViewAsh* frame_view =
      new BrowserNonClientFrameViewAsh(frame, browser_view);
  frame_view->Init();
#else
  OpaqueBrowserFrameView* frame_view =
      new OpaqueBrowserFrameView(frame, browser_view);
#endif
  return frame_view;
}

}  // namespace chrome
