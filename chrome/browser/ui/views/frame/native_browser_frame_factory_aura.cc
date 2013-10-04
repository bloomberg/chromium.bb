// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

#if defined(USE_ASH)
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/views/frame/browser_frame_ash.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura.h"
#endif

NativeBrowserFrame* NativeBrowserFrameFactory::Create(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
#if defined(OS_CHROMEOS)
  return new BrowserFrameAsh(browser_frame, browser_view);
#else
#if defined(USE_ASH)
  if (chrome::ShouldOpenAshOnStartup() ||
      browser_view->browser()->
          host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH) {
    return new BrowserFrameAsh(browser_frame, browser_view);
  }
#endif  // USE_ASH
  return new DesktopBrowserFrameAura(browser_frame, browser_view);
#endif
}

