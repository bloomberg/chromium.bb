// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_

#import "chrome/browser/ui/cocoa/vertical_gradient_view.h"
#include "third_party/skia/include/core/SkColor.h"

#import <Cocoa/Cocoa.h>

// A custom view that draws the background gradient for an infobar.
@interface InfoBarGradientView : VerticalGradientView

// Sets the infobar background color.
- (void)setInfobarBackgroundColor:(SkColor)color;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_
