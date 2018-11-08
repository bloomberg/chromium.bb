// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_controller.h"

#include <memory>

#include "base/logging.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_view_sizing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfoBarController () {
  UIView<InfoBarViewSizing>* _infoBarView;
}
@end

@implementation InfoBarController

@synthesize delegate = _delegate;
@synthesize infoBarDelegate = _infoBarDelegate;

#pragma mark - Public

- (instancetype)initWithInfoBarDelegate:
    (infobars::InfoBarDelegate*)infoBarDelegate {
  self = [super init];
  if (self) {
    _infoBarDelegate = infoBarDelegate;
    _infoBarView = [self infobarView];
    [_infoBarView setSizingDelegate:self];
  }
  return self;
}

- (void)dealloc {
  [_infoBarView removeFromSuperview];
}

- (int)barHeight {
  return CGRectGetHeight([_infoBarView frame]);
}

- (void)onHeightRecalculated:(int)newHeight {
  [_infoBarView setVisibleHeight:newHeight];
}

- (UIView<InfoBarViewSizing>*)view {
  return _infoBarView;
}

- (void)removeView {
  [_infoBarView removeFromSuperview];
}

- (void)detachView {
  [_infoBarView setSizingDelegate:nil];
  _delegate = nullptr;
  _infoBarDelegate = nullptr;
}

#pragma mark - Protected

- (UIView<InfoBarViewSizing>*)infobarView {
  NOTREACHED() << "Must be overriden in subclasses.";
  return _infoBarView;
}

- (BOOL)shouldIgnoreUserInteraction {
  // Ignore user interaction if view is already detached or is about to.
  return !_delegate || !_delegate->IsOwned() || !_infoBarDelegate;
}

#pragma mark - InfoBarViewDelegate

- (void)didSetInfoBarTargetHeight:(CGFloat)height {
  if (!_delegate)
    return;

  _delegate->SetInfoBarTargetHeight(height);
}

@end
