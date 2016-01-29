// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"

#include <algorithm>
#include <utility>

#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "grit/theme_resources.h"
#include "ui/base/cocoa/appkit_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

NSString* const kBrowserActionGrippyDragStartedNotification =
    @"BrowserActionGrippyDragStartedNotification";
NSString* const kBrowserActionGrippyDraggingNotification =
    @"BrowserActionGrippyDraggingNotification";
NSString* const kBrowserActionGrippyDragFinishedNotification =
    @"BrowserActionGrippyDragFinishedNotification";
NSString* const kBrowserActionsContainerWillAnimate =
    @"BrowserActionsContainerWillAnimate";
NSString* const kBrowserActionsContainerMouseEntered =
    @"BrowserActionsContainerMouseEntered";
NSString* const kBrowserActionsContainerAnimationEnded =
    @"BrowserActionsContainerAnimationEnded";
NSString* const kTranslationWithDelta =
    @"TranslationWithDelta";
NSString* const kBrowserActionsContainerReceivedKeyEvent =
    @"BrowserActionsContainerReceivedKeyEvent";
NSString* const kBrowserActionsContainerKeyEventKey =
    @"BrowserActionsContainerKeyEventKey";

namespace {
const CGFloat kAnimationDuration = 0.2;
const CGFloat kGrippyWidth = 3.0;
const CGFloat kMinimumContainerWidth = 3.0;
}  // namespace

@interface BrowserActionsContainerView(Private)
// Returns the cursor that should be shown when hovering over the grippy based
// on |canDragLeft_| and |canDragRight_|.
- (NSCursor*)appropriateCursorForGrippy;

// Returns the maximum allowed size for the container.
- (CGFloat)maxAllowedWidth;
@end

@implementation BrowserActionsContainerView

@synthesize canDragLeft = canDragLeft_;
@synthesize canDragRight = canDragRight_;
@synthesize grippyPinned = grippyPinned_;
@synthesize maxDesiredWidth = maxDesiredWidth_;
@synthesize userIsResizing = userIsResizing_;
@synthesize delegate = delegate_;

#pragma mark -
#pragma mark Overridden Class Functions

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    grippyRect_ = NSMakeRect(0.0, 0.0, kGrippyWidth, NSHeight([self bounds]));
    canDragLeft_ = YES;
    canDragRight_ = YES;
    resizable_ = YES;

    resizeAnimation_.reset([[NSViewAnimation alloc] init]);
    [resizeAnimation_ setDuration:kAnimationDuration];
    [resizeAnimation_ setAnimationBlockingMode:NSAnimationNonblocking];
    [resizeAnimation_ setDelegate:self];

    [self setHidden:YES];
  }
  return self;
}

- (void)dealloc {
  if (trackingArea_.get())
    [self removeTrackingArea:trackingArea_.get()];
  [super dealloc];
}

- (void)drawRect:(NSRect)rect {
  [super drawRect:rect];
  if (highlight_) {
    ui::DrawNinePartImage(
        [self bounds], *highlight_, NSCompositeSourceOver, 1.0, true);
  }
}

- (void)viewDidMoveToWindow {
  if (isOverflow_) {
    // Yet another Cocoa oddity: Custom views in menu items in Cocoa, by
    // default, won't receive key events. However, if we make this the first
    // responder when it's moved to a window, it will, and it will behave
    // properly (i.e., will only receive key events if the menu item is
    // highlighted, not for any key event in the menu). More strangely,
    // setting this to be first responder at any other time (such as calling
    // [[containerView window] makeFirstResponder:containerView] when the menu
    // item is highlighted) does *not* work (it messes up the currently-
    // highlighted item).
    // Since this seems to have the right behavior, use it.
    [[self window] makeFirstResponder:self];
  }
}

- (void)setTrackingEnabled:(BOOL)enabled {
  if (enabled) {
    trackingArea_.reset(
        [[CrTrackingArea alloc] initWithRect:NSZeroRect
                                     options:NSTrackingMouseEnteredAndExited |
                                             NSTrackingActiveInActiveApp |
                                             NSTrackingInVisibleRect
                                       owner:self
                                    userInfo:nil]);
    [self addTrackingArea:trackingArea_.get()];
  } else if (trackingArea_.get()) {
    [self removeTrackingArea:trackingArea_.get()];
    [trackingArea_.get() clearOwner];
    trackingArea_.reset(nil);
  }
}

- (BOOL)trackingEnabled {
  return trackingArea_.get() != nullptr;
}

- (void)keyDown:(NSEvent*)theEvent {
  // If this is the overflow container, we handle three key events: left, right,
  // and space. Left and right navigate the actions within the container, and
  // space activates the current one. We have to handle this ourselves, because
  // Cocoa doesn't treat custom views with subviews in menu items differently
  // than any other menu item, so it would otherwise be impossible to navigate
  // to a particular action from the keyboard.
  ui::KeyboardCode key = ui::KeyboardCodeFromNSEvent(theEvent);
  BOOL shouldProcess = isOverflow_ &&
      (key == ui::VKEY_RIGHT || key == ui::VKEY_LEFT || key == ui::VKEY_SPACE);

  // If this isn't the overflow container, or isn't one of the keys we process,
  // forward the event on.
  if (!shouldProcess) {
    [super keyDown:theEvent];
    return;
  }

  // TODO(devlin): The keyboard navigation should be adjusted for RTL, but right
  // now we only ever display the extension items in the same way (LTR) on Mac.
  BrowserActionsContainerKeyAction action = BROWSER_ACTIONS_INVALID_KEY_ACTION;
  switch (key) {
    case ui::VKEY_RIGHT:
      action = BROWSER_ACTIONS_INCREMENT_FOCUS;
      break;
    case ui::VKEY_LEFT:
      action = BROWSER_ACTIONS_DECREMENT_FOCUS;
      break;
    case ui::VKEY_SPACE:
      action = BROWSER_ACTIONS_EXECUTE_CURRENT;
      break;
    default:
      NOTREACHED();  // Should have weeded this case out above.
  }

  DCHECK_NE(BROWSER_ACTIONS_INVALID_KEY_ACTION, action);
  NSDictionary* userInfo = @{ kBrowserActionsContainerKeyEventKey : @(action) };
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionsContainerReceivedKeyEvent
                    object:self
                    userInfo:userInfo];
  [super keyDown:theEvent];
}

- (void)setHighlight:(scoped_ptr<ui::NinePartImageIds>)highlight {
  if (highlight || highlight_) {
    highlight_ = std::move(highlight);
    // We don't allow resizing when the container is highlighting.
    resizable_ = highlight.get() == nullptr;
    [self setNeedsDisplay:YES];
  }
}

- (BOOL)isHighlighting {
  return highlight_.get() != nullptr;
}

- (void)setIsOverflow:(BOOL)isOverflow {
  if (isOverflow_ != isOverflow) {
    isOverflow_ = isOverflow;
    resizable_ = !isOverflow_;
    [self setNeedsDisplay:YES];
  }
}

- (void)resetCursorRects {
  [self addCursorRect:grippyRect_ cursor:[self appropriateCursorForGrippy]];
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)mouseEntered:(NSEvent*)theEvent {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionsContainerMouseEntered
                    object:self];
}

- (void)mouseDown:(NSEvent*)theEvent {
  initialDragPoint_ = [self convertPoint:[theEvent locationInWindow]
                                fromView:nil];
  if (!resizable_ ||
      !NSMouseInRect(initialDragPoint_, grippyRect_, [self isFlipped]))
    return;

  userIsResizing_ = YES;

  [[self appropriateCursorForGrippy] push];
  // Disable cursor rects so that the Omnibox and other UI elements don't push
  // cursors while the user is dragging. The cursor should be grippy until
  // the |-mouseUp:| message is received.
  [[self window] disableCursorRects];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDragStartedNotification
                    object:self];
}

- (void)mouseUp:(NSEvent*)theEvent {
  if (!userIsResizing_)
    return;

  [NSCursor pop];
  [[self window] enableCursorRects];

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
  CGFloat maxAllowedWidth = [self maxAllowedWidth];
  containerFrame.size.width =
      std::max(NSWidth(containerFrame) - dX, kMinimumContainerWidth);
  canDragLeft_ = withDelta <= initialDragPoint_.x &&
      NSWidth(containerFrame) < maxDesiredWidth_ &&
      NSWidth(containerFrame) < maxAllowedWidth;

  if ((dX < 0.0 && !canDragLeft_) || (dX > 0.0 && !canDragRight_))
    return;

  if (NSWidth(containerFrame) <= kMinimumContainerWidth)
    return;

  grippyPinned_ = NSWidth(containerFrame) >= maxAllowedWidth;
  containerFrame.origin.x += dX;

  [self setFrame:containerFrame];
  [self setNeedsDisplay:YES];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionGrippyDraggingNotification
                    object:self];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  // We notify asynchronously so that the animation fully finishes before any
  // listeners do work.
  [self performSelector:@selector(notifyAnimationEnded)
              withObject:self
              afterDelay:0];
}

- (void)animationDidStop:(NSAnimation*)animation {
  // We notify asynchronously so that the animation fully finishes before any
  // listeners do work.
  [self performSelector:@selector(notifyAnimationEnded)
              withObject:self
              afterDelay:0];
}

- (void)notifyAnimationEnded {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionsContainerAnimationEnded
                    object:self];
}

- (ViewID)viewID {
  return VIEW_ID_BROWSER_ACTION_TOOLBAR;
}

#pragma mark -
#pragma mark Public Methods

- (void)resizeToWidth:(CGFloat)width animate:(BOOL)animate {
  width = std::max(width, kMinimumContainerWidth);
  NSRect frame = [self frame];

  CGFloat maxAllowedWidth = [self maxAllowedWidth];
  width = std::min(maxAllowedWidth, width);

  CGFloat dX = frame.size.width - width;
  frame.size.width = width;
  NSRect newFrame = NSOffsetRect(frame, dX, 0);

  grippyPinned_ = width == maxAllowedWidth;

  [self stopAnimation];

  if (animate) {
    NSDictionary* animationDictionary = @{
      NSViewAnimationTargetKey : self,
      NSViewAnimationStartFrameKey : [NSValue valueWithRect:[self frame]],
      NSViewAnimationEndFrameKey : [NSValue valueWithRect:newFrame]
    };
    [resizeAnimation_ setViewAnimations:@[ animationDictionary ]];
    [resizeAnimation_ startAnimation];

    [[NSNotificationCenter defaultCenter]
        postNotificationName:kBrowserActionsContainerWillAnimate
                      object:self];
  } else {
    [self setFrame:newFrame];
    [self setNeedsDisplay:YES];
  }
}

- (NSRect)animationEndFrame {
  if ([resizeAnimation_ isAnimating]) {
    NSRect endFrame = [[[[resizeAnimation_ viewAnimations] objectAtIndex:0]
        valueForKey:NSViewAnimationEndFrameKey] rectValue];
    return endFrame;
  } else {
    return [self frame];
  }
}

- (BOOL)isAnimating {
  return [resizeAnimation_ isAnimating];
}

- (void)stopAnimation {
  if ([resizeAnimation_ isAnimating])
    [resizeAnimation_ stopAnimation];
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

- (CGFloat)maxAllowedWidth {
  return delegate_ ? delegate_->GetMaxAllowedWidth() : CGFLOAT_MAX;
}

@end
