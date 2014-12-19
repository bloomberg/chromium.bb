// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "base/basictypes.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "components/infobars/core/infobar.h"

@protocol InfoBarViewProtocol;
class InfoBarViewDelegate;

// InfoBar for iOS acts as a UIViewController for InfoBarView.
@interface InfoBarController : NSObject {
 @protected
  base::scoped_nsobject<UIView<InfoBarViewProtocol>> infoBarView_;
  __weak InfoBarViewDelegate* delegate_;
}

// Creates a view and lays out all the infobar elements in it. Will not add
// it as a subview yet. This method must be overriden in subclasses.
- (void)layoutForDelegate:(infobars::InfoBarDelegate*)delegate
                    frame:(CGRect)bounds;

// Designated initializer.
- (instancetype)initWithDelegate:(InfoBarViewDelegate*)delegate;

// Detaches view from its delegate.
// After this function is called, no user interaction can be handled.
- (void)detachView;

// Returns the actual height in pixels of this infobar instance.
- (int)barHeight;

// Adjusts visible portion of this infobar.
- (void)onHeightsRecalculated:(int)newHeight;

// Removes the view.
- (void)removeView;

// Accesses the view.
- (UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_H_
