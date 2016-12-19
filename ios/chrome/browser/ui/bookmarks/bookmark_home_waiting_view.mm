// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_waiting_view.h"

#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/material_components/activity_indicator.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"

@interface BookmarkHomeWaitingView ()<MDCActivityIndicatorDelegate> {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkHomeWaitingView;
}
@property(nonatomic, retain) MDCActivityIndicator* activityIndicator;
@property(nonatomic, copy) ProceduralBlock animateOutCompletionBlock;
@end

@implementation BookmarkHomeWaitingView

@synthesize activityIndicator = _activityIndicator;
@synthesize animateOutCompletionBlock = _animateOutCompletionBlock;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkHomeWaitingView.Init(
        self, [BookmarkHomeWaitingView class]);
    self.backgroundColor = bookmark_utils_ios::mainBackgroundColor();
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return self;
}

- (void)startWaiting {
  dispatch_time_t delayForIndicatorAppearance =
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC));
  dispatch_after(delayForIndicatorAppearance, dispatch_get_main_queue(), ^{
    base::scoped_nsobject<MDCActivityIndicator> activityIndicator(
        [[MDCActivityIndicator alloc] initWithFrame:CGRectMake(0, 0, 24, 24)]);
    self.activityIndicator = activityIndicator;
    self.activityIndicator.delegate = self;
    self.activityIndicator.autoresizingMask =
        UIViewAutoresizingFlexibleLeadingMargin() |
        UIViewAutoresizingFlexibleTopMargin |
        UIViewAutoresizingFlexibleTrailingMargin() |
        UIViewAutoresizingFlexibleBottomMargin;
    self.activityIndicator.center = CGPointMake(
        CGRectGetWidth(self.bounds) / 2, CGRectGetHeight(self.bounds) / 2);
    self.activityIndicator.cycleColors = ActivityIndicatorBrandedCycleColors();
    [self addSubview:self.activityIndicator];
    [self.activityIndicator startAnimating];
  });
}

- (void)stopWaitingWithCompletion:(ProceduralBlock)completion {
  if (self.activityIndicator) {
    self.animateOutCompletionBlock = completion;
    [self.activityIndicator stopAnimating];
  } else if (completion) {
    completion();
  }
}

#pragma mark - MDCActivityIndicatorDelegate

- (void)activityIndicatorAnimationDidFinish:
    (MDCActivityIndicator*)activityIndicator {
  [self.activityIndicator removeFromSuperview];
  self.activityIndicator = nil;
  if (self.animateOutCompletionBlock)
    self.animateOutCompletionBlock();
  self.animateOutCompletionBlock = nil;
}

@end
