// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_frame_aura.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

#if defined(USE_ASH)
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/ash_init.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura.h"
#endif

NativeBrowserFrame* NativeBrowserFrameFactory::Create(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
#if !defined(OS_CHROMEOS)
  if (
#if defined(USE_ASH)
      !chrome::ShouldOpenAshOnStartup() &&
#endif
      browser_view->browser()->
          host_desktop_type() == chrome::HOST_DESKTOP_TYPE_NATIVE)
    return new DesktopBrowserFrameAura(browser_frame, browser_view);
#endif
  return new BrowserFrameAura(browser_frame, browser_view);
}

