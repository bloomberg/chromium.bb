// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reader_mode/reader_mode_view.h"

#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"
#import "ios/chrome/browser/ui/material_components/activity_indicator.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kCloseButtonSize = 40;
const CGFloat kCloseButtonMargin = 10;
const CGFloat kMinWidth = kCloseButtonSize + kCloseButtonMargin * 2;
const CGFloat kMinHeight = kMinWidth;
}  // namespace

@interface ReaderModeView ()<MDCActivityIndicatorDelegate> {
  std::unique_ptr<dom_distiller::DistillerViewer> _viewer;
}
@property(nonatomic, strong) MDCActivityIndicator* activityIndicator;
@property(nonatomic, copy) ProceduralBlock animateOutCompletionBlock;
@property(nonatomic, strong) UIButton* closeButton;

@end

@implementation ReaderModeView
@synthesize activityIndicator = _activityIndicator;
@synthesize animateOutCompletionBlock = _animateOutCompletionBlock;
@synthesize closeButton = _closeButton;
@synthesize delegate = _delegate;

- (instancetype)initWithFrame:(CGRect)frame
                     delegate:(id<ReaderModeViewDelegate>)delegate {
  self = [super initWithFrame:frame];
  if (self) {
    _delegate = delegate;

    self.backgroundColor = [UIColor whiteColor];

    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    _closeButton = [UIButton buttonWithType:UIButtonTypeRoundedRect];
    [_closeButton addTarget:self
                     action:@selector(close)
           forControlEvents:UIControlEventTouchUpInside];
    [_closeButton setTitle:@"X" forState:UIControlStateNormal];
    _closeButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin |
                                    UIViewAutoresizingFlexibleTopMargin;

    _activityIndicator =
        [[MDCActivityIndicator alloc] initWithFrame:CGRectMake(0, 0, 24, 24)];
    _activityIndicator.delegate = self;
    _activityIndicator.cycleColors = ActivityIndicatorBrandedCycleColors();
    _activityIndicator.autoresizingMask =
        UIViewAutoresizingFlexibleLeftMargin |
        UIViewAutoresizingFlexibleTopMargin |
        UIViewAutoresizingFlexibleRightMargin |
        UIViewAutoresizingFlexibleBottomMargin;

    [self addSubview:_closeButton];
    [self addSubview:_activityIndicator];
    [self sizeToFit];
  }
  return self;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)start {
  [self.activityIndicator startAnimating];
}

- (void)stopWaitingWithCompletion:(ProceduralBlock)completion {
  if (self.activityIndicator.isAnimating) {
    self.animateOutCompletionBlock = completion;
    [self.activityIndicator stopAnimating];
  } else if (completion) {
    completion();
  }
}

- (void)close {
  [self.delegate exitReaderMode];
}

- (void)assignDistillerViewer:
    (std::unique_ptr<dom_distiller::DistillerViewer>)viewer {
  _viewer.reset(viewer.release());
}

- (void)layoutSubviews {
  [super layoutSubviews];

  self.closeButton.frame = CGRectMake(
      self.bounds.size.width - kCloseButtonSize - kCloseButtonMargin,
      self.bounds.size.height - kCloseButtonSize - kCloseButtonMargin,
      kCloseButtonSize, kCloseButtonSize);
  self.activityIndicator.center = CGPointMake(CGRectGetWidth(self.bounds) / 2,
                                              CGRectGetHeight(self.bounds) / 2);
}

- (CGSize)sizeThatFits:(CGSize)size {
  CGSize newSize = [super sizeThatFits:size];
  if (newSize.width < kMinWidth)
    newSize.width = kMinWidth;
  if (newSize.height < kMinHeight)
    newSize.height = kMinHeight;

  return newSize;
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
