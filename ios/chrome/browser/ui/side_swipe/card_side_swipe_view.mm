// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/card_side_swipe_view.h"

#include <cmath>

#include "base/ios/device_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/background_generator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_util.h"
#import "ios/chrome/browser/ui/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
// Spacing between cards.
const CGFloat kCardHorizontalSpacing = 30;

// Portion of the screen an edge card can be dragged.
const CGFloat kEdgeCardDragPercentage = 0.35;

// Card animation times.
const NSTimeInterval kAnimationDuration = 0.15;

// Reduce size in -smallGreyImage by this factor.
const CGFloat kResizeFactor = 4;
}  // anonymous namespace

@implementation SwipeView

- (id)initWithFrame:(CGRect)frame topMargin:(CGFloat)topMargin {
  self = [super initWithFrame:frame];
  if (self) {
    image_.reset([[UIImageView alloc] initWithFrame:CGRectZero]);
    [image_ setClipsToBounds:YES];
    [image_ setContentMode:UIViewContentModeScaleAspectFill];
    [self addSubview:image_];

    toolbarHolder_.reset([[UIImageView alloc] initWithFrame:CGRectZero]);
    [self addSubview:toolbarHolder_];

    shadowView_.reset([[UIImageView alloc] initWithFrame:self.bounds]);
    [shadowView_ setImage:NativeImage(IDR_IOS_TOOLBAR_SHADOW)];
    [self addSubview:shadowView_];

    // All subviews are as wide as the parent
    NSMutableArray* constraints = [NSMutableArray array];
    for (UIView* view in self.subviews) {
      [view setTranslatesAutoresizingMaskIntoConstraints:NO];
      [constraints addObject:[view.leadingAnchor
                                 constraintEqualToAnchor:self.leadingAnchor]];
      [constraints addObject:[view.trailingAnchor
                                 constraintEqualToAnchor:self.trailingAnchor]];
    }

    [constraints addObjectsFromArray:@[
      [[image_ topAnchor] constraintEqualToAnchor:self.topAnchor
                                         constant:topMargin],
      [[image_ bottomAnchor] constraintEqualToAnchor:self.bottomAnchor],
      [[toolbarHolder_ topAnchor] constraintEqualToAnchor:self.topAnchor
                                                 constant:-StatusBarHeight()],
      [[toolbarHolder_ heightAnchor]
          constraintEqualToConstant:topMargin + StatusBarHeight()],
      [[shadowView_ topAnchor] constraintEqualToAnchor:self.topAnchor
                                              constant:topMargin],
      [[shadowView_ heightAnchor]
          constraintEqualToConstant:kNewTabPageShadowHeight]
    ]];

    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateImageBoundsAndZoom];
}

- (void)updateImageBoundsAndZoom {
  UIImage* image = [image_ image];
  if (image) {
    CGSize imageSize = image.size;
    CGSize viewSize = [image_ frame].size;
    CGFloat zoomRatio = std::max(viewSize.height / imageSize.height,
                                 viewSize.width / imageSize.width);
    [image_ layer].contentsRect =
        CGRectMake(0.0, 0.0, viewSize.width / (zoomRatio * imageSize.width),
                   viewSize.height / (zoomRatio * imageSize.height));
  }
}

- (void)setImage:(UIImage*)image {
  [image_ setImage:image];
  [self updateImageBoundsAndZoom];
}

- (void)setToolbarImage:(UIImage*)image isNewTabPage:(BOOL)isNewTabPage {
  [toolbarHolder_ setImage:image];
  [shadowView_ setHidden:isNewTabPage];
}

@end

@interface CardSideSwipeView ()
// Pan touches ended or were cancelled.
- (void)finishPan;
// Is the current card is an edge card based on swipe direction.
- (BOOL)isEdgeSwipe;
// Initialize card based on model_ index.
- (void)setupCard:(SwipeView*)card
        withIndex:(NSInteger)index
      withToolbar:(WebToolbarController*)toolbarController;
// Build a |kResizeFactor| sized greyscaled version of |image|.
- (UIImage*)smallGreyImage:(UIImage*)image;
@end

@implementation CardSideSwipeView

@synthesize delegate = delegate_;
@synthesize topMargin = topMargin_;

- (id)initWithFrame:(CGRect)frame
          topMargin:(CGFloat)topMargin
              model:(TabModel*)model {
  self = [super initWithFrame:frame];
  if (self) {
    model_ = model;
    currentPoint_ = CGPointZero;
    topMargin_ = topMargin;

    UIView* background = [[UIView alloc] initWithFrame:CGRectZero];
    [self addSubview:background];

    [background setTranslatesAutoresizingMaskIntoConstraints:NO];
    [NSLayoutConstraint activateConstraints:@[
      [[background rightAnchor] constraintEqualToAnchor:self.rightAnchor],
      [[background leftAnchor] constraintEqualToAnchor:self.leftAnchor],
      [[background topAnchor] constraintEqualToAnchor:self.topAnchor
                                             constant:-StatusBarHeight()],
      [[background bottomAnchor] constraintEqualToAnchor:self.bottomAnchor]
    ]];

    InstallBackgroundInView(background);

    rightCard_.reset(
        [[SwipeView alloc] initWithFrame:CGRectZero topMargin:topMargin]);
    leftCard_.reset(
        [[SwipeView alloc] initWithFrame:CGRectZero topMargin:topMargin]);
    [rightCard_ setTranslatesAutoresizingMaskIntoConstraints:NO];
    [leftCard_ setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:rightCard_];
    [self addSubview:leftCard_];
    AddSameConstraints(rightCard_, self);
    AddSameConstraints(leftCard_, self);
  }
  return self;
}

- (CGRect)cardFrame {
  return self.bounds;
}

// Set up left and right card views depending on current tab and swipe
// direction.
- (void)updateViewsForDirection:(UISwipeGestureRecognizerDirection)direction
                    withToolbar:(WebToolbarController*)tbc {
  direction_ = direction;
  CGRect cardFrame = [self cardFrame];
  NSUInteger currentIndex = [model_ indexOfTab:model_.currentTab];
  CGFloat offset = UseRTLLayout() ? -1 : 1;
  if (direction_ == UISwipeGestureRecognizerDirectionRight) {
    [self setupCard:rightCard_ withIndex:currentIndex withToolbar:tbc];
    [rightCard_ setFrame:cardFrame];
    [self setupCard:leftCard_ withIndex:currentIndex - offset withToolbar:tbc];
    cardFrame.origin.x -= cardFrame.size.width + kCardHorizontalSpacing;
    [leftCard_ setFrame:cardFrame];
  } else {
    [self setupCard:leftCard_ withIndex:currentIndex withToolbar:tbc];
    [leftCard_ setFrame:cardFrame];
    [self setupCard:rightCard_ withIndex:currentIndex + offset withToolbar:tbc];
    cardFrame.origin.x += cardFrame.size.width + kCardHorizontalSpacing;
    [rightCard_ setFrame:cardFrame];
  }
  [tbc resetToolbarAfterSideSwipeSnapshot];
}

- (UIImage*)smallGreyImage:(UIImage*)image {
  CGRect smallSize = CGRectMake(0, 0, image.size.width / kResizeFactor,
                                image.size.height / kResizeFactor);
  // Using CIFilter here on iOS 5+ might be faster, but it doesn't easily
  // allow for resizing.  At the max size, it's still too slow for side swipe.
  UIGraphicsBeginImageContextWithOptions(smallSize.size, YES, 0);
  [image drawInRect:smallSize blendMode:kCGBlendModeLuminosity alpha:1.0];
  UIImage* greyImage = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return greyImage;
}

// Create card view based on TabModel index.
- (void)setupCard:(SwipeView*)card
        withIndex:(NSInteger)index
      withToolbar:(WebToolbarController*)toolbarController {
  if (index < 0 || index >= (NSInteger)[model_ count]) {
    [card setHidden:YES];
    return;
  }
  [card setHidden:NO];

  Tab* tab = [model_ tabAtIndex:index];
  BOOL isNTP = tab.lastCommittedURL.host() == kChromeUINewTabHost;
  [toolbarController updateToolbarForSideSwipeSnapshot:tab];
  UIImage* toolbarView = CaptureViewWithOption([toolbarController view],
                                               [[UIScreen mainScreen] scale],
                                               kClientSideRendering);
  [card setToolbarImage:toolbarView isNewTabPage:isNTP];

  // Converting snapshotted images to grey takes too much time for single core
  // devices.  Instead, show the colored image for single core devices and the
  // grey image for multi core devices.
  dispatch_queue_t priorityQueue =
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0ul);
  [tab retrieveSnapshot:^(UIImage* image) {
    if (tab.webController.usePlaceholderOverlay &&
        !ios::device_util::IsSingleCoreDevice()) {
      [card setImage:[CRWWebController defaultSnapshotImage]];
      dispatch_async(priorityQueue, ^{
        UIImage* greyImage = [self smallGreyImage:image];
        dispatch_async(dispatch_get_main_queue(), ^{
          [card setImage:greyImage];
        });
      });
    } else {
      [card setImage:image];
    }
  }];
}

// Place cards around |currentPoint_.x|, and lean towards each card near the
// X edges of |bounds|.  Shrink cards as they are dragged towards the middle of
// the |bounds|, and edge cards only drag |kEdgeCardDragPercentage| of |bounds|.
- (void)updateCardPositions {
  CGRect bounds = [self cardFrame];
  [rightCard_ setFrame:bounds];
  [leftCard_ setFrame:bounds];

  CGFloat width = CGRectGetWidth([self cardFrame]);
  CGPoint center = CGPointMake(bounds.origin.x + bounds.size.width / 2,
                               bounds.origin.y + bounds.size.height / 2);
  if ([self isEdgeSwipe]) {
    // If an edge card, don't allow the card to be dragged across the screen.
    // Instead, drag across |kEdgeCardDragPercentage| of the screen.
    center.x = currentPoint_.x - width / 2 -
               (currentPoint_.x - width) / width *
                   (width * (1 - kEdgeCardDragPercentage));
    [leftCard_ setCenter:center];
    center.x = currentPoint_.x / width * (width * kEdgeCardDragPercentage) +
               bounds.size.width / 2;
    [rightCard_ setCenter:center];
  } else {
    // Place cards around the finger as it is dragged across the screen.
    // Place the finger between the cards in the middle of the screen, on the
    // right card border when on the left side of the screen, and on the left
    // card border when on the right side of the screen.
    CGFloat rightXBuffer = kCardHorizontalSpacing * currentPoint_.x / width;
    CGFloat leftXBuffer = kCardHorizontalSpacing - rightXBuffer;

    center.x = currentPoint_.x - leftXBuffer - width / 2;
    [leftCard_ setCenter:center];

    center.x = currentPoint_.x + rightXBuffer + width / 2;
    [rightCard_ setCenter:center];
  }
}

// Update layout with new touch event.
- (void)handleHorizontalPan:(SideSwipeGestureRecognizer*)gesture {
  currentPoint_ = [gesture locationInView:self];
  currentPoint_.x -= gesture.swipeOffset;

  // Since it's difficult to touch the very edge of the screen (touches tend to
  // sit near x ~ 4), push the touch towards the edge.
  CGFloat width = CGRectGetWidth([self cardFrame]);
  CGFloat half = floor(width / 2);
  CGFloat padding = floor(std::abs(currentPoint_.x - half) / half);

  // Push towards the edges.
  if (currentPoint_.x > half)
    currentPoint_.x += padding;
  else
    currentPoint_.x -= padding;

  // But don't go past the edges.
  if (currentPoint_.x < 0)
    currentPoint_.x = 0;
  else if (currentPoint_.x > width)
    currentPoint_.x = width;

  [self updateCardPositions];

  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled ||
      gesture.state == UIGestureRecognizerStateFailed) {
    [self finishPan];
  }
}

- (BOOL)isEdgeSwipe {
  NSUInteger currentIndex = [model_ indexOfTab:model_.currentTab];
  return (IsSwipingBack(direction_) && currentIndex == 0) ||
         (IsSwipingForward(direction_) && currentIndex == [model_ count] - 1);
}

// Update the current tab and animate the proper card view if the
// |currentPoint_| is past the center of |bounds|.
- (void)finishPan {
  NSUInteger currentIndex = [model_ indexOfTab:model_.currentTab];
  // Something happened and now currentTab is gone.  End card side swipe and let
  // BVC show no tabs UI.
  if (currentIndex == NSNotFound)
    return [delegate_ sideSwipeViewDismissAnimationDidEnd:self];

  CGRect finalSize = [self cardFrame];
  CGFloat width = CGRectGetWidth([self cardFrame]);
  CGRect leftFrame, rightFrame;
  SwipeView* dominantCard;
  Tab* destinationTab = model_.currentTab;
  CGFloat offset = UseRTLLayout() ? -1 : 1;
  if (direction_ == UISwipeGestureRecognizerDirectionRight) {
    // If swipe is right and |currentPoint_.x| is over the first 1/3, move left.
    if (currentPoint_.x > width / 3.0 && ![self isEdgeSwipe]) {
      destinationTab = [model_ tabAtIndex:currentIndex - offset];
      dominantCard = leftCard_;
      rightFrame = leftFrame = finalSize;
      rightFrame.origin.x += rightFrame.size.width + kCardHorizontalSpacing;
      base::RecordAction(UserMetricsAction("MobileStackSwipeCompleted"));
    } else {
      dominantCard = rightCard_;
      leftFrame = rightFrame = finalSize;
      leftFrame.origin.x -= rightFrame.size.width + kCardHorizontalSpacing;
      base::RecordAction(UserMetricsAction("MobileStackSwipeCancelled"));
    }
  } else {
    // If swipe is left and |currentPoint_.x| is over the first 1/3, move right.
    if (currentPoint_.x < (width / 3.0) * 2.0 && ![self isEdgeSwipe]) {
      destinationTab = [model_ tabAtIndex:currentIndex + offset];
      dominantCard = rightCard_;
      leftFrame = rightFrame = finalSize;
      leftFrame.origin.x -= rightFrame.size.width + kCardHorizontalSpacing;
      base::RecordAction(UserMetricsAction("MobileStackSwipeCompleted"));
    } else {
      dominantCard = leftCard_;
      rightFrame = leftFrame = finalSize;
      rightFrame.origin.x += rightFrame.size.width + kCardHorizontalSpacing;
      base::RecordAction(UserMetricsAction("MobileStackSwipeCancelled"));
    }
  }

  // Changing the model even when the tab is the same at the end of the
  // animation allows the UI to recover.
  [model_ setCurrentTab:destinationTab];

  // Make sure the dominant card animates on top.
  [dominantCard.superview bringSubviewToFront:dominantCard];

  [UIView animateWithDuration:kAnimationDuration
      animations:^{
        [leftCard_ setTransform:CGAffineTransformIdentity];
        [rightCard_ setTransform:CGAffineTransformIdentity];
        [leftCard_ setFrame:leftFrame];
        [rightCard_ setFrame:rightFrame];
      }
      completion:^(BOOL finished) {
        [leftCard_ setImage:nil];
        [rightCard_ setImage:nil];
        [leftCard_ setToolbarImage:nil isNewTabPage:NO];
        [rightCard_ setToolbarImage:nil isNewTabPage:NO];
        [delegate_ sideSwipeViewDismissAnimationDidEnd:self];
      }];
}

@end
