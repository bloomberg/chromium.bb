// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_panel_view.h"

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"

// The position of the MenuViewWrapper doesn't change, but its subview menuView
// can slide horizontally. This UIView subclass decides whether to swallow
// touches based on the transform of its subview, since its subview might lie
// outsides the bounds of itself.
@interface MenuViewWrapper : UIView {
  base::mac::ObjCPropertyReleaser _propertyReleaser_MenuViewWrapper;
}
@property(nonatomic, retain) UIView* menuView;
@end

@implementation MenuViewWrapper
@synthesize menuView = _menuView;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_MenuViewWrapper.Init(self, [MenuViewWrapper class]);
  }
  return self;
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  return CGRectContainsPoint(self.menuView.frame, point);
}

@end

@interface BookmarkPanelView ()<UIGestureRecognizerDelegate> {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkPanelView;
}
// The content view always has the same size as this view.
// Redefined to be read-write.
@property(nonatomic, retain) UIView* contentView;
// When the menu is showing, the cover partially obscures the content view.
@property(nonatomic, retain) UIView* contentViewCover;
// The menu view's frame never changes. Sliding it left and right is performed
// by changing its transform property.
// Redefined to be read-write.
@property(nonatomic, retain) UIView* menuView;
// The menu view's layout is adjusted by changing its transform property.
// Changing the transform property results in a layoutSubviews call to the
// parentView. To prevent confusion to the origin of the layoutSubview call, the
// menu is placed inside a wrapper. The wrapper is always placed offscreen to
// the left. It requires a UIView subclass to correctly decide whether touches
// should make it to the menuView.
@property(nonatomic, retain) MenuViewWrapper* menuViewWrapper;
@property(nonatomic, assign) CGFloat menuWidth;
@property(nonatomic, retain) UIPanGestureRecognizer* panRecognizer;

// This property corresponds to whether startPoint is valid. It also reflects
// whether this class is responding to a user-driven animation.
@property(nonatomic, assign) BOOL hasStartPoint;
@property(nonatomic, assign) CGPoint startPoint;
// The most recent point of the user's pan gesture.
@property(nonatomic, assign) CGPoint lastPoint;

// When an animation that tracks the user's gesture is in progress, this
// property reflects the state of the menu at the beginning of the animation.
// Redefined to be read-write.
@property(nonatomic, assign) BOOL showingMenu;

// The user panned the view.
// Invoked frequently during a pan gesture.
- (void)panRecognized:(id)target;
// Returns true if the last point was updated.
// Updates the last point of the user's gesture.
// If hasStartPoint is NO, sets the startPoint and sets hasStartPoint to YES.
- (BOOL)updateLastPoint;
// The width of the menu. This does not change when the screen orientation
// changes.
- (CGFloat)menuWidth;
// Resets all state and UI pertaining to the user driven animation.
- (void)resetUserDrivenAnimation;
// Callback for when the user tapped the content view cover.
- (void)contentViewCoverTapped;
// Updates the layout of subviews. Similar to layoutSubviews, but intended to
// also be called from -init.
- (void)updateLayout;
// Given a touch position, calculates the visible width of menu respecting menu
// state (open/closed) and RTL.
- (CGFloat)peekWidthWithTouchPosition:(CGFloat)position;
// Updates menu visibility given the visible width of menu, respecting RTL.
- (void)updateMenuPositionWithPeekWidth:(CGFloat)peekWidth;
@end

@implementation BookmarkPanelView
@synthesize contentView = _contentView;
@synthesize contentViewCover = _contentViewCover;
@synthesize delegate = _delegate;
@synthesize hasStartPoint = _hasStartPoint;
@synthesize lastPoint = _lastPoint;
@synthesize menuView = _menuView;
@synthesize menuViewWrapper = _menuViewWrapper;
@synthesize menuWidth = _menuWidth;
@synthesize panRecognizer = _panRecognizer;
@synthesize showingMenu = _showingMenu;
@synthesize startPoint = _startPoint;

#pragma mark Initialization

- (id)init {
  NOTREACHED();
  return nil;
}

- (id)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (id)initWithFrame:(CGRect)frame menuViewWidth:(CGFloat)width {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkPanelView.Init(self, [BookmarkPanelView class]);

    DCHECK(width);
    _menuWidth = width;

    self.contentView = base::scoped_nsobject<UIView>([[UIView alloc] init]);
    self.contentView.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    [self addSubview:self.contentView];

    self.contentViewCover =
        base::scoped_nsobject<UIView>([[UIView alloc] init]);
    [self addSubview:self.contentViewCover];
    self.contentViewCover.backgroundColor =
        [UIColor colorWithWhite:0 alpha:0.8];
    self.contentViewCover.alpha = 0;

    base::scoped_nsobject<UITapGestureRecognizer> tapRecognizer(
        [[UITapGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(contentViewCoverTapped)]);
    [self.contentViewCover addGestureRecognizer:tapRecognizer];
    self.contentViewCover.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;

    self.menuViewWrapper =
        base::scoped_nsobject<MenuViewWrapper>([[MenuViewWrapper alloc] init]);
    self.menuViewWrapper.backgroundColor = [UIColor clearColor];
    [self addSubview:self.menuViewWrapper];

    self.menuView = base::scoped_nsobject<UIView>([[UIView alloc] init]);
    [self.menuViewWrapper addSubview:self.menuView];
    self.menuViewWrapper.menuView = self.menuView;

    self.panRecognizer = base::scoped_nsobject<UIPanGestureRecognizer>(
        [[UIPanGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(panRecognized:)]);
    [self addGestureRecognizer:self.panRecognizer];

    [self updateLayout];
  }
  return self;
}

#pragma mark Gesture recognizer

- (void)panRecognized:(id)target {
  switch (self.panRecognizer.state) {
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateBegan:
      [self updateLastPoint];
      break;

    case UIGestureRecognizerStateChanged: {
      BOOL hasPoint = [self updateLastPoint];

      if (hasPoint) {
        CGFloat touchPosition =
            [self.panRecognizer locationOfTouch:0 inView:self].x;
        CGFloat peekWidth = [self peekWidthWithTouchPosition:touchPosition];
        [self updateMenuPositionWithPeekWidth:peekWidth];

        CGFloat visibility = peekWidth / self.menuWidth;
        self.contentViewCover.alpha = visibility;
        [self.delegate bookmarkPanelView:self updatedMenuVisibility:visibility];
      }
      break;
    }
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
      [self resetUserDrivenAnimation];
      break;
  }
}

- (BOOL)updateLastPoint {
  if ([self.panRecognizer numberOfTouches] == 0)
    return NO;

  self.lastPoint = [self.panRecognizer locationOfTouch:0 inView:self];

  if (!self.hasStartPoint) {
    self.hasStartPoint = YES;
    self.startPoint = self.lastPoint;
  }

  return YES;
}

#pragma mark Layout

- (void)layoutSubviews {
  [self resetUserDrivenAnimation];
  [self updateLayout];
}

- (void)updateLayout {
  self.contentView.frame = self.bounds;
  self.contentViewCover.frame = self.bounds;

  CGFloat menuLeading = self.showingMenu ? 0 : -1 * self.menuWidth;
  LayoutRect menuWrapperLayout =
      LayoutRectMake(menuLeading, self.bounds.size.width, 0, self.menuWidth,
                     self.bounds.size.height);

  self.menuViewWrapper.frame = LayoutRectGetRect(menuWrapperLayout);
  self.menuView.frame = self.menuViewWrapper.bounds;
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  if (!self.showingMenu)
    return NO;
  [self hideMenuAnimated:YES];
  return YES;
}

#pragma mark - Public Methods

- (void)showMenuAnimated:(BOOL)animated {
  if (self.hasStartPoint)
    return;

  self.showingMenu = YES;
  self.menuViewWrapper.accessibilityViewIsModal = YES;

  CGFloat animationDuration = 0;

  if (animated) {
    CGFloat baseDuration = bookmark_utils_ios::menuAnimationDuration;
    // Reduce the time of the animation if the menu is close to its destination.
    CGFloat closeness =
        fabs(self.menuWidth - self.menuView.transform.tx) / self.menuWidth;
    animationDuration = baseDuration * closeness;
    animationDuration = MIN(baseDuration, animationDuration);
  }

  [self.delegate bookmarkPanelView:self
                      willShowMenu:YES
             withAnimationDuration:animationDuration];

  [UIView animateWithDuration:animated ? animationDuration : 0
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        [self updateMenuPositionWithPeekWidth:self.menuWidth];
        self.contentViewCover.alpha = 1;
      }
      completion:^(BOOL finished) {
        UIAccessibilityPostNotification(
            UIAccessibilityScreenChangedNotification, self.menuView);
      }];
}

- (void)hideMenuAnimated:(BOOL)animated {
  if (self.hasStartPoint)
    return;

  self.showingMenu = NO;
  self.menuViewWrapper.accessibilityViewIsModal = NO;

  CGFloat animationDuration = 0;

  if (animated) {
    CGFloat baseDuration = bookmark_utils_ios::menuAnimationDuration;
    // Reduce the time of the animation if the menu is close to its destination.
    CGFloat closeness = fabs(self.menuView.transform.tx) / self.menuWidth;
    animationDuration = baseDuration * closeness;
    animationDuration = MIN(baseDuration, animationDuration);
  }

  [self.delegate bookmarkPanelView:self
                      willShowMenu:NO
             withAnimationDuration:animationDuration];

  [UIView animateWithDuration:animated ? animationDuration : 0
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        [self updateMenuPositionWithPeekWidth:0];
        self.contentViewCover.alpha = 0;
      }
      completion:^(BOOL finished) {
        UIAccessibilityPostNotification(
            UIAccessibilityScreenChangedNotification, self.contentView);
      }];
}

- (BOOL)userDrivenAnimationInProgress {
  return self.hasStartPoint;
}

- (void)enableSideSwiping:(BOOL)enable {
  self.panRecognizer.enabled = enable;
}

#pragma mark Private methods

- (void)resetUserDrivenAnimation {
  // If no user-driven animation is in progress, there's nothing to do.
  if (!self.hasStartPoint)
    return;

  CGFloat width = self.menuWidth;
  CGFloat peekWidth = [self peekWidthWithTouchPosition:self.lastPoint.x];

  self.hasStartPoint = NO;

  // If the menu is more than half showing when the user lets go, open it all
  // the way. Otherwise, close it all the way.
  if (self.showingMenu) {
    if (peekWidth < width / 2) {
      [self hideMenuAnimated:YES];
    } else {
      [self showMenuAnimated:YES];
    }
  } else {
    if (peekWidth > width / 2) {
      [self showMenuAnimated:YES];
    } else {
      [self hideMenuAnimated:YES];
    }
  }
}

- (void)contentViewCoverTapped {
  [self hideMenuAnimated:YES];
}

- (CGFloat)peekWidthWithTouchPosition:(CGFloat)position {
  if (!self.hasStartPoint)
    return 0;

  CGFloat delta = position - self.startPoint.x;
  CGFloat peekWidth = 0;
  CGFloat menuWidth = self.menuWidth;
  if (self.showingMenu) {
    // The menu is already open.
    if (UseRTLLayout()) {
      delta = MAX(0, delta);
      peekWidth = menuWidth - delta;
    } else {
      delta = MIN(0, delta);
      peekWidth = menuWidth + delta;
    }
  } else {
    // The menu is not open yet.
    if (UseRTLLayout()) {
      delta = MIN(0, delta);
      peekWidth = -1 * delta;
    } else {
      delta = MAX(0, delta);
      peekWidth = delta;
    }
  }

  peekWidth = MIN(peekWidth, menuWidth);
  peekWidth = MAX(0, peekWidth);
  return peekWidth;
}

- (void)updateMenuPositionWithPeekWidth:(CGFloat)peekWidth {
  DCHECK(peekWidth >= 0);
  DCHECK(peekWidth <= self.menuWidth);

  self.menuView.transform = CGAffineTransformMakeTranslation(
      UseRTLLayout() ? -peekWidth : peekWidth, 0);
}

@end
