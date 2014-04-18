// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_

#import "chrome/browser/ui/cocoa/vertical_gradient_view.h"
#include "components/infobars/core/infobar_delegate.h"

#import <Cocoa/Cocoa.h>

// A custom view that draws the background gradient for an infobar.
@interface InfoBarGradientView : VerticalGradientView {
 @private
  CGFloat arrowHeight_;
  CGFloat arrowHalfWidth_;
  CGFloat arrowX_;
  BOOL hasTip_;
}

@property(assign, nonatomic) CGFloat arrowHeight;
@property(assign, nonatomic) CGFloat arrowHalfWidth;
@property(assign, nonatomic) CGFloat arrowX;
@property(assign, nonatomic) BOOL hasTip;

// Sets the infobar type. This will change the view's gradient.
- (void)setInfobarType:(infobars::InfoBarDelegate::Type)infobarType;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_
