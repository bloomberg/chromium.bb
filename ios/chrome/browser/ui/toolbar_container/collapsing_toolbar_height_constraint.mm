// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/collapsing_toolbar_height_constraint.h"

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_collapsing.h"
#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The progress range.
const CGFloat kMinProgress = 0.0;
const CGFloat kMaxProgress = 1.0;
}  // namespace

@interface CollapsingToolbarHeightConstraint ()
// Redefine as readwrite.
@property(nonatomic, readwrite) CGFloat collapsedHeight;
@property(nonatomic, readwrite) CGFloat expandedHeight;
// The collapsing toolbar whose height range is being observed.
@property(nonatomic, weak) UIView<ToolbarCollapsing>* collapsingToolbar;
@end

@implementation CollapsingToolbarHeightConstraint
@synthesize collapsedHeight = _collapsedHeight;
@synthesize expandedHeight = _expandedHeight;
@synthesize additionalHeight = _additionalHeight;
@synthesize collapsesAdditionalHeight = _collapsesAdditionalHeight;
@synthesize progress = _progress;
@synthesize collapsingToolbar = _collapsingToolbar;

+ (instancetype)constraintWithView:(UIView*)view {
  DCHECK(view);
  CollapsingToolbarHeightConstraint* constraint =
      [[self class] constraintWithItem:view
                             attribute:NSLayoutAttributeHeight
                             relatedBy:NSLayoutRelationEqual
                                toItem:nil
                             attribute:NSLayoutAttributeNotAnAttribute
                            multiplier:0.0
                              constant:0.0];
  if ([view conformsToProtocol:@protocol(ToolbarCollapsing)]) {
    constraint.collapsingToolbar =
        static_cast<UIView<ToolbarCollapsing>*>(view);
  } else {
    CGFloat intrinsicHeight = view.intrinsicContentSize.height;
    constraint.collapsedHeight = intrinsicHeight;
    constraint.expandedHeight = intrinsicHeight;
  }
  constraint.progress = 1.0;
  [constraint updateHeight];

  return constraint;
}

#pragma mark - Accessors

- (void)setActive:(BOOL)active {
  [super setActive:active];
  if (self.active)
    [self startObservingCollapsingToolbar];
  else
    [self stopObservingCollapsingToolbar];
}

- (void)setCollapsedHeight:(CGFloat)collapsedHeight {
  if (AreCGFloatsEqual(_collapsedHeight, collapsedHeight))
    return;
  _collapsedHeight = collapsedHeight;
  [self updateHeight];
}

- (void)setExpandedHeight:(CGFloat)expandedHeight {
  if (AreCGFloatsEqual(_expandedHeight, expandedHeight))
    return;
  _expandedHeight = expandedHeight;
  [self updateHeight];
}

- (void)setAdditionalHeight:(CGFloat)additionalHeight {
  if (AreCGFloatsEqual(_additionalHeight, additionalHeight))
    return;
  _additionalHeight = additionalHeight;
  [self updateHeight];
}

- (void)setCollapsesAdditionalHeight:(BOOL)collapsesAdditionalHeight {
  if (_collapsesAdditionalHeight == collapsesAdditionalHeight)
    return;
  _collapsesAdditionalHeight = collapsesAdditionalHeight;
  [self updateHeight];
}

- (void)setProgress:(CGFloat)progress {
  progress = base::ClampToRange(progress, kMinProgress, kMaxProgress);
  if (AreCGFloatsEqual(_progress, progress))
    return;
  _progress = progress;
  [self updateHeight];
}

- (void)setCollapsingToolbar:(UIView<ToolbarCollapsing>*)collapsingToolbar {
  if (_collapsingToolbar == collapsingToolbar)
    return;

  [self stopObservingCollapsingToolbar];
  _collapsingToolbar = collapsingToolbar;
  [self updateToolbarHeightRange];
  if (self.active)
    [self startObservingCollapsingToolbar];
}

#pragma mark - Public

- (CGFloat)toolbarHeightForProgress:(CGFloat)progress {
  progress = base::ClampToRange(progress, kMinProgress, kMaxProgress);
  CGFloat base = self.collapsedHeight;
  CGFloat range = self.expandedHeight - self.collapsedHeight;
  if (self.collapsesAdditionalHeight) {
    range += self.additionalHeight;
  } else {
    base += self.additionalHeight;
  }
  return base + progress * range;
}

#pragma mark - KVO

- (void)observeValueForKeyPath:(NSString*)key
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  [self updateToolbarHeightRange];
}

#pragma mark - KVO Helpers

- (NSArray<NSString*>* const)collapsingToolbarKeyPaths {
  static NSArray<NSString*>* const kKeyPaths =
      @[ @"expandedToolbarHeight", @"collapsedToolbarHeight" ];
  return kKeyPaths;
}

- (void)startObservingCollapsingToolbar {
  for (NSString* keyPath in [self collapsingToolbarKeyPaths]) {
    [self.collapsingToolbar addObserver:self
                             forKeyPath:keyPath
                                options:NSKeyValueObservingOptionNew
                                context:nullptr];
  }
}

- (void)stopObservingCollapsingToolbar {
  for (NSString* keyPath in [self collapsingToolbarKeyPaths]) {
    [self.collapsingToolbar removeObserver:self forKeyPath:keyPath];
  }
}

#pragma mark - Private

// Updates the constraint using the collapsing toolbar's height range.
- (void)updateToolbarHeightRange {
  self.collapsedHeight = self.collapsingToolbar.collapsedToolbarHeight;
  self.expandedHeight = self.collapsingToolbar.expandedToolbarHeight;
}

// Updates the constraint's constant
- (void)updateHeight {
  self.constant = [self toolbarHeightForProgress:self.progress];
}

@end
