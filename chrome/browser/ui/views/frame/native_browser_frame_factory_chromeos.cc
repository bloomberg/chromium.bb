// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

#include "chrome/browser/mojo_runner_util.h"
#include "chrome/browser/ui/views/frame/browser_frame_ash.h"

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
  return new BrowserFrameAsh(browser_frame, browser_view);
}
