// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_

#include "chrome/browser/infobars/infobar_delegate.h"
#import "chrome/browser/ui/cocoa/vertical_gradient_view.h"

#import <Cocoa/Cocoa.h>

// A custom view that draws the background gradient for an infobar.
@interface InfoBarGradientView : VerticalGradientView {
 @private
  NSPoint tipApex_;
}

// The point, in window coordinates, at which the infobar tip is the highest and
// pointing at the omnibox decoration.
@property(assign, nonatomic) NSPoint tipApex;

// Sets the infobar type. This will change the view's gradient.
- (void)setInfobarType:(InfoBarDelegate::Type)infobarType;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_
