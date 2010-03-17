// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "app/resource_bundle.h"
#include "base/logging.h"
#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/cocoa/menu_button.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"

extern const NSString* kBrowserActionGrippyDragStartedNotification =
    @"BrowserActionGrippyDragStartedNotification";
extern const NSString* kBrowserActionGrippyDraggingNotification =
    @"BrowserActionGrippyDraggingNotification";
extern const NSString* kBrowserActionGrippyDragFinishedNotification =
    @"BrowserActionGrippyDragFinishedNotification";
extern const CGFloat kChevronWidth = 14.0;

namespace {
const CGFloat kAnimationDuration = 0.2;
const CGFloat kChevronHeight = 28.0;
const CGFloat kChevronRightPadding = 5.0;
const CGFloat kGrippyLowerPadding = 4.0;
const CGFloat kGrippyUpperPadding = 8.0;
const CGFloat kGrippyWidth = 10.0;
const CGFloat kLowerPadding = 5.0;
const CGFloat kMinimumContainerWidth = 5.0;
const CGFloat kRightBorderXOffset = -1.0;
const CGFloat kRightBorderWidth = 1.0;
const CGFloat kRightBorderGrayscale = 0.5;
const CGFloat kUpperPadding = 9.0;
}  // namespace

@interface BrowserActionsContainerView(Private)
- (NSCursor*)appropriateCursorForGrippy;
- (void)drawGrippy;
- (void)updateChevronPosition;
@end

@implementation BrowserActionsContainerView

@synthesize canDragLeft = canDragLeft_;
@synthesize canDragRight = canDragRight_;
@synthesize grippyPinned = grippyPinned_;
@synthesize userIsResizing = userIsResizing_;
@synthesize rightBorderShown = rightBorderShown_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    grippyRect_ = NSMakeRect(0.0, 0.0, kGrippyWidth, NSHeight([self bounds]));
    animation_.reset([[NSViewAnimation alloc] init]);
    [animation_ setDuration:kAnimationDuration];
    [animation_ setAnimationBlockingMode:NSAnimationNonblocking];
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  if (rightBorderShown_) {
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

  [self drawGrippy];
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  [self updateChevronPosition];
}

// Draws the area that the user can use to resize the container. Currently, two
// vertical "grip" bars.
- (void)drawGrippy {
  NSRect grippyRect = NSMakeRect(0.0, kLowerPadding + kGrippyLowerPadding, 1.0,
      [self bounds].size.height - kUpperPadding - kGrippyUpperPadding);
  [[NSColor colorWithCalibratedWhite:0.7 alpha:0.5] set];
  NSRectFill(grippyRect);

  [[NSColor colorWithCalibratedWhite:1.0 alpha:1.0] set];
  grippyRect.origin.x += 1.0;
  NSRectFill(grippyRect);

  grippyRect.origin.x += 1.0;

  [[NSColor colorWithCalibratedWhite:0.7 alpha:0.5] set];
  grippyRect.origin.x += 1.0;
  NSRectFill(grippyRect);

  [[NSColor colorWithCalibratedWhite:1.0 alpha:1.0] set];
  grippyRect.origin.x += 1.0;
  NSRectFill(grippyRect);
}

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
  } else {
    // TODO(andybons): Worry about animations already in progress in this case.
    [self setFrame:newFrame];
  }
  [self setNeedsDisplay:YES];
}

// Returns the cursor to display over the grippy hover region depending on the
// current drag state.
- (NSCursor*)appropriateCursorForGrippy {
  NSCursor* retVal;
  if (!canDragLeft_ && !canDragRight_) {
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
  if (!NSMouseInRect(initialDragPoint_, grippyRect_, [self isFlipped]))
    return;

  [self setChevronHidden:YES animate:YES];

  lastXPos_ = [self frame].origin.x;
  userIsResizing_ = YES;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDragStartedNotification
                    object:self];
  // TODO(andybons): The cursor does not stick once moved outside of the
  // toolbar. Investigate further. http://crbug.com/36698
  while (1) {
    // This inner run loop catches and removes mouse up and drag events from the
    // default event queue and dispatches them to the appropriate custom
    // handlers. This is to prevent the cursor from changing (or any other side-
    // effects of dragging the mouse around the app).
    theEvent =
        [NSApp nextEventMatchingMask:NSLeftMouseUpMask | NSLeftMouseDraggedMask
                           untilDate:[NSDate distantFuture]
                              inMode:NSDefaultRunLoopMode dequeue:YES];

    NSEventType type = [theEvent type];
    if (type == NSLeftMouseDragged) {
      [self mouseDragged:theEvent];
    } else if (type == NSLeftMouseUp) {
      [self mouseUp:theEvent];
      break;
    }
  }
}

- (void)mouseUp:(NSEvent*)theEvent {
  if (!userIsResizing_)
    return;

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
  CGFloat dX = [theEvent deltaX];
  CGFloat withDelta = location.x - dX;
  canDragRight_ = withDelta >= initialDragPoint_.x;

  if ((dX < 0.0 && !canDragLeft_) || (dX > 0.0 && !canDragRight_))
    return;

  NSRect containerFrame = [self frame];
  containerFrame.origin.x += dX;
  containerFrame.size.width -= dX;
  [self setFrame:containerFrame];
  [self setNeedsDisplay:YES];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDraggingNotification
                    object:self];

  lastXPos_ += dX;
}

- (CGFloat)resizeDeltaX {
  return [self frame].origin.x - lastXPos_;
}

- (BOOL)chevronIsHidden {
  if (!chevronMenuButton_.get())
    return YES;

  if (![animation_ isAnimating])
    return [chevronMenuButton_ isHidden];

  DCHECK([[animation_ viewAnimations] count] > 0);

  // The chevron is animating in or out. Determine which one and have the return
  // value reflect where the animation is headed.
  NSString* effect = [[[animation_ viewAnimations] objectAtIndex:0]
      valueForKey:NSViewAnimationEffectKey];
  if (effect == NSViewAnimationFadeInEffect) {
    return NO;
  } else if (effect == NSViewAnimationFadeOutEffect) {
    return YES;
  }

  NOTREACHED();
  return YES;
}

- (void)setChevronHidden:(BOOL)hidden animate:(BOOL)animate {
  if (hidden == [self chevronIsHidden])
    return;

  if (!chevronMenuButton_.get()) {
    chevronMenuButton_.reset([[MenuButton alloc] init]);
    [chevronMenuButton_ setBordered:NO];
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    [chevronMenuButton_ setImage:rb.GetNSImageNamed(IDR_BOOKMARK_BAR_CHEVRONS)];
    [self addSubview:chevronMenuButton_];
  }

  [self updateChevronPosition];
  // Stop any running animation.
  [animation_ stopAnimation];

  if (!animate) {
    [chevronMenuButton_ setHidden:hidden];
    return;
  }

  NSDictionary* animationDictionary;
  if (hidden) {
    animationDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
        chevronMenuButton_.get(), NSViewAnimationTargetKey,
        NSViewAnimationFadeOutEffect, NSViewAnimationEffectKey,
        nil];
  } else {
    [chevronMenuButton_ setHidden:NO];
    animationDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
        chevronMenuButton_.get(), NSViewAnimationTargetKey,
        NSViewAnimationFadeInEffect, NSViewAnimationEffectKey,
        nil];
  }
  [animation_ setViewAnimations:
      [NSArray arrayWithObjects:animationDictionary, nil]];
  [animation_ startAnimation];
}

- (void)updateChevronPosition {
  CGFloat xPos = NSWidth([self frame]) - kChevronWidth - kChevronRightPadding;
  NSRect buttonFrame = NSMakeRect(xPos,
                                  kLowerPadding,
                                  kChevronWidth,
                                  kChevronHeight);
  [chevronMenuButton_ setFrame:buttonFrame];
}

@end
