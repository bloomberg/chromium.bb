// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

#include "chrome/browser/ui/views/frame/native_browser_frame.h"

namespace {

NativeBrowserFrameFactory* factory = NULL;

}

// static
NativeBrowserFrame* NativeBrowserFrameFactory::CreateNativeBrowserFrame(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  if (!factory)
    factory = new NativeBrowserFrameFactory;
  return factory->Create(browser_frame, browser_view);
}

// static
void NativeBrowserFrameFactory::Set(NativeBrowserFrameFactory* new_factory) {
  delete factory;
  factory = new_factory;
}

#if !defined(OS_WIN)
// static
chrome::HostDesktopType NativeBrowserFrameFactory::AdjustHostDesktopType(
    chrome::HostDesktopType desktop_type) {
  return desktop_type;
}
#endif  // !defined(OS_WIN)

#if !defined(USE_ASH) || defined(OS_CHROMEOS)
// static
bool NativeBrowserFrameFactory::ShouldCreateForAshDesktop(
    BrowserView* browser_view) {
  NOTREACHED();
  return false;
}
#endif  // !defined(OS_WIN) || !defined(USE_ASH)
