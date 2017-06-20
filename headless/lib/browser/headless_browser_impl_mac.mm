// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include "content/public/browser/web_contents.h"
#include "ui/base/cocoa/base_view.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace headless {

void HeadlessBrowserImpl::PlatformInitialize() {}

void HeadlessBrowserImpl::PlatformStart() {}

void HeadlessBrowserImpl::PlatformInitializeWebContents(
    HeadlessWebContentsImpl* web_contents) {
  NSView* web_view = web_contents->web_contents()->GetNativeView();
  [web_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
}

void HeadlessBrowserImpl::PlatformSetWebContentsBounds(
    HeadlessWebContentsImpl* web_contents,
    const gfx::Rect& bounds) {
  NSView* web_view = web_contents->web_contents()->GetNativeView();
  NSRect frame = gfx::ScreenRectToNSRect(bounds);
  [web_view setFrame:frame];
}

}  // namespace headless
