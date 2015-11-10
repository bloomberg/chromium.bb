// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

#include "ash/shell.h"
#include "chrome/browser/mojo_runner_util.h"
#include "chrome/browser/ui/views/frame/browser_frame_ashwin.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura.h"

#if defined(MOJO_RUNNER_CLIENT)
#include "chrome/browser/ui/views/frame/browser_frame_mus.h"
#endif

NativeBrowserFrame* NativeBrowserFrameFactory::Create(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
#if defined(MOJO_RUNNER_CLIENT)
  if (IsRunningInMojoRunner())
    return new BrowserFrameMus(browser_frame, browser_view);
#endif

  if (ShouldCreateForAshDesktop(browser_view))
    return new BrowserFrameAshWin(browser_frame, browser_view);

  return new DesktopBrowserFrameAura(browser_frame, browser_view);
}

// static
chrome::HostDesktopType NativeBrowserFrameFactory::AdjustHostDesktopType(
    chrome::HostDesktopType desktop_type) {
  if (ash::Shell::HasInstance())
    return chrome::HOST_DESKTOP_TYPE_ASH;

  return desktop_type;
}
