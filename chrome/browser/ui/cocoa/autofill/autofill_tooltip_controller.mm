// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_tooltip_controller.h"

#include "base/mac/foundation_util.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_bubble_controller.h"
#import "ui/base/cocoa/hover_image_button.h"

// Delay time before tooltip shows/hides.
const NSTimeInterval kTooltipDelay = 0.1;

// How far to inset tooltip contents.
CGFloat kTooltipInset = 10;

#pragma mark AutofillTooltip

// The actual tooltip control - based on HoverButton, which comes with free
// hover handling.
@interface AutofillTooltip : HoverButton {
 @private
  id<AutofillTooltipDelegate> tooltipDelegate_;
}

@property(assign, nonatomic) id<AutofillTooltipDelegate> tooltipDelegate;

@end


@implementation AutofillTooltip

@synthesize tooltipDelegate = tooltipDelegate_;

- (void)drawRect:(NSRect)rect {
 [[self image] drawInRect:rect
                 fromRect:NSZeroRect
                operation:NSCompositeSourceOver
                 fraction:1.0
           respectFlipped:YES
                    hints:nil];
}

- (void)setHoverState:(HoverState)state {
  HoverState oldHoverState = [self hoverState];
  [super setHoverState:state];
  if (state != oldHoverState) {
    switch (state) {
      case kHoverStateNone:
        [tooltipDelegate_ didEndHover];
        break;
      case kHoverStateMouseOver:
        [tooltipDelegate_ didBeginHover];
        break;
      case kHoverStateMouseDown:
        break;
    }
  }
}

@end

#pragma mark AutofillTooltipController

@implementation AutofillTooltipController

@synthesize message = message_;

- (id)initWithArrowLocation:(info_bubble::BubbleArrowLocation)arrowLocation {
  if ((self = [super init])) {
    arrowLocation_ = arrowLocation;
    view_.reset([[AutofillTooltip alloc] init]);
    [self setView:view_];
    [view_ setTooltipDelegate:self];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:[bubbleController_ window]];
  [super dealloc];
}

- (void)setImage:(NSImage*)image {
  [view_ setImage:image];
  [view_ setFrameSize:[image size]];
}

- (void)didBeginHover {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  // Start a timer to display the tooltip, unless it's already displayed.
  if (!bubbleController_) {
    [self performSelector:@selector(displayHover)
               withObject:nil
               afterDelay:kTooltipDelay];
  }
}

- (void)tooltipWindowWillClose:(NSNotification*)notification {
  bubbleController_ = nil;
}

- (void)displayHover {
  [bubbleController_ close];
  bubbleController_ =
    [[AutofillBubbleController alloc]
        initWithParentWindow:[[self view] window]
                     message:[self message]
                       inset:NSMakeSize(kTooltipInset, kTooltipInset)
               arrowLocation:arrowLocation_];
  [bubbleController_ setShouldCloseOnResignKey:NO];

  // Handle bubble self-deleting.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(tooltipWindowWillClose:)
                 name:NSWindowWillCloseNotification
               object:[bubbleController_ window]];

  // Compute anchor point (in window coords - views might be flipped).
  NSRect viewRect = [view_ convertRect:[view_ bounds] toView:nil];
  NSPoint anchorPoint = NSMakePoint(NSMidX(viewRect), NSMinY(viewRect));
  [bubbleController_ setAnchorPoint:
      [[[self view] window] convertBaseToScreen:anchorPoint]];
  [bubbleController_ showWindow:self];
}

- (void)hideHover {
  [bubbleController_ close];
}

- (void)didEndHover {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  // Start a timer to display the tooltip, unless it's already hidden.
  if (bubbleController_) {
    [self performSelector:@selector(hideHover)
               withObject:nil
               afterDelay:kTooltipDelay];
  }
}

@end
