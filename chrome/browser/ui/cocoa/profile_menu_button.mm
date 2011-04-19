// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profile_menu_button.h"

#include <algorithm>

#include "base/logging.h"
#import "third_party/GTM/AppKit/GTMFadeTruncatingTextFieldCell.h"

namespace {

const CGFloat kTabWidth = 24;
const CGFloat kTabHeight = 13;
const CGFloat kTabArrowWidth = 7;
const CGFloat kTabArrowHeight = 4;
const CGFloat kTabRoundRectRadius = 5;
const CGFloat kTabDisplayNameMarginY = 3;

NSColor* GetWhiteWithAlpha(CGFloat alpha) {
  return [NSColor colorWithCalibratedWhite:1.0 alpha:alpha];
}

NSColor* GetBlackWithAlpha(CGFloat alpha) {
  return [NSColor colorWithCalibratedWhite:0.0 alpha:alpha];
}

} // namespace

@interface ProfileMenuButton (Private)
- (void)commonInit;
- (NSPoint)popUpMenuPosition;
- (NSImage*)tabImage;
- (NSRect)textFieldRect;
- (NSBezierPath*)tabPathWithRect:(NSRect)rect
                          radius:(CGFloat)radius;
- (NSBezierPath*)downArrowPathWithRect:(NSRect)rect;
- (NSImage*)tabImageWithSize:(NSSize)tabSize
                   fillColor:(NSColor*)fillColor
                   isPressed:(BOOL)isPressed;
@end

@implementation ProfileMenuButton

@synthesize shouldShowProfileDisplayName = shouldShowProfileDisplayName_;

- (void)commonInit {
  textFieldCell_.reset(
      [[GTMFadeTruncatingTextFieldCell alloc] initTextCell:@""]);
  [textFieldCell_ setBackgroundStyle:NSBackgroundStyleRaised];
  [textFieldCell_ setAlignment:NSRightTextAlignment];
  [textFieldCell_ setFont:[NSFont systemFontOfSize:
      [NSFont smallSystemFontSize]]];
}

- (id)initWithFrame:(NSRect)frame
          pullsDown:(BOOL)flag {
  if ((self = [super initWithFrame:frame pullsDown:flag]))
    [self commonInit];
  return self;
}

- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder]))
    [self commonInit];
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (NSString*)profileDisplayName {
  return [textFieldCell_ stringValue];
}

- (void)setProfileDisplayName:(NSString*)name {
  if (![[textFieldCell_ stringValue] isEqual:name]) {
    [textFieldCell_ setStringValue:name];
    [self setNeedsDisplay:YES];
  }
}

- (void)setShouldShowProfileDisplayName:(BOOL)flag {
  shouldShowProfileDisplayName_ = flag;
  [self setNeedsDisplay:YES];
}

- (BOOL)isFlipped {
  return NO;
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  if ([self window] == newWindow)
    return;

  if ([self window]) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowDidBecomeMainNotification
                object:[self window]];
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowDidResignMainNotification
                object:[self window]];
  }

  if (newWindow) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowFocusChanged:)
               name:NSWindowDidBecomeMainNotification
             object:newWindow];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowFocusChanged:)
               name:NSWindowDidResignMainNotification
             object:newWindow];
  }
}

- (void)onWindowFocusChanged:(NSNotification*)note {
  [self setNeedsDisplay:YES];
}

- (NSRect)tabRect {
  NSRect bounds = [self bounds];
  NSRect tabRect;
  tabRect.size.width = kTabWidth;
  tabRect.size.height = kTabHeight;
  tabRect.origin.x = NSMaxX(bounds) - NSWidth(tabRect);
  tabRect.origin.y = NSMaxY(bounds) - NSHeight(tabRect);
  return tabRect;
}

- (NSRect)textFieldRect {
  NSRect bounds = [self bounds];
  NSSize desiredSize = [textFieldCell_ cellSize];

  NSRect textRect = bounds;
  textRect.size.height = std::min(desiredSize.height, NSHeight(bounds));

  // For some reason there's always a 2 pixel gap on the right side of the
  // text field. Fix it by moving the text field to the right by 2 pixels.
  textRect.origin.x += 2;

  return textRect;
}

- (NSView*)hitTest:(NSPoint)aPoint {
  NSView* probe = [super hitTest:aPoint];
  if (probe != self)
    return probe;

  NSPoint viewPoint = [self convertPoint:aPoint fromView:[self superview]];
  BOOL isFlipped = [self isFlipped];
  if (NSMouseInRect(viewPoint, [self tabRect], isFlipped))
    return self;
  else
    return nil;
}

- (NSBezierPath*)tabPathWithRect:(NSRect)rect
                          radius:(CGFloat)radius {
  const NSRect innerRect = NSInsetRect(rect, radius, radius);
  NSBezierPath* path = [NSBezierPath bezierPath];

  // Top left
  [path moveToPoint:NSMakePoint(NSMinX(rect), NSMaxY(rect))];

  // Bottom left
  [path lineToPoint:NSMakePoint(NSMinX(rect), NSMinY(innerRect))];
  [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(innerRect),
                                                      NSMinY(innerRect))
                                   radius:radius
                               startAngle:180
                                 endAngle:270
                                clockwise:NO];

  // Bottom right
  [path lineToPoint:NSMakePoint(NSMaxX(innerRect), NSMinY(rect))];
  [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(innerRect),
                                                      NSMinY(innerRect))
                                   radius:radius
                               startAngle:270
                                 endAngle:360
                                clockwise:NO];

  // Top right
  [path lineToPoint:NSMakePoint(NSMaxX(rect), NSMaxY(rect))];

  [path closePath];
  return path;
}

- (NSBezierPath*)downArrowPathWithRect:(NSRect)rect {
  NSBezierPath* path = [NSBezierPath bezierPath];

  // Top left
  [path moveToPoint:NSMakePoint(NSMinX(rect), NSMaxY(rect))];

  // Bottom middle
  [path lineToPoint:NSMakePoint(NSMidX(rect), NSMinY(rect))];

  // Top right
  [path lineToPoint:NSMakePoint(NSMaxX(rect), NSMaxY(rect))];

  [path closePath];
  return path;
}

- (NSImage*)tabImageWithSize:(NSSize)tabSize
                   fillColor:(NSColor*)fillColor
                   isPressed:(BOOL)isPressed {
  NSImage* image = [[[NSImage alloc] initWithSize:tabSize] autorelease];
  [image lockFocus];

  // White shadow for inset look
  [[NSGraphicsContext currentContext] saveGraphicsState];
  scoped_nsobject<NSShadow> tabShadow([[NSShadow alloc] init]);
  [tabShadow.get() setShadowOffset:NSMakeSize(0, -1)];
  [tabShadow setShadowBlurRadius:0];
  [tabShadow.get() setShadowColor:GetWhiteWithAlpha(0.6)];
  [tabShadow set];

  // Gray outline
  NSRect tabRect = NSMakeRect(0, 1, tabSize.width, tabSize.height - 1);
  NSBezierPath* outlinePath = [self tabPathWithRect:tabRect
                                             radius:kTabRoundRectRadius];
  [[NSColor colorWithCalibratedWhite:0.44 alpha:1.0] set];
  [outlinePath fill];

  [[NSGraphicsContext currentContext] restoreGraphicsState];

  // Fill
  NSRect fillRect = NSInsetRect(tabRect, 1, 0);
  fillRect.size.height -= 1;
  fillRect.origin.y += 1;
  NSBezierPath* fillPath = [self tabPathWithRect:fillRect
                                          radius:kTabRoundRectRadius - 1];
  [fillColor set];
  [fillPath fill];

  // Shading for fill to make the bottom of the tab slightly darker.
  scoped_nsobject<NSGradient> gradient([[NSGradient alloc]
      initWithStartingColor:GetBlackWithAlpha(isPressed ? 0.2 : 0.0)
                endingColor:GetBlackWithAlpha(0.2)]);
  [gradient drawInBezierPath:fillPath angle:270];

  // Highlight on top
  NSRect highlightRect = NSInsetRect(tabRect, 1, 0);
  highlightRect.size.height = 1;
  highlightRect.origin.y = NSMaxY(tabRect) - highlightRect.size.height;
  [GetWhiteWithAlpha(0.5) set];
  NSRectFillUsingOperation(highlightRect, NSCompositeSourceOver);

  // Arrow shadow
  [[NSGraphicsContext currentContext] saveGraphicsState];
  scoped_nsobject<NSShadow> arrowShadow([[NSShadow alloc] init]);
  [arrowShadow.get() setShadowOffset:NSMakeSize(0, -1)];
  [arrowShadow setShadowBlurRadius:0];
  [arrowShadow.get() setShadowColor:GetBlackWithAlpha(0.6)];
  [arrowShadow set];

  // Down arrow
  NSRect arrowRect;
  arrowRect.size.width = kTabArrowWidth;
  arrowRect.size.height = kTabArrowHeight;
  arrowRect.origin.x = NSMinX(tabRect) + roundf((NSWidth(tabRect) -
                                                 NSWidth(arrowRect)) / 2.0);
  arrowRect.origin.y = NSMinY(tabRect) + roundf((tabRect.size.height -
                                                 arrowRect.size.height) / 2.0);
  NSBezierPath* arrowPath = [self downArrowPathWithRect:arrowRect];
  if (isPressed)
    [[NSColor colorWithCalibratedWhite:0.8 alpha:1.0] set];
  else
    [[NSColor whiteColor] set];
  [arrowPath fill];

  [[NSGraphicsContext currentContext] restoreGraphicsState];

  [image unlockFocus];
  return image;
}

- (NSImage*)tabImage {
  BOOL isPressed = [[self cell] isHighlighted];

  // Invalidate the cached image if necessary.
  if (cachedTabImageIsPressed_ != isPressed) {
    cachedTabImageIsPressed_ = isPressed;
    cachedTabImage_.reset();
  }

  if (cachedTabImage_)
    return cachedTabImage_;

  // TODO: Use different colors for different profiles and tint for
  // the current browser theme.
  NSColor* fillColor = [NSColor colorWithCalibratedRed:122.0/255.0
                                                 green:177.0/255.0
                                                  blue:252.0/255.0
                                                 alpha:1.0];
  NSRect tabRect = [self tabRect];
  cachedTabImage_.reset([[self tabImageWithSize:tabRect.size
                                     fillColor:fillColor
                                     isPressed:isPressed] retain]);
  return cachedTabImage_;
}

- (void)drawRect:(NSRect)rect {
  CGFloat alpha = [[self window] isMainWindow] ? 1.0 : 0.5;
  [[self tabImage] drawInRect:[self tabRect]
                     fromRect:NSZeroRect
                    operation:NSCompositeSourceOver
                     fraction:alpha];

  if (shouldShowProfileDisplayName_) {
    NSColor* textColor = [[self window] isMainWindow] ?
        GetBlackWithAlpha(0.6) : GetBlackWithAlpha(0.4);
    if (![[textFieldCell_ textColor] isEqual:textColor])
      [textFieldCell_ setTextColor:textColor];
    [textFieldCell_ drawWithFrame:[self textFieldRect] inView:self];
  }
}

- (NSPoint)popUpMenuPosition {
  NSPoint menuPos = [self tabRect].origin;
  // By default popUpContextMenu: causes the menu to show up a few pixels above
  // the point you give it. We need to shift it down a bit so that it lines up
  // with the bottom of the tab.
  menuPos.y -= 6;
  return [self convertPoint:menuPos toView:nil];
}

- (void)   mouseDown:(NSEvent*)event
  withShowMenuTarget:(id)target {
  if (![self menu]) {
    [super mouseDown:event];
    return;
  }

  NSPoint point = [[self superview]
      convertPointFromBase:[event locationInWindow]];
  if (![[self hitTest:point] isEqual:self])
    return;

  // Draw the control as depressed.
  [self highlight:YES];

  NSEvent* fakeEvent = [NSEvent
      mouseEventWithType:[event type]
                location:[self popUpMenuPosition]
           modifierFlags:[event modifierFlags]
               timestamp:[event timestamp]
            windowNumber:[event windowNumber]
                 context:[event context]
             eventNumber:[event eventNumber]
              clickCount:[event clickCount]
                pressure:[event pressure]];
  DCHECK([target respondsToSelector:
      @selector(popUpContextMenu:withEvent:forView:)]);
  [target popUpContextMenu:[self menu]
                 withEvent:fakeEvent
                   forView:self];

  [self highlight:NO];
}

- (void)mouseDown:(NSEvent*)event {
  [self      mouseDown:event
    withShowMenuTarget:[NSMenu class]];
}

- (NSSize)desiredControlSize {
  NSSize size = [self tabRect].size;

  if (shouldShowProfileDisplayName_) {
    NSSize textFieldSize = [textFieldCell_ cellSize];
    size.width = std::max(size.width, textFieldSize.width);
    size.height += textFieldSize.height + kTabDisplayNameMarginY;
  }

  size.width = ceil(size.width);
  size.height = ceil(size.height);
  return size;
}

- (NSSize)minControlSize {
  return [self tabRect].size;
}

@end
