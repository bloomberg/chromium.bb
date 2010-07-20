// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/extensions/browser_actions_container_view.h"

#include <algorithm>

#include "base/logging.h"
#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/view_id_util.h"

NSString* const kBrowserActionGrippyDragStartedNotification =
    @"BrowserActionGrippyDragStartedNotification";
NSString* const kBrowserActionGrippyDraggingNotification =
    @"BrowserActionGrippyDraggingNotification";
NSString* const kBrowserActionGrippyDragFinishedNotification =
    @"BrowserActionGrippyDragFinishedNotification";

namespace {
const CGFloat kAnimationDuration = 0.2;
const CGFloat kGrippyWidth = 8.0;
const CGFloat kLowerPadding = 6.0;
const CGFloat kMinimumContainerWidth = 10.0;
const CGFloat kRightBorderXOffset = -1.0;
const CGFloat kRightBorderWidth = 1.0;
const CGFloat kRightBorderGrayscale = 0.5;
const CGFloat kUpperPadding = 9.0;
}  // namespace

@interface BrowserActionsContainerView(Private)
// Returns the cursor that should be shown when hovering over the grippy based
// on |canDragLeft_| and |canDragRight_|.
- (NSCursor*)appropriateCursorForGrippy;
@end

@implementation BrowserActionsContainerView

@synthesize animationEndFrame = animationEndFrame_;
@synthesize canDragLeft = canDragLeft_;
@synthesize canDragRight = canDragRight_;
@synthesize grippyPinned = grippyPinned_;
@synthesize maxWidth = maxWidth_;
@synthesize userIsResizing = userIsResizing_;

#pragma mark -
#pragma mark Overridden Class Functions

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    grippyRect_ = NSMakeRect(0.0, 0.0, kGrippyWidth, NSHeight([self bounds]));
    canDragLeft_ = YES;
    canDragRight_ = YES;
    resizable_ = YES;
    [self setHidden:YES];
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  NSRect bounds = [self bounds];
  NSColor* middleColor =
     [NSColor colorWithCalibratedWhite:kRightBorderGrayscale alpha:1.0];
  NSColor* endPointColor =
     [NSColor colorWithCalibratedWhite:kRightBorderGrayscale alpha:0.0];
  scoped_nsobject<NSGradient> borderGradient([[NSGradient alloc]
     initWithColorsAndLocations:endPointColor, (CGFloat)0.0,
                                middleColor, (CGFloat)0.5,
                                endPointColor, (CGFloat)1.0,
                                nil]);
  CGFloat xPos = bounds.origin.x + bounds.size.width - kRightBorderWidth +
     kRightBorderXOffset;
  NSRect borderRect = NSMakeRect(xPos, kLowerPadding, kRightBorderWidth,
     bounds.size.height - kUpperPadding);
  [borderGradient drawInRect:borderRect angle:90.0];
}

- (void)setResizable:(BOOL)resizable {
  if (resizable == resizable_)
    return;
  resizable_ = resizable;
  [self setNeedsDisplay:YES];
}

- (BOOL)isResizable {
  return resizable_;
}

- (void)resetCursorRects {
  [self discardCursorRects];
  [self addCursorRect:grippyRect_ cursor:[self appropriateCursorForGrippy]];
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)mouseDown:(NSEvent*)theEvent {
  initialDragPoint_ = [self convertPoint:[theEvent locationInWindow]
                                fromView:nil];
  if (!resizable_ ||
      !NSMouseInRect(initialDragPoint_, grippyRect_, [self isFlipped]))
    return;

  lastXPos_ = [self frame].origin.x;
  userIsResizing_ = YES;

  // TODO(andybons): The cursor does not stick once moved outside of the
  // toolbar. Investigate further. http://crbug.com/36698
  [[self appropriateCursorForGrippy] push];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDragStartedNotification
                    object:self];
}

- (void)mouseUp:(NSEvent*)theEvent {
  if (!userIsResizing_)
    return;

  [NSCursor pop];

  userIsResizing_ = NO;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDragFinishedNotification
                    object:self];
}

- (void)mouseDragged:(NSEvent*)theEvent {
  if (!userIsResizing_)
    return;

  NSPoint location = [self convertPoint:[theEvent locationInWindow]
                               fromView:nil];
  NSRect containerFrame = [self frame];
  CGFloat dX = [theEvent deltaX];
  CGFloat withDelta = location.x - dX;
  canDragRight_ = (withDelta >= initialDragPoint_.x) &&
      (NSWidth(containerFrame) > kMinimumContainerWidth);
  canDragLeft_ = (withDelta <= initialDragPoint_.x) &&
      (NSWidth(containerFrame) < maxWidth_);
  if ((dX < 0.0 && !canDragLeft_) || (dX > 0.0 && !canDragRight_))
    return;

  containerFrame.size.width =
      std::max(NSWidth(containerFrame) - dX, kMinimumContainerWidth);

  if (NSWidth(containerFrame) == kMinimumContainerWidth)
    return;

  containerFrame.origin.x += dX;

  [self setFrame:containerFrame];
  [self setNeedsDisplay:YES];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDraggingNotification
                    object:self];

  lastXPos_ += dX;
}

- (ViewID)viewID {
  return VIEW_ID_BROWSER_ACTION_TOOLBAR;
}

#pragma mark -
#pragma mark Public Methods

- (void)resizeToWidth:(CGFloat)width animate:(BOOL)animate {
  width = std::max(width, kMinimumContainerWidth);
  NSRect frame = [self frame];
  lastXPos_ = frame.origin.x;
  CGFloat dX = frame.size.width - width;
  frame.size.width = width;
  NSRect newFrame = NSOffsetRect(frame, dX, 0);
  if (animate) {
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:kAnimationDuration];
    [[self animator] setFrame:newFrame];
    [NSAnimationContext endGrouping];
    animationEndFrame_ = newFrame;
  } else {
    [self setFrame:newFrame];
    [self setNeedsDisplay:YES];
  }
}

- (CGFloat)resizeDeltaX {
  return [self frame].origin.x - lastXPos_;
}

#pragma mark -
#pragma mark Private Methods

// Returns the cursor to display over the grippy hover region depending on the
// current drag state.
- (NSCursor*)appropriateCursorForGrippy {
  NSCursor* retVal;
  if (!resizable_ || (!canDragLeft_ && !canDragRight_)) {
    retVal = [NSCursor arrowCursor];
  } else if (!canDragLeft_) {
    retVal = [NSCursor resizeRightCursor];
  } else if (!canDragRight_) {
    retVal = [NSCursor resizeLeftCursor];
  } else {
    retVal = [NSCursor resizeLeftRightCursor];
  }
  return retVal;
}

@end
