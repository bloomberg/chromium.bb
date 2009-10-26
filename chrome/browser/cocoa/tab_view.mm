// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_view.h"

#include "base/logging.h"
#include "base/nsimage_cache_mac.h"
#import "chrome/browser/cocoa/tab_controller.h"
#import "chrome/browser/cocoa/tab_window_controller.h"

// Constants for inset and control points for tab shape.
static const CGFloat kInsetMultiplier = 2.0/3.0;
static const CGFloat kControlPoint1Multiplier = 1.0/3.0;
static const CGFloat kControlPoint2Multiplier = 3.0/8.0;

static const NSTimeInterval kAnimationShowDuration = 0.2;
static const NSTimeInterval kAnimationHideDuration = 0.4;

@implementation TabView

@synthesize state = state_;
@synthesize hoverAlpha = hoverAlpha_;

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    chromeIsVisible_ = YES;
    [self setShowsDivider:NO];
    // TODO(alcor): register for theming, either here or the cell
    // [self gtm_registerForThemeNotifications];
  }
  return self;
}

- (void)awakeFromNib {
  [self setShowsDivider:NO];
  // Set up the tracking rect for the close button mouseover.  Add it
  // to the |closeButton_| view, but we'll handle the message ourself.
  // The mouseover is always enabled, because the close button works
  // regardless of key/main/active status.
  closeTrackingArea_.reset(
      [[NSTrackingArea alloc] initWithRect:[closeButton_ bounds]
                                   options:NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveAlways
                                     owner:self
                                  userInfo:nil]);
  [closeButton_ addTrackingArea:closeTrackingArea_.get()];
}

- (void)dealloc {
  // Cancel any delayed requests that may still be pending (drags or hover).
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  // [self gtm_unregisterForThemeNotifications];
  [closeButton_ removeTrackingArea:closeTrackingArea_.get()];
  [super dealloc];
}

// Overridden so that mouse clicks come to this view (the parent of the
// hierarchy) first. We want to handle clicks and drags in this class and
// leave the background button for display purposes only.
- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return YES;
}

- (void)adjustHoverValue {
  NSTimeInterval thisUpdate = [NSDate timeIntervalSinceReferenceDate];

  NSTimeInterval elapsed = thisUpdate - lastHoverUpdate_;

  CGFloat opacity = [self hoverAlpha];
  if (isMouseInside_) {
    opacity += elapsed / kAnimationShowDuration;
  } else {
    opacity -= elapsed / kAnimationHideDuration;
  }

  if (!isMouseInside_ && opacity < 0) {
    opacity = 0;
  } else if (isMouseInside_ && opacity > 1) {
    opacity = 1;
  } else {
    [self performSelector:_cmd withObject:nil afterDelay:0.02];
  }
  lastHoverUpdate_ = thisUpdate;
  [self setHoverAlpha:opacity];

  [self setNeedsDisplay:YES];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  if ([theEvent trackingArea] == closeTrackingArea_) {
    [closeButton_ setImage:nsimage_cache::ImageNamed(@"close_bar_h.pdf")];
  } else {
    lastHoverUpdate_ = [NSDate timeIntervalSinceReferenceDate];
    isMouseInside_ = YES;
    [self adjustHoverValue];
    [self setNeedsDisplay:YES];
  }
}

- (void)mouseMoved:(NSEvent*)theEvent {
  hoverPoint_ = [self convertPoint:[theEvent locationInWindow]
                          fromView:nil];
  [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)theEvent {
  if ([theEvent trackingArea] == closeTrackingArea_) {
    [closeButton_ setImage:nsimage_cache::ImageNamed(@"close_bar.pdf")];
  } else {
    lastHoverUpdate_ = [NSDate timeIntervalSinceReferenceDate];
    isMouseInside_ = NO;
    [self adjustHoverValue];
    [self setNeedsDisplay:YES];
  }
}

// Determines which view a click in our frame actually hit. It's either this
// view or our child close button.
- (NSView*)hitTest:(NSPoint)aPoint {
  NSPoint viewPoint = [self convertPoint:aPoint fromView:[self superview]];
  NSRect frame = [self frame];

  // Reduce the width of the hit rect slightly to remove the overlap
  // between adjacent tabs.  The drawing code in TabCell has the top
  // corners of the tab inset by height*2/3, so we inset by half of
  // that here.  This doesn't completely eliminate the overlap, but it
  // works well enough.
  NSRect hitRect = NSInsetRect(frame, frame.size.height / 3.0f, 0);
  if (![closeButton_ isHidden])
    if (NSPointInRect(viewPoint, [closeButton_ frame])) return closeButton_;
  if (NSPointInRect(aPoint, hitRect)) return self;
  return nil;
}

// Returns |YES| if this tab can be torn away into a new window.
- (BOOL)canBeDragged {
  NSWindowController* controller = [sourceWindow_ windowController];
  if ([controller isKindOfClass:[TabWindowController class]]) {
    TabWindowController* realController =
        static_cast<TabWindowController*>(controller);
    return [realController isTabDraggable:self];
  }
  return YES;
}

// Returns an array of controllers that could be a drop target, ordered front to
// back. It has to be of the appropriate class, and visible (obviously). Note
// that the window cannot be a target for itself.
- (NSArray*)dropTargetsForController:(TabWindowController*)dragController {
  NSMutableArray* targets = [NSMutableArray array];
  NSWindow* dragWindow = [dragController window];
  for (NSWindow* window in [NSApp orderedWindows]) {
    if (window == dragWindow) continue;
    if (![window isVisible]) continue;
    NSWindowController* controller = [window windowController];
    if ([controller isKindOfClass:[TabWindowController class]]) {
      TabWindowController* realController =
          static_cast<TabWindowController*>(controller);
      if ([realController canReceiveFrom:dragController])
        [targets addObject:controller];
    }
  }
  return targets;
}

// Call to clear out transient weak references we hold during drags.
- (void)resetDragControllers {
  draggedController_ = nil;
  dragWindow_ = nil;
  dragOverlay_ = nil;
  sourceController_ = nil;
  sourceWindow_ = nil;
  targetController_ = nil;
}

// Handle clicks and drags in this button. We get here because we have
// overridden acceptsFirstMouse: and the click is within our bounds.
// TODO(pinkerton/alcor): This routine needs *a lot* of work to marry Cole's
// ideas of dragging cocoa views between windows and how the Browser and
// TabStrip models want to manage tabs.

static const CGFloat kTearDistance = 36.0;
static const NSTimeInterval kTearDuration = 0.333;

// This is used to judge whether the mouse has moved during rapid closure; if it
// has moved less than the threshold, we want to close the tab.
static const CGFloat kRapidCloseDist = 2.5;

- (void)mouseDown:(NSEvent*)theEvent {
  NSPoint downLocation = [theEvent locationInWindow];

  // During the tab closure animation (in particular, during rapid tab closure),
  // we may get incorrectly hit with a mouse down. If it should have gone to the
  // close button, we send it there -- it should then track the mouse, so we
  // don't have to worry about mouse ups.
  if ([controller_ inRapidClosureMode]) {
    NSPoint hitLocation = [[self superview] convertPoint:downLocation
                                                fromView:nil];
    if ([self hitTest:hitLocation] == closeButton_) {
      [closeButton_ mouseDown:theEvent];
      return;
    }
  }

  // Fire the action to select the tab.
  if ([[controller_ target] respondsToSelector:[controller_ action]])
    [[controller_ target] performSelector:[controller_ action]
                               withObject:self];

  [self resetDragControllers];

  // Resolve overlay back to original window.
  sourceWindow_ = [self window];
  if ([sourceWindow_ isKindOfClass:[NSPanel class]]) {
    sourceWindow_ = [sourceWindow_ parentWindow];
  }

  sourceWindowFrame_ = [sourceWindow_ frame];
  sourceTabFrame_ = [self frame];
  sourceController_ = [sourceWindow_ windowController];
  tabWasDragged_ = NO;
  tearTime_ = 0.0;
  draggingWithinTabStrip_ = YES;

  // If there's more than one potential window to be a drop target, we want to
  // treat a drag of a tab just like dragging around a tab that's already
  // detached. Note that unit tests might have |-numberOfTabs| reporting zero
  // since the model won't be fully hooked up. We need to be prepared for that
  // and not send them into the "magnetic" codepath.
  NSArray* targets = [self dropTargetsForController:sourceController_];
  moveWindowOnDrag_ =
      ([sourceController_ numberOfTabs] < 2 && ![targets count]) ||
      ![self canBeDragged];
  // If we are dragging a tab, a window with a single tab should immediately
  // snap off and not drag within the tab strip.
  if (!moveWindowOnDrag_)
    draggingWithinTabStrip_ = [sourceController_ numberOfTabs] > 1;

  dragOrigin_ = [NSEvent mouseLocation];

  // Because we move views between windows, we need to handle the event loop
  // ourselves. Ideally we should use the standard event loop.
  while (1) {
    theEvent =
    [NSApp nextEventMatchingMask:NSLeftMouseUpMask | NSLeftMouseDraggedMask
                       untilDate:[NSDate distantFuture]
                          inMode:NSDefaultRunLoopMode dequeue:YES];
    NSPoint thisPoint = [NSEvent mouseLocation];

    NSEventType type = [theEvent type];
    if (type == NSLeftMouseDragged) {
      [self mouseDragged:theEvent];
    } else if (type == NSLeftMouseUp) {
      NSPoint upLocation = [theEvent locationInWindow];
      CGFloat dx = upLocation.x - downLocation.x;
      CGFloat dy = upLocation.y - downLocation.y;

      // During rapid tab closure (mashing tab close buttons), we may get hit
      // with a mouse down. As long as the mouse up is over the close button,
      // and the mouse hasn't moved too much, we close the tab.
      if ((dx*dx + dy*dy) <= kRapidCloseDist*kRapidCloseDist &&
          [controller_ inRapidClosureMode]) {
        NSPoint hitLocation =
            [[self superview] convertPoint:[theEvent locationInWindow]
                                  fromView:nil];
        if ([self hitTest:hitLocation] == closeButton_) {
          [controller_ closeTab:self];
          break;
        }
      }

      [self mouseUp:theEvent];
      break;
    } else {
      // TODO(viettrungluu): [crbug.com/23830] We can receive right-mouse-ups
      // (and maybe even others?) for reasons I don't understand. So we
      // explicitly check for both events we're expecting, and log others. We
      // should figure out what's going on.
      LOG(WARNING) << "Spurious event received of type " << type << ".";
    }
  }
}

- (void)mouseDragged:(NSEvent*)theEvent {
  // Special-case this to keep the logic below simpler.
  if (moveWindowOnDrag_) {
    NSPoint thisPoint = [NSEvent mouseLocation];
    NSPoint origin = sourceWindowFrame_.origin;
    origin.x += (thisPoint.x - dragOrigin_.x);
    origin.y += (thisPoint.y - dragOrigin_.y);
    [sourceWindow_ setFrameOrigin:NSMakePoint(origin.x, origin.y)];
    return;
  }

  // First, go through the magnetic drag cycle. We break out of this if
  // "stretchiness" ever exceeds a set amount.
  tabWasDragged_ = YES;

  if (draggingWithinTabStrip_) {
    NSRect frame = [self frame];
    NSPoint thisPoint = [NSEvent mouseLocation];
    CGFloat stretchiness = thisPoint.y - dragOrigin_.y;
    stretchiness = copysign(sqrtf(fabs(stretchiness))/sqrtf(kTearDistance),
                            stretchiness) / 2.0;
    CGFloat offset = thisPoint.x - dragOrigin_.x;
    if (fabsf(offset) > 100) stretchiness = 0;
    [sourceController_ insertPlaceholderForTab:self
                                         frame:NSOffsetRect(sourceTabFrame_,
                                                            offset, 0)
                                 yStretchiness:stretchiness];
    // Check that we haven't pulled the tab too far to start a drag. This
    // can include either pulling it too far down, or off the side of the tab
    // strip that would cause it to no longer be fully visible.
    BOOL stillVisible = [sourceController_ isTabFullyVisible:self];
    CGFloat tearForce = fabs(thisPoint.y - dragOrigin_.y);
    if (tearForce > kTearDistance || !stillVisible) {
      draggingWithinTabStrip_ = NO;
      // When you finally leave the strip, we treat that as the origin.
      dragOrigin_.x = thisPoint.x;
    } else {
      return;
    }
  }

  NSPoint lastPoint =
    [[theEvent window] convertBaseToScreen:[theEvent locationInWindow]];

  // Do not start dragging until the user has "torn" the tab off by
  // moving more than 3 pixels.
  NSDate* targetDwellDate = nil;  // The date this target was first chosen.

  NSPoint thisPoint = [NSEvent mouseLocation];

  // Iterate over possible targets checking for the one the mouse is in.
  // If the tab is just in the frame, bring the window forward to make it
  // easier to drop something there. If it's in the tab strip, set the new
  // target so that it pops into that window. We can't cache this because we
  // need the z-order to be correct.
  NSArray* targets = [self dropTargetsForController:draggedController_];
  TabWindowController* newTarget = nil;
  for (TabWindowController* target in targets) {
    NSRect windowFrame = [[target window] frame];
    if (NSPointInRect(thisPoint, windowFrame)) {
      // TODO(pinkerton): If bringing the window to the front immediately is too
      // annoying, use another dwell date. Can't use |targetDwellDate| because
      // this hasn't yet become the new target until the mouse is in the tab
      // strip.
      [[target window] orderFront:self];
      NSRect tabStripFrame = [[target tabStripView] frame];
      tabStripFrame.origin = [[target window]
                              convertBaseToScreen:tabStripFrame.origin];
      if (NSPointInRect(thisPoint, tabStripFrame)) {
        newTarget = target;
      }
      break;
    }
  }

  // If we're now targeting a new window, re-layout the tabs in the old
  // target and reset how long we've been hovering over this new one.
  if (targetController_ != newTarget) {
    targetDwellDate = [NSDate date];
    [targetController_ removePlaceholder];
    targetController_ = newTarget;
    if (!newTarget) {
      tearTime_ = [NSDate timeIntervalSinceReferenceDate];
      tearOrigin_ = [dragWindow_ frame].origin;
    }
  }

  // Create or identify the dragged controller.
  if (!draggedController_) {
    // Detach from the current window and put it in a new window. If there are
    // no more tabs remaining after detaching, the source window is about to
    // go away (it's been autoreleased) so we need to ensure we don't reference
    // it any more. In that case the new controller becomes our source
    // controller.
    draggedController_ = [sourceController_ detachTabToNewWindow:self];
    dragWindow_ = [draggedController_ window];
    [dragWindow_ setAlphaValue:0.0];
    if (![sourceController_ numberOfTabs]) {
      sourceController_ = draggedController_;
      sourceWindow_ = dragWindow_;
    }

    // If dragging the tab only moves the current window, do not show overlay
    // so that sheets stay on top of the window.
    // Bring the target window to the front and make sure it has a border.
    [dragWindow_ setLevel:NSFloatingWindowLevel];
    [dragWindow_ orderFront:nil];
    [dragWindow_ makeMainWindow];
    [draggedController_ showOverlay];
    dragOverlay_ = [draggedController_ overlayWindow];
    // Force the new tab button to be hidden. We'll reset it on mouse up.
    [draggedController_ showNewTabButton:NO];
    //if (![targets count])
    //  [dragOverlay_ setHasShadow:NO];
    tearTime_ = [NSDate timeIntervalSinceReferenceDate];
    tearOrigin_ = sourceWindowFrame_.origin;
  }

  DCHECK(draggedController_);
  DCHECK(sourceController_);

  // When the user first tears off the window, we want slide the window to
  // the current mouse location (to reduce the jarring appearance). We do this
  // by calling ourselves back with additional mouseDragged calls (not actual
  // events). |tearProgress| is a normalized measure of how far through this
  // tear "animation" (of length kTearDuration) we are and has values [0..1].
  // We use sqrt() so the animation is non-linear (slow down near the end
  // point).
  float tearProgress = [NSDate timeIntervalSinceReferenceDate] - tearTime_;
  tearProgress /= kTearDuration;  // Normalize.
  tearProgress = sqrtf(MAX(MIN(tearProgress, 1.0), 0.0));

  // Move the dragged window to the right place on the screen.
  NSPoint origin = sourceWindowFrame_.origin;
  origin.x += (thisPoint.x - dragOrigin_.x);
  origin.y += (thisPoint.y - dragOrigin_.y);

  if (tearProgress < 1) {
    // If the tear animation is not complete, call back to ourself with the
    // same event to animate even if the mouse isn't moving. We need to make
    // sure these get cancelled in mouseUp:.
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSelector:@selector(mouseDragged:)
               withObject:theEvent
               afterDelay:1.0f/30.0f];

    // Set the current window origin based on how far we've progressed through
    // the tear animation.
    origin.x = (1 - tearProgress) * tearOrigin_.x + tearProgress * origin.x;
    origin.y = (1 - tearProgress) * tearOrigin_.y + tearProgress * origin.y;
  }

  if (targetController_) {
    // In order to "snap" two windows of different sizes together at their
    // toolbar, we can't just use the origin of the target frame. We also have
    // to take into consideration the difference in height.
    NSRect targetFrame = [[targetController_ window] frame];
    NSRect sourceFrame = [dragWindow_ frame];
    origin.y = NSMinY(targetFrame) +
                (NSHeight(targetFrame) - NSHeight(sourceFrame));
  }
  [dragWindow_ setFrameOrigin:NSMakePoint(origin.x, origin.y)];

  // If we're not hovering over any window, make the window is fully
  // opaque. Otherwise, find where the tab might be dropped and insert
  // a placeholder so it appears like it's part of that window.
  if (targetController_) {
    if (![[targetController_ window] isKeyWindow]) {
      // && ([targetDwellDate timeIntervalSinceNow] < -REQUIRED_DWELL)) {
      [[targetController_ window] orderFront:nil];
      targetDwellDate = nil;
    }

    // Compute where placeholder should go and insert it into the
    // destination tab strip.
    NSRect dropTabFrame = [[targetController_ tabStripView] frame];
    TabView* draggedTabView = (TabView*)[draggedController_ selectedTabView];
    NSRect tabFrame = [draggedTabView frame];
    tabFrame.origin = [dragWindow_ convertBaseToScreen:tabFrame.origin];
    tabFrame.origin = [[targetController_ window]
                        convertScreenToBase:tabFrame.origin];
    tabFrame = [[targetController_ tabStripView]
                convertRectFromBase:tabFrame];
    NSPoint point =
      [sourceWindow_ convertBaseToScreen:
       [draggedTabView convertPointToBase:NSZeroPoint]];
    [targetController_ insertPlaceholderForTab:self
                                         frame:tabFrame
                                 yStretchiness:0];
    [targetController_ layoutTabs];
  } else {
    [dragWindow_ makeKeyAndOrderFront:nil];
  }
  BOOL chromeShouldBeVisible = targetController_ == nil;

  if (chromeIsVisible_ != chromeShouldBeVisible) {
    // TODO(pinkerton): There appears to be a race-condition in CoreAnimation
    // where if we use animators to set the alpha values, we can't guarantee
    // that we cancel them. This has the side effect of sometimes leaving
    // the dragged window translucent or invisible. We should re-visit this,
    // but for now, don't animate the alpha change.
    [dragWindow_ setHasShadow:YES];
    [[draggedController_ overlayWindow] setAlphaValue:1.0];
    if (targetController_) {
      [dragWindow_ setAlphaValue:0.0];
      [[draggedController_ overlayWindow] setHasShadow:YES];
    } else {
      [dragWindow_ setAlphaValue:0.5];
      [[draggedController_ overlayWindow] setHasShadow:NO];
    }
    chromeIsVisible_ = chromeShouldBeVisible;
  }
}

- (void)mouseUp:(NSEvent*)theEvent {
  // The drag/click is done. If the user dragged the mouse, finalize the drag
  // and clean up.

  // Special-case this to keep the logic below simpler.
  if (moveWindowOnDrag_)
    return;

  // Cancel any delayed -mouseDragged: requests that may still be pending.
  [NSObject cancelPreviousPerformRequestsWithTarget:self];

  // We are now free to re-display the new tab button in the window we're
  // dragging. It will show when the next call to -layoutTabs (which happens
  // indrectly by several of the calls below, such as removing the placeholder).
  [draggedController_ showNewTabButton:YES];

  if (draggingWithinTabStrip_) {
    if (tabWasDragged_) {
      // Move tab to new location.
      DCHECK([sourceController_ numberOfTabs]);
      TabWindowController* dropController = sourceController_;
      [dropController moveTabView:[dropController selectedTabView]
                   fromController:nil];
    }
  } else if (targetController_) {
    // Move between windows. If |targetController_| is nil, we're not dropping
    // into any existing window.
    NSView* draggedTabView = [draggedController_ selectedTabView];
    [draggedController_ removeOverlay];
    [targetController_ moveTabView:draggedTabView
                    fromController:draggedController_];
    [targetController_ showWindow:nil];
  } else {
    // Only move the window around on screen. Make sure it's set back to
    // normal state (fully opaque, has shadow, has key, etc).
    [draggedController_ removeOverlay];
    // Don't want to re-show the window if it was closed during the drag.
    if ([dragWindow_ isVisible]) {
      [dragWindow_ setAlphaValue:1.0];
      [dragOverlay_ setHasShadow:NO];
      [dragWindow_ setHasShadow:YES];
      [dragWindow_ makeKeyAndOrderFront:nil];
    }
    [[draggedController_ window] setLevel:NSNormalWindowLevel];
    [draggedController_ removePlaceholder];
  }
  [sourceController_ removePlaceholder];
  chromeIsVisible_ = YES;

  [self resetDragControllers];
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  // Support middle-click-to-close.
  if ([theEvent buttonNumber] == 2) {
    // |-hitTest:| takes a location in the superview's coordinates.
    NSPoint upLocation =
        [[self superview] convertPoint:[theEvent locationInWindow]
                              fromView:nil];
    // If the mouse up occurred in our view or over the close button, then
    // close.
    if ([self hitTest:upLocation])
      [controller_ closeTab:self];
  }
}

- (void)drawRect:(NSRect)rect {
  NSGraphicsContext* context = [NSGraphicsContext currentContext];
  [context saveGraphicsState];
  rect = [self bounds];
  BOOL active = [[self window] isKeyWindow] || [[self window] isMainWindow];
  BOOL selected = [(NSButton*)self state];

  // Inset by 0.5 in order to draw on pixels rather than on borders (which would
  // cause blurry pixels). Decrease height by 1 in order to move away from the
  // edge for the dark shadow.
  rect = NSInsetRect(rect, -0.5, -0.5);
  rect.origin.y -= 1;

  NSPoint bottomLeft = NSMakePoint(NSMinX(rect), NSMinY(rect) + 2);
  NSPoint bottomRight = NSMakePoint(NSMaxX(rect), NSMinY(rect) + 2);
  NSPoint topRight =
      NSMakePoint(NSMaxX(rect) - kInsetMultiplier * NSHeight(rect),
                  NSMaxY(rect));
  NSPoint topLeft =
      NSMakePoint(NSMinX(rect)  + kInsetMultiplier * NSHeight(rect),
                  NSMaxY(rect));

  float baseControlPointOutset = NSHeight(rect) * kControlPoint1Multiplier;
  float bottomControlPointInset = NSHeight(rect) * kControlPoint2Multiplier;

  // Outset many of these values by 1 to cause the fill to bleed outside the
  // clip area.
  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:NSMakePoint(bottomLeft.x - 1, bottomLeft.y - 2)];
  [path lineToPoint:NSMakePoint(bottomLeft.x - 1, bottomLeft.y)];
  [path lineToPoint:bottomLeft];
  [path curveToPoint:topLeft
       controlPoint1:NSMakePoint(bottomLeft.x + baseControlPointOutset,
                                 bottomLeft.y)
       controlPoint2:NSMakePoint(topLeft.x - bottomControlPointInset,
                                 topLeft.y)];
  [path lineToPoint:topRight];
  [path curveToPoint:bottomRight
       controlPoint1:NSMakePoint(topRight.x + bottomControlPointInset,
                                 topRight.y)
       controlPoint2:NSMakePoint(bottomRight.x - baseControlPointOutset,
                                 bottomRight.y)];
  [path lineToPoint:NSMakePoint(bottomRight.x + 1, bottomRight.y)];
  [path lineToPoint:NSMakePoint(bottomRight.x + 1, bottomRight.y - 2)];

  GTMTheme* theme = [self gtm_theme];

  // Setting the pattern phase
  NSPoint phase = [self gtm_themePatternPhase];
  [context setPatternPhase:phase];

  if (!selected) {
    NSColor* windowColor =
        [theme backgroundPatternColorForStyle:GTMThemeStyleWindow
                                        state:GTMThemeStateActiveWindow];
    if (windowColor) {
      [windowColor set];
    } else {
      [[NSColor windowBackgroundColor] set];
    }

    [path fill];

    NSColor* tabColor =
        [theme backgroundPatternColorForStyle:GTMThemeStyleTabBarDeselected
                                        state:GTMThemeStateActiveWindow];
    if (tabColor) {
      [tabColor set];
    } else {
      [[NSColor colorWithCalibratedWhite:1.0 alpha:0.3] set];
    }
    [path fill];
  }

  [context saveGraphicsState];
  [path addClip];

  if (selected || hoverAlpha_ > 0) {
    // Draw the background.
    CGFloat backgroundAlpha = hoverAlpha_ * 0.5;
    [context saveGraphicsState];
    CGContextRef cgContext =
        (CGContextRef)([context graphicsPort]);
    CGContextBeginTransparencyLayer(cgContext, 0);
    if (!selected)
      CGContextSetAlpha(cgContext, backgroundAlpha);
    [path addClip];
    [context saveGraphicsState];
    [super drawBackground];
    [context restoreGraphicsState];

    // Draw a mouse hover gradient for the default themes
    if (!selected) {
      if (![theme backgroundImageForStyle:GTMThemeStyleTabBarDeselected
                                    state:GTMThemeStateActiveWindow]) {
        scoped_nsobject<NSGradient> glow([NSGradient alloc]);
        [glow initWithStartingColor:[NSColor colorWithCalibratedWhite:1.0
                                                                alpha:1.0 *
                                                                    hoverAlpha_]
                        endingColor:[NSColor colorWithCalibratedWhite:1.0
                                                                alpha:0.0]];

        NSPoint point = hoverPoint_;
        point.y = NSHeight(rect);
        [glow drawFromCenter:point
                      radius:0
                    toCenter:point
                      radius:NSWidth(rect)/3
                     options:NSGradientDrawsBeforeStartingLocation];

        [glow drawInBezierPath:path relativeCenterPosition:hoverPoint_];
      }
    }

    CGContextEndTransparencyLayer(cgContext);
    [context restoreGraphicsState];
  }

  // Draw the top inner highlight.
  NSAffineTransform* highlightTransform = [NSAffineTransform transform];
  [highlightTransform translateXBy:1 yBy:-1];
  scoped_nsobject<NSBezierPath> highlightPath([path copy]);
  [highlightPath transformUsingAffineTransform:highlightTransform];
  [[NSColor colorWithCalibratedWhite:1.0 alpha:0.2 + 0.3 * hoverAlpha_]
      setStroke];
  [highlightPath stroke];

  [context restoreGraphicsState];

  // Draw the top stroke.
  [context saveGraphicsState];
  if (selected) {
    [[NSColor colorWithDeviceWhite:0.0 alpha:active ? 0.3 : 0.15] set];
  } else {
    [[NSColor colorWithDeviceWhite:0.0 alpha:active ? 0.2 : 0.15] set];
    [[NSBezierPath bezierPathWithRect:NSOffsetRect(rect, 0, 2.5)] addClip];
  }
  [path setLineWidth:1.0];
  [path stroke];
  [context restoreGraphicsState];

  // Draw the bottom border.
  if (!selected) {
    [path addClip];
    NSRect borderRect, contentRect;
    NSDivideRect(rect, &borderRect, &contentRect, 2.5, NSMinYEdge);
    [[NSColor colorWithDeviceWhite:0.0 alpha:active ? 0.3 : 0.15] set];
    NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
  }
  [context restoreGraphicsState];
}

// Called when the user hits the right mouse button (or control-clicks) to
// show a context menu.
- (void)rightMouseDown:(NSEvent*)theEvent {
  [NSMenu popUpContextMenu:[self menu] withEvent:theEvent forView:self];
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  if ([self window]) {
    [controller_ updateTitleColor];
  }
}

@end
