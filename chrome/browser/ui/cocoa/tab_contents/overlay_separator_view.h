// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_OVERLAY_SEPARATOR_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_OVERLAY_SEPARATOR_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/background_gradient_view.h"

// A view used to draw a separator above Instant overlay view.
@interface OverlayTopSeparatorView : BackgroundGradientView {
}

+ (CGFloat)preferredHeight;

@end

// A view used to draw a drop shadow beneath the Instant overlay view.
@interface OverlayBottomSeparatorView : NSView {
}

+ (CGFloat)preferredHeight;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_OVERLAY_SEPARATOR_VIEW_H_
