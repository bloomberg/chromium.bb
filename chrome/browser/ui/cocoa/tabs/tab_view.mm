// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_view.h"

#include "base/logging.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// Constants for inset and control points for tab shape.
const CGFloat kInsetMultiplier = 2.0/3.0;
const CGFloat kControlPoint1Multiplier = 1.0/3.0;
const CGFloat kControlPoint2Multiplier = 3.0/8.0;

// The amount of time in seconds during which each type of glow increases, holds
// steady, and decreases, respectively.
const NSTimeInterval kHoverShowDuration = 0.2;
const NSTimeInterval kHoverHoldDuration = 0.02;
const NSTimeInterval kHoverHideDuration = 0.4;
const NSTimeInterval kAlertShowDuration = 0.4;
const NSTimeInterval kAlertHoldDuration = 0.4;
const NSTimeInterval kAlertHideDuration = 0.4;

// The default time interval in seconds between glow updates (when
// increasing/decreasing).
const NSTimeInterval kGlowUpdateInterval = 0.025;

// This is used to judge whether the mouse has moved during rapid closure; if it
// has moved less than the threshold, we want to close the tab.
const CGFloat kRapidCloseDist = 2.5;

}  // namespace

@interface TabView(Private)

- (void)resetLastGlowUpdateTime;
- (NSTimeInterval)timeElapsedSinceLastGlowUpdate;
- (void)adjustGlowValue;
- (NSBezierPath*)bezierPathForRect:(NSRect)rect;
- (NSBezierPath*)topHighlightBezierPathForRect:(NSRect)rect;

@end  // TabView(Private)

@implementation TabView

@synthesize state = state_;
@synthesize hoverAlpha = hoverAlpha_;
@synthesize alertAlpha = alertAlpha_;
@synthesize closing = closing_;

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setShowsDivider:NO];
    // TODO(alcor): register for theming
  }
  return self;
}

- (void)awakeFromNib {
  [self setShowsDivider:NO];
}

- (void)dealloc {
  // Cancel any delayed requests that may still be pending (drags or hover).
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [super dealloc];
}

// Called to obtain the context menu for when the user hits the right mouse
// button (or control-clicks). (Note that -rightMouseDown: is *not* called for
// control-click.)
- (NSMenu*)menu {
  if ([self isClosing])
    return nil;

  // Sheets, being window-modal, should block contextual menus. For some reason
  // they do not. Disallow them ourselves.
  if ([[self window] attachedSheet])
    return nil;

  return [controller_ menu];
}

// Overridden so that mouse clicks come to this view (the parent of the
// hierarchy) first. We want to handle clicks and drags in this class and
// leave the background button for display purposes only.
- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return YES;
}

- (void)mouseEntered:(NSEvent*)theEvent {
  isMouseInside_ = YES;
  [self resetLastGlowUpdateTime];
  [self adjustGlowValue];
}

- (void)mouseMoved:(NSEvent*)theEvent {
  hoverPoint_ = [self convertPoint:[theEvent locationInWindow]
                          fromView:nil];
  [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)theEvent {
  isMouseInside_ = NO;
  hoverHoldEndTime_ =
      [NSDate timeIntervalSinceReferenceDate] + kHoverHoldDuration;
  [self resetLastGlowUpdateTime];
  [self adjustGlowValue];
}

- (void)setTrackingEnabled:(BOOL)enabled {
  if (![closeButton_ isHidden]) {
    [closeButton_ setTrackingEnabled:enabled];
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
  return [controller_ tabCanBeDragged:controller_];
}

// Handle clicks and drags in this button. We get here because we have
// overridden acceptsFirstMouse: and the click is within our bounds.
- (void)mouseDown:(NSEvent*)theEvent {
  if ([self isClosing])
    return;

  // Record the point at which this event happened. This is used by other mouse
  // events that are dispatched from |-maybeStartDrag::|.
  mouseDownPoint_ = [theEvent locationInWindow];

  // Record the state of the close button here, because selecting the tab will
  // unhide it.
  BOOL closeButtonActive = ![closeButton_ isHidden];

  // During the tab closure animation (in particular, during rapid tab closure),
  // we may get incorrectly hit with a mouse down. If it should have gone to the
  // close button, we send it there -- it should then track the mouse, so we
  // don't have to worry about mouse ups.
  if (closeButtonActive && [controller_ inRapidClosureMode]) {
    NSPoint hitLocation = [[self superview] convertPoint:mouseDownPoint_
                                                fromView:nil];
    if ([self hitTest:hitLocation] == closeButton_) {
      [closeButton_ mouseDown:theEvent];
      return;
    }
  }

  // If the tab gets torn off, the tab controller will be removed from the tab
  // strip and then deallocated. This will also result in *us* being
  // deallocated. Both these are bad, so we prevent this by retaining the
  // controller.
  scoped_nsobject<TabController> controller([controller_ retain]);

  // Try to initiate a drag. This will spin a custom event loop and may
  // dispatch other mouse events.
  [controller_ maybeStartDrag:theEvent forTab:controller];

  // The custom loop has ended, so clear the point.
  mouseDownPoint_ = NSZeroPoint;
}

- (void)mouseUp:(NSEvent*)theEvent {
  // Check for rapid tab closure.
  if ([theEvent type] == NSLeftMouseUp) {
    NSPoint upLocation = [theEvent locationInWindow];
    CGFloat dx = upLocation.x - mouseDownPoint_.x;
    CGFloat dy = upLocation.y - mouseDownPoint_.y;

    // During rapid tab closure (mashing tab close buttons), we may get hit
    // with a mouse down. As long as the mouse up is over the close button,
    // and the mouse hasn't moved too much, we close the tab.
    if (![closeButton_ isHidden] &&
        (dx*dx + dy*dy) <= kRapidCloseDist*kRapidCloseDist &&
        [controller_ inRapidClosureMode]) {
      NSPoint hitLocation =
          [[self superview] convertPoint:[theEvent locationInWindow]
                                fromView:nil];
      if ([self hitTest:hitLocation] == closeButton_) {
        [controller_ closeTab:self];
        return;
      }
    }
  }

  // Fire the action to select the tab.
  if ([[controller_ target] respondsToSelector:[controller_ action]])
    [[controller_ target] performSelector:[controller_ action]
                               withObject:self];

  // Messaging the drag controller with |-endDrag:| would seem like the right
  // thing to do here. But, when a tab has been detached, the controller's
  // target is nil until the drag is finalized. Since |-mouseUp:| gets called
  // via the manual event loop inside -[TabStripDragController
  // maybeStartDrag:forTab:], the drag controller can end the dragging session
  // itself directly after calling this.
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  if ([self isClosing])
    return;

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

- (void)drawRect:(NSRect)dirtyRect {
  const CGFloat lineWidth = [self cr_lineWidth];

  gfx::ScopedNSGraphicsContextSaveGState scopedGState;
  NSGraphicsContext* context = [NSGraphicsContext currentContext];

  ThemeService* themeProvider =
      static_cast<ThemeService*>([[self window] themeProvider]);
  [context setPatternPhase:[[self window] themePatternPhase]];

  NSRect rect = [self bounds];
  NSBezierPath* path = [self bezierPathForRect:rect];

  BOOL selected = [self state];
  // Don't draw the window/tab bar background when selected, since the tab
  // background overlay drawn over it (see below) will be fully opaque.
  BOOL hasBackgroundImage = NO;
  if (!selected) {
    // ui::ThemeProvider::HasCustomImage is true only if the theme provides the
    // image. However, even if the theme doesn't provide a tab background, the
    // theme machinery will make one if given a frame image. See
    // BrowserThemePack::GenerateTabBackgroundImages for details.
    hasBackgroundImage = themeProvider &&
        (themeProvider->HasCustomImage(IDR_THEME_TAB_BACKGROUND) ||
         themeProvider->HasCustomImage(IDR_THEME_FRAME));

    NSColor* backgroundImageColor = hasBackgroundImage ?
        themeProvider->GetNSImageColorNamed(IDR_THEME_TAB_BACKGROUND, true) :
        nil;

    if (backgroundImageColor) {
      [backgroundImageColor set];
      [path fill];
    } else {
      // Use the window's background color rather than |[NSColor
      // windowBackgroundColor]|, which gets confused by the fullscreen window.
      // (The result is the same for normal, non-fullscreen windows.)
      [[[self window] backgroundColor] set];
      [path fill];

      gfx::ScopedNSGraphicsContextSaveGState drawBackgroundState;
      NSGraphicsContext* context = [NSGraphicsContext currentContext];
      CGContextRef cgContext =
          static_cast<CGContextRef>([context graphicsPort]);
      CGContextBeginTransparencyLayer(cgContext, 0);
      CGContextSetAlpha(cgContext, 0.5);
      [path addClip];
      [super drawBackgroundWithOpaque:NO];
      CGContextEndTransparencyLayer(cgContext);
    }
  }

  BOOL active = [[self window] isKeyWindow] || [[self window] isMainWindow];
  CGFloat borderAlpha = selected ? (active ? 0.3 : 0.2) : 0.2;
  NSColor* borderColor = [NSColor colorWithDeviceWhite:0.0 alpha:borderAlpha];
  NSColor* highlightColor = themeProvider ? themeProvider->GetNSColor(
      themeProvider->UsingDefaultTheme() ?
          ThemeService::COLOR_TOOLBAR_BEZEL :
          ThemeService::COLOR_TOOLBAR, true) : nil;

  {
    gfx::ScopedNSGraphicsContextSaveGState contextSave;
    [path addClip];

    // Use the same overlay for the selected state and for hover and alert
    // glows; for the selected state, it's fully opaque.
    CGFloat hoverAlpha = [self hoverAlpha];
    CGFloat alertAlpha = [self alertAlpha];
    if (selected || hoverAlpha > 0 || alertAlpha > 0) {
      // Draw the selected background / glow overlay.
      gfx::ScopedNSGraphicsContextSaveGState drawHoverState;
      NSGraphicsContext* context = [NSGraphicsContext currentContext];
      CGContextRef cgContext =
          static_cast<CGContextRef>([context graphicsPort]);
      CGContextBeginTransparencyLayer(cgContext, 0);
      if (!selected) {
        // The alert glow overlay is like the selected state but at most at most
        // 80% opaque. The hover glow brings up the overlay's opacity at most
        // 50%.
        CGFloat backgroundAlpha = 0.8 * alertAlpha;
        backgroundAlpha += (1 - backgroundAlpha) * 0.5 * hoverAlpha;
        CGContextSetAlpha(cgContext, backgroundAlpha);
      }
      [path addClip];
      {
        gfx::ScopedNSGraphicsContextSaveGState drawBackgroundState;
        [super drawBackgroundWithOpaque:NO];
      }

      // Draw a mouse hover gradient for the default themes.
      if (!selected && hoverAlpha > 0) {
        if (themeProvider && !hasBackgroundImage) {
          scoped_nsobject<NSGradient> glow([NSGradient alloc]);
          [glow initWithStartingColor:[NSColor colorWithCalibratedWhite:1.0
                                          alpha:1.0 * hoverAlpha]
                          endingColor:[NSColor colorWithCalibratedWhite:1.0
                                                                  alpha:0.0]];

          NSPoint point = hoverPoint_;
          point.y = NSHeight(rect);
          [glow drawFromCenter:point
                        radius:0.0
                      toCenter:point
                        radius:NSWidth(rect) / 3.0
                       options:NSGradientDrawsBeforeStartingLocation];

          [glow drawInBezierPath:path relativeCenterPosition:hoverPoint_];
        }
      }

      CGContextEndTransparencyLayer(cgContext);
    }

    // Draw the top inner highlight within the tab if using the default theme.
    if (themeProvider && themeProvider->UsingDefaultTheme()) {
      NSAffineTransform* highlightTransform = [NSAffineTransform transform];
      [highlightTransform translateXBy:lineWidth yBy:-lineWidth];
      if (selected) {
        scoped_nsobject<NSBezierPath> highlightPath([path copy]);
        [highlightPath transformUsingAffineTransform:highlightTransform];
        [highlightColor setStroke];
        [highlightPath setLineWidth:lineWidth];
        [highlightPath stroke];
        highlightTransform = [NSAffineTransform transform];
        [highlightTransform translateXBy:-2 * lineWidth yBy:0.0];
        [highlightPath transformUsingAffineTransform:highlightTransform];
        [highlightPath stroke];
      } else {
        NSBezierPath* topHighlightPath =
            [self topHighlightBezierPathForRect:[self bounds]];
        [topHighlightPath transformUsingAffineTransform:highlightTransform];
        [highlightColor setStroke];
        [topHighlightPath setLineWidth:lineWidth];
        [topHighlightPath stroke];
      }
    }
  }

  // Draw the top stroke.
  {
    gfx::ScopedNSGraphicsContextSaveGState drawBorderState;
    [borderColor set];
    [path setLineWidth:lineWidth];
    [path stroke];
  }

  // Mimic the tab strip's bottom border, which consists of a dark border
  // and light highlight.
  if (![controller_ active]) {
    [path addClip];
    NSRect borderRect = rect;
    borderRect.origin.y = lineWidth;
    borderRect.size.height = lineWidth;
    [borderColor set];
    NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);

    borderRect.origin.y = 0;
    [highlightColor set];
    NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
  }
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  if ([self window]) {
    [controller_ updateTitleColor];
  }
}

- (void)setClosing:(BOOL)closing {
  closing_ = closing;  // Safe because the property is nonatomic.
  // When closing, ensure clicks to the close button go nowhere.
  if (closing) {
    [closeButton_ setTarget:nil];
    [closeButton_ setAction:nil];
  }
}

- (void)startAlert {
  // Do not start a new alert while already alerting or while in a decay cycle.
  if (alertState_ == tabs::kAlertNone) {
    alertState_ = tabs::kAlertRising;
    [self resetLastGlowUpdateTime];
    [self adjustGlowValue];
  }
}

- (void)cancelAlert {
  if (alertState_ != tabs::kAlertNone) {
    alertState_ = tabs::kAlertFalling;
    alertHoldEndTime_ =
        [NSDate timeIntervalSinceReferenceDate] + kGlowUpdateInterval;
    [self resetLastGlowUpdateTime];
    [self adjustGlowValue];
  }
}

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (NSArray*)accessibilityActionNames {
  NSArray* parentActions = [super accessibilityActionNames];

  return [parentActions arrayByAddingObject:NSAccessibilityPressAction];
}

- (NSArray*)accessibilityAttributeNames {
  NSMutableArray* attributes =
      [[super accessibilityAttributeNames] mutableCopy];
  [attributes addObject:NSAccessibilityTitleAttribute];
  [attributes addObject:NSAccessibilityEnabledAttribute];

  return attributes;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  if ([attribute isEqual:NSAccessibilityTitleAttribute])
    return NO;

  if ([attribute isEqual:NSAccessibilityEnabledAttribute])
    return NO;

  return [super accessibilityIsAttributeSettable:attribute];
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqual:NSAccessibilityRoleAttribute])
    return l10n_util::GetNSStringWithFixup(IDS_ACCNAME_TAB);

  if ([attribute isEqual:NSAccessibilityTitleAttribute])
    return [controller_ title];

  if ([attribute isEqual:NSAccessibilityEnabledAttribute])
    return [NSNumber numberWithBool:YES];

  return [super accessibilityAttributeValue:attribute];
}

- (ViewID)viewID {
  return VIEW_ID_TAB;
}

@end  // @implementation TabView

@implementation TabView (TabControllerInterface)

- (void)setController:(TabController*)controller {
  controller_ = controller;
}

@end  // @implementation TabView (TabControllerInterface)

@implementation TabView(Private)

- (void)resetLastGlowUpdateTime {
  lastGlowUpdate_ = [NSDate timeIntervalSinceReferenceDate];
}

- (NSTimeInterval)timeElapsedSinceLastGlowUpdate {
  return [NSDate timeIntervalSinceReferenceDate] - lastGlowUpdate_;
}

- (void)adjustGlowValue {
  // A time interval long enough to represent no update.
  const NSTimeInterval kNoUpdate = 1000000;

  // Time until next update for either glow.
  NSTimeInterval nextUpdate = kNoUpdate;

  NSTimeInterval elapsed = [self timeElapsedSinceLastGlowUpdate];
  NSTimeInterval currentTime = [NSDate timeIntervalSinceReferenceDate];

  // TODO(viettrungluu): <http://crbug.com/30617> -- split off the stuff below
  // into a pure function and add a unit test.

  CGFloat hoverAlpha = [self hoverAlpha];
  if (isMouseInside_) {
    // Increase hover glow until it's 1.
    if (hoverAlpha < 1) {
      hoverAlpha = MIN(hoverAlpha + elapsed / kHoverShowDuration, 1);
      [self setHoverAlpha:hoverAlpha];
      nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
    }  // Else already 1 (no update needed).
  } else {
    if (currentTime >= hoverHoldEndTime_) {
      // No longer holding, so decrease hover glow until it's 0.
      if (hoverAlpha > 0) {
        hoverAlpha = MAX(hoverAlpha - elapsed / kHoverHideDuration, 0);
        [self setHoverAlpha:hoverAlpha];
        nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
      }  // Else already 0 (no update needed).
    } else {
      // Schedule update for end of hold time.
      nextUpdate = MIN(hoverHoldEndTime_ - currentTime, nextUpdate);
    }
  }

  CGFloat alertAlpha = [self alertAlpha];
  if (alertState_ == tabs::kAlertRising) {
    // Increase alert glow until it's 1 ...
    alertAlpha = MIN(alertAlpha + elapsed / kAlertShowDuration, 1);
    [self setAlertAlpha:alertAlpha];

    // ... and having reached 1, switch to holding.
    if (alertAlpha >= 1) {
      alertState_ = tabs::kAlertHolding;
      alertHoldEndTime_ = currentTime + kAlertHoldDuration;
      nextUpdate = MIN(kAlertHoldDuration, nextUpdate);
    } else {
      nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
    }
  } else if (alertState_ != tabs::kAlertNone) {
    if (alertAlpha > 0) {
      if (currentTime >= alertHoldEndTime_) {
        // Stop holding, then decrease alert glow (until it's 0).
        if (alertState_ == tabs::kAlertHolding) {
          alertState_ = tabs::kAlertFalling;
          nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
        } else {
          DCHECK_EQ(tabs::kAlertFalling, alertState_);
          alertAlpha = MAX(alertAlpha - elapsed / kAlertHideDuration, 0);
          [self setAlertAlpha:alertAlpha];
          nextUpdate = MIN(kGlowUpdateInterval, nextUpdate);
        }
      } else {
        // Schedule update for end of hold time.
        nextUpdate = MIN(alertHoldEndTime_ - currentTime, nextUpdate);
      }
    } else {
      // Done the alert decay cycle.
      alertState_ = tabs::kAlertNone;
    }
  }

  if (nextUpdate < kNoUpdate)
    [self performSelector:_cmd withObject:nil afterDelay:nextUpdate];

  [self resetLastGlowUpdateTime];
  [self setNeedsDisplay:YES];
}

// Returns the bezier path used to draw the tab given the bounds to draw it in.
- (NSBezierPath*)bezierPathForRect:(NSRect)rect {
  const CGFloat lineWidth = [self cr_lineWidth];
  const CGFloat halfLineWidth = lineWidth / 2.0;

  // Outset by halfLineWidth in order to draw on pixels rather than on borders
  // (which would cause blurry pixels). Subtract lineWidth of height to
  // compensate, otherwise clipping will occur.
  rect = NSInsetRect(rect, -halfLineWidth, -halfLineWidth);
  rect.size.height -= lineWidth;

  NSPoint bottomLeft = NSMakePoint(NSMinX(rect), NSMinY(rect) + 2 * lineWidth);
  NSPoint bottomRight = NSMakePoint(NSMaxX(rect), NSMinY(rect) + 2 * lineWidth);
  NSPoint topRight =
      NSMakePoint(NSMaxX(rect) - kInsetMultiplier * NSHeight(rect),
                  NSMaxY(rect));
  NSPoint topLeft =
      NSMakePoint(NSMinX(rect)  + kInsetMultiplier * NSHeight(rect),
                  NSMaxY(rect));

  CGFloat baseControlPointOutset = NSHeight(rect) * kControlPoint1Multiplier;
  CGFloat bottomControlPointInset = NSHeight(rect) * kControlPoint2Multiplier;

  // Outset many of these values by lineWidth to cause the fill to bleed outside
  // the clip area.
  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:NSMakePoint(bottomLeft.x - lineWidth,
                                bottomLeft.y - (2 * lineWidth))];
  [path lineToPoint:NSMakePoint(bottomLeft.x - lineWidth, bottomLeft.y)];
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
  [path lineToPoint:NSMakePoint(bottomRight.x + lineWidth, bottomRight.y)];
  [path lineToPoint:NSMakePoint(bottomRight.x + lineWidth,
                                bottomRight.y - (2 * lineWidth))];
  return path;
}

- (NSBezierPath*)topHighlightBezierPathForRect:(NSRect)rect {
  // Outset by 0.5 in order to draw on pixels rather than on borders (which
  // would cause blurry pixels). Subtract 1px of height to compensate, otherwise
  // clipping will occur.
  rect = NSInsetRect(rect, -0.5, -0.5);
  rect.size.height -= 1.0;

  NSPoint topRight =
      NSMakePoint(NSMaxX(rect) - kInsetMultiplier * NSHeight(rect),
                  NSMaxY(rect));
  NSPoint topLeft =
      NSMakePoint(NSMinX(rect)  + kInsetMultiplier * NSHeight(rect),
                  NSMaxY(rect));

  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:topLeft];
  [path lineToPoint:topRight];
  return path;
}

@end  // @implementation TabView(Private)
