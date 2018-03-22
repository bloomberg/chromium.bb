// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/named_guide.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NamedGuide ()

// The constraints used to connect the guide to |constrainedView| or
// |constrainedFrame|.
@property(nonatomic, strong) NSArray* constraints;
// A dummy view that is used to support |constrainedFrame|.
@property(nonatomic, strong) UIView* constrainedFrameView;

// Updates |constraints| to constrain the guide to |view|.
- (void)updateConstraintsWithView:(UIView*)view;

// Updates |constrainedFrameView| according to |constrainedFrame| and
// |autoresizingMask|.  This function will lazily instantiate the view if
// necessary and set up constraints so that this layout guide follows the view.
- (void)updateConstrainedFrameView;

@end

@implementation NamedGuide
@synthesize name = _name;
@synthesize constrainedView = _constrainedView;
@synthesize constrainedFrame = _constrainedFrame;
@synthesize autoresizingMask = _autoresizingMask;
@synthesize constraints = _constraints;
@synthesize constrainedFrameView = _constrainedFrameView;

- (instancetype)initWithName:(GuideName*)name {
  if (self = [super init]) {
    _name = name;
    _constrainedFrame = CGRectNull;
  }
  return self;
}

- (void)dealloc {
  _constrainedView = nil;
  _constrainedFrame = CGRectNull;
  if (_constraints.count)
    [NSLayoutConstraint deactivateConstraints:_constraints];
}

#pragma mark - Accessors

- (void)setConstrainedView:(UIView*)constrainedView {
  if (_constrainedView == constrainedView)
    return;

  // Reset the constrained frame to null if specifying a new constrained view.
  if (constrainedView)
    self.constrainedFrame = CGRectNull;

  _constrainedView = constrainedView;
  [self updateConstraintsWithView:_constrainedView];
}

- (void)setConstrainedFrame:(CGRect)constrainedFrame {
  if (CGRectEqualToRect(_constrainedFrame, constrainedFrame))
    return;

  // Reset the constrained view to nil if specifying a new constrained frame.
  if (!CGRectIsNull(constrainedFrame))
    self.constrainedView = nil;

  _constrainedFrame = constrainedFrame;
  [self updateConstrainedFrameView];
}

- (void)setAutoresizingMask:(UIViewAutoresizing)autoresizingMask {
  if (_autoresizingMask == autoresizingMask)
    return;
  _autoresizingMask = autoresizingMask;
  [self updateConstrainedFrameView];
}

- (void)setConstraints:(NSArray*)constraints {
  if (_constraints == constraints)
    return;
  if (_constraints.count)
    [NSLayoutConstraint deactivateConstraints:_constraints];
  _constraints = constraints;
  if (_constraints.count)
    [NSLayoutConstraint activateConstraints:_constraints];
}

- (void)setConstrainedFrameView:(UIView*)constrainedFrameView {
  if (_constrainedFrameView == constrainedFrameView)
    return;

  if (_constrainedFrameView)
    [_constrainedFrameView removeFromSuperview];
  _constrainedFrameView = constrainedFrameView;

  // The constrained frame view is inserted at the bottom of the owning view's
  // hierarchy in an effort to minimize additional rendering costs.
  if (_constrainedFrameView)
    [self.owningView insertSubview:_constrainedFrameView atIndex:0];

  [self updateConstraintsWithView:_constrainedFrameView];
}

#pragma mark - Public

+ (instancetype)guideWithName:(GuideName*)name view:(UIView*)view {
  while (view) {
    for (UILayoutGuide* guide in view.layoutGuides) {
      NamedGuide* namedGuide = base::mac::ObjCCast<NamedGuide>(guide);
      if ([namedGuide.name isEqualToString:name])
        return namedGuide;
    }
    view = view.superview;
  }
  return nil;
}

- (void)resetConstraints {
  self.constrainedView = nil;
  self.constrainedFrame = CGRectNull;
}

#pragma mark - Private

- (void)updateConstraintsWithView:(UIView*)view {
  if (view) {
    self.constraints = @[
      [self.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
      [self.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
      [self.topAnchor constraintEqualToAnchor:view.topAnchor],
      [self.bottomAnchor constraintEqualToAnchor:view.bottomAnchor]
    ];
  } else {
    self.constraints = nil;
  }
}

- (void)updateConstrainedFrameView {
  // Remove the dummy view if |constrainedFrame| is null.
  if (CGRectIsNull(self.constrainedFrame)) {
    self.constrainedFrameView = nil;
    return;
  }

  // Lazily create the view if necessary and set it up using the specified frame
  // and autoresizing mask.
  // NOTE: The view's |translatesAutoresizingMaskIntoConstraints| remains set to
  // the default value of |YES| in order to leverage UIKit's built in frame =>
  // constraint conversion.
  if (!self.constrainedFrameView) {
    self.constrainedFrameView = [[UIView alloc] init];
    self.constrainedFrameView.backgroundColor = [UIColor clearColor];
  }
  self.constrainedFrameView.frame = self.constrainedFrame;
  self.constrainedFrameView.autoresizingMask = self.autoresizingMask;
}

@end
