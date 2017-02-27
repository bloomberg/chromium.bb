// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include "content/public/browser/web_contents.h"
#include "ui/base/cocoa/base_view.h"

namespace headless {

void HeadlessBrowserImpl::PlatformInitialize() {}

void HeadlessBrowserImpl::PlatformCreateWindow() {}

void HeadlessBrowserImpl::PlatformInitializeWebContents(
    const gfx::Size& initial_size,
    content::WebContents* web_contents) {
  NSView* web_view = web_contents->GetNativeView();
  [web_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

  NSRect frame = NSMakeRect(0, 0, initial_size.width(), initial_size.height());
  [web_view setFrame:frame];
  return;
}

}  // namespace headless
