// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_VIEW_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_VIEW_BRIDGE_H_

#include <Cocoa/Cocoa.h>

#include "ui/gfx/native_widget_types.h"

class BrowserView;

// These utility functions serve to bridge between native Cocoa code (which
// often can't include BrowserView directly, because of conflicts between Cocoa
// and Views class names) and BrowserView.

namespace browser_view_bridge {

BrowserView* BrowserViewForNativeWindow(gfx::NativeWindow window);

bool BrowserViewHasAcceleratorForEvent(BrowserView* browser_view,
                                       NSEvent* event);

}  // namespace browser_view_bridge

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_VIEW_BRIDGE_H_
