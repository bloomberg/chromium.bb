// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller_container_view.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

#pragma mark - CRWToolbarContainerView

// Class that manages the display of toolbars.
@interface CRWToolbarContainerView : UIView {
  // Backing object for |self.toolbars|.
  base::scoped_nsobject<NSMutableArray> _toolbars;
}

// The toolbars currently managed by this view.
@property(nonatomic, retain, readonly) NSMutableArray* toolbars;

// Adds |toolbar| as a subview and bottom aligns to any previously added
// toolbars.
- (void)addToolbar:(UIView*)toolbar;

// Removes |toolbar| from the container view.
- (void)removeToolbar:(UIView*)toolbar;

@end

@implementation CRWToolbarContainerView

#pragma mark Accessors

- (NSMutableArray*)toolbars {
  if (!_toolbars)
    _toolbars.reset([[NSMutableArray alloc] init]);
  return _toolbars.get();
}

#pragma mark Layout

- (void)layoutSubviews {
  [super layoutSubviews];

  // Bottom-align the toolbars.
  CGPoint toolbarOrigin =
      CGPointMake(self.bounds.origin.x, CGRectGetMaxY(self.bounds));
  for (UIView* toolbar in self.toolbars) {
    CGSize toolbarSize = [toolbar sizeThatFits:self.bounds.size];
    toolbarSize.width = self.bounds.size.width;
    toolbarOrigin.y -= toolbarSize.height;
    toolbar.frame = CGRectMake(toolbarOrigin.x, toolbarOrigin.y,
                               toolbarSize.width, toolbarSize.height);
  }
}

- (CGSize)sizeThatFits:(CGSize)size {
  CGSize boundingRectSize = CGSizeMake(size.width, CGFLOAT_MAX);
  CGFloat necessaryHeight = 0.0f;
  for (UIView* toolbar in self.toolbars)
    necessaryHeight += [toolbar sizeThatFits:boundingRectSize].height;
  return CGSizeMake(size.width, necessaryHeight);
}

#pragma mark Toolbars

- (void)addToolbar:(UIView*)toolbar {
  DCHECK(toolbar);
  DCHECK(![self.toolbars containsObject:toolbar]);
  toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.toolbars addObject:toolbar];
  [self addSubview:toolbar];
}

- (void)removeToolbar:(UIView*)toolbar {
  DCHECK(toolbar);
  DCHECK([self.toolbars containsObject:toolbar]);
  [self.toolbars removeObject:toolbar];
  [toolbar removeFromSuperview];
}

@end

#pragma mark - CRWWebControllerContainerView

@interface CRWWebControllerContainerView () {
  // Backing object for |self.toolbarContainerView|.
  base::scoped_nsobject<CRWToolbarContainerView> _toolbarContainerView;
}

// Container view that displays any added toolbars.  It is always the top-most
// subview, and is bottom aligned with the CRWWebControllerContainerView.
@property(nonatomic, retain, readonly)
    CRWToolbarContainerView* toolbarContainerView;

@end

@implementation CRWWebControllerContainerView

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor whiteColor];
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return self;
}

#pragma mark Accessors

- (void)setToolbarContainerView:(CRWToolbarContainerView*)toolbarContainerView {
  if (![_toolbarContainerView isEqual:toolbarContainerView]) {
    [_toolbarContainerView removeFromSuperview];
    _toolbarContainerView.reset([toolbarContainerView retain]);
    [self addSubview:_toolbarContainerView];
  }
}

- (UIView*)toolbarContainerView {
  return _toolbarContainerView.get();
}

#pragma mark Layout

- (void)layoutSubviews {
  [super layoutSubviews];

  if (self.toolbarContainerView) {
    [self bringSubviewToFront:self.toolbarContainerView];
    CGSize toolbarContainerSize =
        [self.toolbarContainerView sizeThatFits:self.bounds.size];
    self.toolbarContainerView.frame =
        CGRectMake(CGRectGetMinX(self.bounds),
                   CGRectGetMaxY(self.bounds) - toolbarContainerSize.height,
                   toolbarContainerSize.width, toolbarContainerSize.height);
  }
}

#pragma mark Toolbars

- (void)addToolbar:(UIView*)toolbar {
  // Create toolbar container if necessary.
  if (!self.toolbarContainerView) {
    self.toolbarContainerView = [
        [[CRWToolbarContainerView alloc] initWithFrame:CGRectZero] autorelease];
  }
  // Add the toolbar to the container.
  [self.toolbarContainerView addToolbar:toolbar];
  [self setNeedsLayout];
}

- (void)addToolbars:(NSArray*)toolbars {
  DCHECK(toolbars);
  for (UIView* toolbar in toolbars)
    [self addToolbar:toolbar];
}

- (void)removeToolbar:(UIView*)toolbar {
  // Remove the toolbar from the container view.
  [self.toolbarContainerView removeToolbar:toolbar];
  // Reset the container if there are no more toolbars.
  if ([self.toolbarContainerView.toolbars count])
    [self setNeedsLayout];
  else
    self.toolbarContainerView = nil;
}

- (void)removeAllToolbars {
  // Resetting the property will remove the toolbars from the hierarchy.
  self.toolbarContainerView = nil;
}

@end
