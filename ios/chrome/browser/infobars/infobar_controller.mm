// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_controller.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_protocol.h"

@interface InfoBarController () {
  base::scoped_nsobject<UIView<InfoBarViewProtocol>> _infoBarView;
}
@end

@implementation InfoBarController
@synthesize delegate = _delegate;

- (instancetype)initWithDelegate:(InfoBarViewDelegate*)delegate {
  self = [super init];
  if (self) {
    DCHECK(delegate);
    _delegate = delegate;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [_infoBarView removeFromSuperview];
  [super dealloc];
}

- (int)barHeight {
  return CGRectGetHeight([_infoBarView frame]);
}

- (void)layoutForDelegate:(infobars::InfoBarDelegate*)delegate
                    frame:(CGRect)bounds {
  DCHECK(!_infoBarView);
  _infoBarView = [self viewForDelegate:delegate frame:bounds];
}

- (base::scoped_nsobject<UIView<InfoBarViewProtocol>>)
    viewForDelegate:(infobars::InfoBarDelegate*)delegate
              frame:(CGRect)bounds {
  // Must be overriden in subclasses.
  NOTREACHED();
  return _infoBarView;
}

- (void)onHeightsRecalculated:(int)newHeight {
  [_infoBarView setVisibleHeight:newHeight];
}

- (UIView<InfoBarViewProtocol>*)view {
  return _infoBarView;
}

- (void)removeView {
  [_infoBarView removeFromSuperview];
}

- (void)detachView {
  [_infoBarView resetDelegate];
  _delegate = nullptr;
}

@end
