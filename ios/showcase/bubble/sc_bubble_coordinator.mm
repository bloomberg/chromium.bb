// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/bubble/sc_bubble_coordinator.h"

#include "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCBubbleCoordinator ()
@property(nonatomic, strong) UIViewController* containerViewController;
@property(nonatomic, strong) BubbleViewController* bubbleViewController;
@end

@implementation SCBubbleCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize containerViewController = _containerViewController;
@synthesize bubbleViewController = _bubbleViewController;

- (void)start {
  self.containerViewController = [[UIViewController alloc] init];
  UIView* containerView = self.containerViewController.view;
  containerView.backgroundColor = [UIColor whiteColor];
  self.containerViewController.title = @"Bubble";

  BubbleArrowDirection direction = BubbleArrowDirectionUp;
  BubbleAlignment alignment = BubbleAlignmentTrailing;
  self.bubbleViewController =
      [[BubbleViewController alloc] initWithText:@"Lorem ipsum dolor"
                                  arrowDirection:direction
                                       alignment:alignment];

  // Mock UI element for the bubble to be anchored on. Set the x-coordinate of
  // the origin to be two-thirds of the container's width.
  UIView* elementView = [[UIView alloc]
      initWithFrame:CGRectMake(containerView.frame.size.width * 2.0f / 3.0f,
                               20.0f, 20.0f, 20.0f)];
  elementView.backgroundColor = [UIColor grayColor];
  [containerView addSubview:elementView];
  // Maximum width of the bubble such that it stays within |containerView|.
  CGFloat maxBubbleWidth = bubble_util::MaxWidth(
      elementView.frame, alignment, containerView.frame.size.width);
  CGSize maxSize =
      CGSizeMake(maxBubbleWidth, containerView.frame.size.height -
                                     CGRectGetMaxY(elementView.frame));
  CGSize bubbleSize = [self.bubbleViewController.view sizeThatFits:maxSize];
  CGRect bubbleFrame =
      [self bubbleFrameWithTargetFrame:elementView.frame
                            bubbleSize:bubbleSize
                        arrowDirection:direction
                             alignment:alignment
                         boundingWidth:containerView.frame.size.width];

  [self addBubbleViewControllerWithFrame:bubbleFrame];
  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

#pragma mark - Private methods

// Add |bubbleViewController| as a child view controller of the container and
// set its frame.
- (void)addBubbleViewControllerWithFrame:(CGRect)bubbleFrame {
  [self.containerViewController
      addChildViewController:self.bubbleViewController];
  self.bubbleViewController.view.frame = bubbleFrame;
  self.bubbleViewController.view.backgroundColor = [UIColor blueColor];
  [self.containerViewController.view addSubview:self.bubbleViewController.view];
  [self.bubbleViewController
      didMoveToParentViewController:self.containerViewController];
}

// Calculate the bubble's frame using bubble_util methods. Depends on the target
// view, bubble size, arrow direction, alignment, and the width of the
// containing view.
- (CGRect)bubbleFrameWithTargetFrame:(CGRect)targetFrame
                          bubbleSize:(CGSize)size
                      arrowDirection:(BubbleArrowDirection)direction
                           alignment:(BubbleAlignment)alignment
                       boundingWidth:(CGFloat)boundingWidth {
  CGFloat leading =
      bubble_util::LeadingDistance(targetFrame, alignment, size.width);
  CGFloat originY = bubble_util::OriginY(targetFrame, direction, size.height);
  // Use a |LayoutRect| to ensure that the bubble is mirrored in RTL contexts.
  CGRect bubbleFrame = LayoutRectGetRect(
      LayoutRectMake(leading, boundingWidth, originY, size.width, size.height));
  return bubbleFrame;
}

@end
