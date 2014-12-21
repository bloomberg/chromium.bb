// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_protocol.h"
#include "ui/gfx/image/image.h"

@implementation InfoBarController

- (instancetype)initWithDelegate:(InfoBarViewDelegate*)delegate {
  self = [super init];
  if (self) {
    DCHECK(delegate);
    delegate_ = delegate;
  }
  return self;
}

- (void)dealloc {
  [infoBarView_ removeFromSuperview];
  [super dealloc];
}

- (int)barHeight {
  return CGRectGetHeight([infoBarView_ frame]);
}

- (void)layoutForDelegate:(infobars::InfoBarDelegate*)delegate
                    frame:(CGRect)bounds {
  // Must be overriden in subclasses.
  NOTREACHED();
}

- (void)onHeightsRecalculated:(int)newHeight {
  [infoBarView_ setVisibleHeight:newHeight];
}

- (UIView*)view {
  return infoBarView_;
}

- (void)removeView {
  [infoBarView_ removeFromSuperview];
}

- (void)detachView {
  [infoBarView_ resetDelegate];
  delegate_ = nullptr;
}

@end
