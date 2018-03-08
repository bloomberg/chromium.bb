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

// The constraints used to connect the guide to |constrainedView|.
@property(nonatomic, strong) NSArray* constrainedViewConstraints;

@end

@implementation NamedGuide
@synthesize name = _name;
@synthesize constrainedView = _constrainedView;
@synthesize constrainedViewConstraints = _constrainedViewConstraints;

- (instancetype)initWithName:(GuideName*)name {
  if (self = [super init]) {
    _name = name;
  }
  return self;
}

- (void)dealloc {
  self.constrainedView = nil;
}

#pragma mark - Accessors

- (void)setConstrainedView:(UIView*)constrainedView {
  if (_constrainedView == constrainedView)
    return;
  _constrainedView = constrainedView;
  if (_constrainedView) {
    self.constrainedViewConstraints = @[
      [self.leadingAnchor
          constraintEqualToAnchor:_constrainedView.leadingAnchor],
      [self.trailingAnchor
          constraintEqualToAnchor:_constrainedView.trailingAnchor],
      [self.topAnchor constraintEqualToAnchor:_constrainedView.topAnchor],
      [self.bottomAnchor constraintEqualToAnchor:_constrainedView.bottomAnchor]
    ];
  } else {
    self.constrainedViewConstraints = nil;
  }
}

- (void)setConstrainedViewConstraints:(NSArray*)constrainedViewConstraints {
  if (_constrainedViewConstraints == constrainedViewConstraints)
    return;
  if (_constrainedViewConstraints.count)
    [NSLayoutConstraint deactivateConstraints:_constrainedViewConstraints];
  _constrainedViewConstraints = constrainedViewConstraints;
  if (_constrainedViewConstraints.count)
    [NSLayoutConstraint activateConstraints:_constrainedViewConstraints];
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

@end
