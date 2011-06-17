// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/infobars/infobar_tip_drawing_model.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"
#include "chrome/browser/ui/cocoa/infobars/infobar_gradient_view.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

@interface InfobarTipDrawingModel (Private)
- (BrowserWindowController*)browserWindowController;
- (InfoBarController*)controllerForView:(NSView*)view;
- (NSBezierPath*)pathForController:(InfoBarController*)controller;
- (NSPoint)pointForController:(InfoBarController*)controller;
- (BOOL)firstInfobarUsesExpandedTip;
@end

////////////////////////////////////////////////////////////////////////////////

@implementation InfobarTipDrawingModel

- (id)initWithContainerController:(InfoBarContainerController*)controller {
  if ((self = [super init])) {
    containerController_ = controller;
  }
  return self;
}

- (infobars::Tip*)createTipForView:(InfoBarGradientView*)view {
  InfoBarController* controller = [self controllerForView:view];
  if (!controller)
    return NULL;

  infobars::Tip* tip = new infobars::Tip();
  tip_ = tip;

  tip->point = [self pointForController:controller];
  if ([containerController_ indexOfController:controller] == 0 &&
      [self firstInfobarUsesExpandedTip]) {
    tip->point.x -= infobars::kExpandedTipWidth / 2;
  } else {
    tip->point.x -= infobars::kDefaultTipWidth / 2;
  }
  tip->path.reset([[self pathForController:controller] retain]);
  tip->point.y = tip->tip_height - tip->point.y;

  tip_ = NULL;
  return tip;
}

- (CGFloat)totalHeightForController:(InfoBarController*)controller {
  NSUInteger index = [containerController_ indexOfController:controller];
  DCHECK_NE(NSNotFound, index);

  const CGFloat height = infobars::kBaseHeight;
  if (index == 0 && [self firstInfobarUsesExpandedTip])
    return height + infobars::kExpandedTipHeight;
  return height + infobars::kDefaultTipHeight;
}

// Private /////////////////////////////////////////////////////////////////////

// Gets the BrowserWindowController for this model.
- (BrowserWindowController*)browserWindowController {
  return [BrowserWindowController browserWindowControllerForView:
      [containerController_ view]];
}

// Iterates through the infobars and finds the controller associated with a
// given view.
- (InfoBarController*)controllerForView:(NSView*)view {
  NSUInteger count = [containerController_ infobarCount];
  for (NSUInteger i = 0; i < count; ++i) {
    InfoBarController* controller = [containerController_ controllerAtIndex:i];
    if ([view isDescendantOf:[controller view]])
      return controller;
  }
  return nil;
}

// Returns a bezier path for the given controller. This is a three-point path
// that forms an isosceles triangle arrow tip.
- (NSBezierPath*)pathForController:(InfoBarController*)controller {
  NSUInteger index = [containerController_ indexOfController:controller];
  DCHECK_NE(NSNotFound, index);

  NSSize tipSize;
  // Special case the first infobar because it has the extra-long tip.
  if (index == 0 && [self firstInfobarUsesExpandedTip]) {
    tipSize = NSMakeSize(infobars::kExpandedTipWidth,
                         infobars::kExpandedTipHeight);
  } else {
    tipSize = NSMakeSize(infobars::kDefaultTipWidth,
                         infobars::kDefaultTipHeight);
  }

  // Set up the base infobar.
  NSRect bounds = [[controller gradientView] bounds];
  bounds.size.height = infobars::kBaseHeight;

  tip_->tip_height = tipSize.height;

  // Create a path for the tip that acts as the anti-spoofing countermeasure.
  NSBezierPath* tipPath = [NSBezierPath bezierPath];
  NSPoint startPoint = NSMakePoint(tip_->point.x, infobars::kBaseHeight);
  [tipPath moveToPoint:startPoint];
  [tipPath relativeLineToPoint:NSMakePoint(tipSize.width/2, tipSize.height)];
  tip_->mid_point = [tipPath currentPoint];
  [tipPath relativeLineToPoint:NSMakePoint(tipSize.width/2, -tipSize.height)];

  return tipPath;
}

// Returns the point, in gradient view coordinates, at which the infobar tip
// should start being drawn.
- (NSPoint)pointForController:(InfoBarController*)controller {
  BrowserWindowController* windowController = [self browserWindowController];
  LocationBarViewMac* locationBar = [windowController locationBarBridge];
  NSPoint point = locationBar->GetPageInfoBubblePoint();
  point = [[controller gradientView] convertPoint:point fromView:nil];
  return point;
}

// Determines whether or not to use the expanded tip size for the first infobar.
- (BOOL)firstInfobarUsesExpandedTip {
  BrowserWindowController* controller = [self browserWindowController];
  BookmarkBarController* bookmarkBar = [controller bookmarkBarController];
  return [bookmarkBar isInState:bookmarks::kShowingState] ||
      [bookmarkBar isAnimatingToState:bookmarks::kShowingState] ||
      [bookmarkBar isAnimatingFromState:bookmarks::kShowingState];
}

@end
