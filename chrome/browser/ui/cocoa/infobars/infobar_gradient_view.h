// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_
#pragma once

#include "chrome/browser/tab_contents/infobar_delegate.h"
#import "chrome/browser/ui/cocoa/vertical_gradient_view.h"

#import <Cocoa/Cocoa.h>

@class InfobarTipDrawingModel;

// A custom view that draws the background gradient for an infobar.
@interface InfoBarGradientView : VerticalGradientView {
 @private
  InfobarTipDrawingModel* drawingModel_;  // weak
}

@property(nonatomic, assign) InfobarTipDrawingModel* drawingModel;

// Sets the infobar type. This will change the view's gradient.
- (void)setInfobarType:(InfoBarDelegate::Type)infobarType;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_GRADIENT_VIEW_H_
