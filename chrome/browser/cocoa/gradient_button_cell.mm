// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/gradient_button_cell.h"
#import "third_party/GTM/AppKit/GTMTheme.h"
#import "base/scoped_nsobject.h"

@interface GradientButtonCell (Private)
- (void)drawUnderlayImageWithFrame:(NSRect)cellFrame
                            inView:(NSView*)controlView;
@end


@implementation GradientButtonCell

// For nib instantiations
- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    shouldTheme_ = YES;
  }
  return self;
}

// For programmatic instantiations
- (id)initTextCell:(NSString*)string {
  if ((self = [super initTextCell:string])) {
    shouldTheme_ = YES;
  }
  return self;
}

- (void)setShouldTheme:(BOOL)shouldTheme {
  shouldTheme_ = shouldTheme;
}

- (NSImage*)underlayImage {
  return underlayImage_;
}

- (void)setUnderlayImage:(NSImage*)image {
  underlayImage_.reset([image retain]);

  [[self controlView] setNeedsDisplay:YES];
}

- (NSBackgroundStyle)interiorBackgroundStyle {
  return [self isHighlighted] ?
      NSBackgroundStyleLowered : NSBackgroundStyleRaised;
}

- (void)mouseEntered:(NSEvent *)theEvent {
  isMouseInside_ = YES;
  [[self controlView] setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent *)theEvent {
  isMouseInside_ = NO;
  [[self controlView] setNeedsDisplay:YES];
}

- (BOOL)isMouseInside {
  return trackingArea_ && isMouseInside_;
}

// Since we have our own drawWithFrame:, we need to also have our own
// logic for determining when the mouse is inside for honoring this
// request.
- (void)setShowsBorderOnlyWhileMouseInside:(BOOL)showOnly {
  [super setShowsBorderOnlyWhileMouseInside:showOnly];
  if (showOnly) {
    if (trackingArea_.get()) {
      [self setShowsBorderOnlyWhileMouseInside:NO];
    }
    trackingArea_.reset([[NSTrackingArea alloc]
                          initWithRect:[[self controlView]
                                         bounds]
                               options:(NSTrackingMouseEnteredAndExited |
                                        NSTrackingActiveInActiveApp)
                                 owner:self
                            userInfo:nil]);
    [[self controlView] addTrackingArea:trackingArea_];
  } else {
    if (trackingArea_) {
      [[self controlView] removeTrackingArea:trackingArea_];
      trackingArea_.reset(nil);
      isMouseInside_ = NO;
    }
  }
}

- (void)drawBorderAndFillForTheme:(GTMTheme*)theme
                      controlView:(NSView*)controlView
                        outerPath:(NSBezierPath*)outerPath
                        innerPath:(NSBezierPath*)innerPath
            showHighlightGradient:(BOOL)showHighlightGradient
              showClickedGradient:(BOOL)showClickedGradient
                           active:(BOOL)active
                        cellFrame:(NSRect)cellFrame {
  [[NSColor colorWithCalibratedWhite:1.0 alpha:0.25] set];
  [outerPath stroke];

  NSImage* backgroundImage =
      [theme backgroundImageForStyle:GTMThemeStyleToolBarButton state:YES];

  if (backgroundImage) {
    NSColor* patternColor = [NSColor colorWithPatternImage:backgroundImage];
    [patternColor set];
    // Set the phase to match window.
    NSRect trueRect = [controlView convertRectToBase:cellFrame];
    [[NSGraphicsContext currentContext]
        setPatternPhase:NSMakePoint(NSMinX(trueRect), NSMaxY(trueRect))];
    [innerPath fill];
  } else {
    if (showClickedGradient) {
      NSGradient* gradient =
          [theme gradientForStyle:GTMThemeStyleToolBarButtonPressed
                            state:active];
      [gradient drawInBezierPath:innerPath angle:90.0];
    }
  }

  if (!showClickedGradient && showHighlightGradient) {
    [NSGraphicsContext saveGraphicsState];
    [innerPath addClip];

    // Draw the inner glow.
    [innerPath setLineWidth:2];
    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.9] setStroke];
    [innerPath stroke];

    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.9] setStroke];
    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.2] setFill];

    // Draw the top inner highlight.
    NSAffineTransform* highlightTransform = [NSAffineTransform transform];
    [highlightTransform translateXBy:1 yBy:1];
    scoped_nsobject<NSBezierPath> highlightPath([innerPath copy]);
    [highlightPath transformUsingAffineTransform:highlightTransform];

    [highlightPath stroke];

    NSColor* startColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.666];
    NSColor* endColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.333];
    scoped_nsobject<NSBezierPath> gradient([[NSGradient alloc]
        initWithColorsAndLocations:startColor, 0.33, endColor, 1.0, nil]);

    [gradient drawInBezierPath:innerPath angle:90.0];

    [NSGraphicsContext restoreGraphicsState];
  }

  NSColor* stroke = [theme strokeColorForStyle:GTMThemeStyleToolBarButton
                                         state:active];
  [stroke setStroke];

  [innerPath setLineWidth:1];
  [innerPath stroke];
}


- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  // Constants from Cole.  Will kConstant them once the feedback loop
  // is complete.
  NSRect drawFrame = NSInsetRect(cellFrame, 1.5, 1.5);
  NSRect innerFrame = NSInsetRect(cellFrame, 2, 2);
  ButtonType type = [[(NSControl*)controlView cell] tag];
  switch (type) {
    case kMiddleButtonType:
      drawFrame.size.width += 20;
      innerFrame.size.width += 2;
      // Fallthrough
    case kRightButtonType:
      drawFrame.origin.x -= 20;
      innerFrame.origin.x -= 2;
      // Fallthrough
    case kLeftButtonType:
    case kLeftButtonWithShadowType:
      drawFrame.size.width += 20;
      innerFrame.size.width += 2;
    default:
      break;
  }

  const float radius = 3.5;
  BOOL pressed = [self isHighlighted];
  NSWindow* window = [controlView window];
  BOOL active = [window isKeyWindow] || [window isMainWindow];

  GTMTheme *theme = [controlView gtm_theme];

  NSBezierPath* innerPath =
      [NSBezierPath bezierPathWithRoundedRect:drawFrame
                                      xRadius:radius
                                      yRadius:radius];
  NSBezierPath* outerPath =
      [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(drawFrame, -1, -1)
                                      xRadius:radius + 1
                                      yRadius:radius + 1];

  // Stroke the borders and appropriate fill gradient. If we're borderless,
  // the only time we want to draw the inner gradient is if we're highlighted.
  if (([self isBordered] && ![self showsBorderOnlyWhileMouseInside]) ||
      pressed ||
      [self isMouseInside]) {

    [self drawBorderAndFillForTheme:theme
                        controlView:controlView
                          outerPath:outerPath
                          innerPath:innerPath
              showHighlightGradient:YES
                showClickedGradient:pressed
                             active:active
                          cellFrame:cellFrame];
  }

  // If this is the left side of a segmented button, draw a slight shadow.
  if (type == kLeftButtonWithShadowType) {
    NSRect borderRect, contentRect;
    NSDivideRect(cellFrame, &borderRect, &contentRect, 1.0, NSMaxXEdge);
    NSColor* stroke = [theme strokeColorForStyle:GTMThemeStyleToolBarButton
                                           state:active];
    [[stroke colorWithAlphaComponent:0.2] set];
    NSRectFillUsingOperation(NSInsetRect(borderRect, 0, 2),
                             NSCompositeSourceOver);
    innerFrame.size.width -= 1.0;
  }
  [self drawInteriorWithFrame:innerFrame inView:controlView];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  GTMTheme* theme = [controlView gtm_theme];

  if (shouldTheme_) {
    BOOL isTemplate = [[self image] isTemplate];

    [NSGraphicsContext saveGraphicsState];

    CGContextRef context =
        (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);

    if (isTemplate) {
      scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
      [shadow setShadowColor:[NSColor whiteColor]];
      [shadow setShadowOffset:NSMakeSize(0, -1.0)];
      [shadow setShadowBlurRadius:1.0];
      [shadow set];
    }

    [self drawUnderlayImageWithFrame:cellFrame inView:controlView];

    CGContextBeginTransparencyLayer(context, 0);
    NSRect imageRect = NSZeroRect;
    imageRect.size = [[self image] size];
    [[self image] setFlipped:[controlView isFlipped]];
    [[self image] drawInRect:[self imageRectForBounds:cellFrame]
                    fromRect:imageRect
                   operation:NSCompositeSourceOver
                    fraction:[self isEnabled] ? 1.0 : 0.5];
    if (isTemplate) {
      NSColor* color = [theme iconColorForStyle:GTMThemeStyleToolBarButton
                                          state:YES];
      if (color) {
        [color set];
        NSRectFillUsingOperation(cellFrame, NSCompositeSourceAtop);
      }
    }

    CGContextEndTransparencyLayer(context);
    [NSGraphicsContext restoreGraphicsState];
  } else {
    [self drawUnderlayImageWithFrame:cellFrame inView:controlView];

    // NSCell draws these uncentered for some reason, probably because of the
    // of control in the xib
    [super drawInteriorWithFrame:NSOffsetRect(cellFrame, 0, 1)
                          inView:controlView];
  }
}

- (void)drawUnderlayImageWithFrame:(NSRect)cellFrame
                            inView:(NSView*)controlView {
  if (underlayImage_) {
    NSRect imageRect = NSZeroRect;
    imageRect.size = [underlayImage_ size];
    [underlayImage_ setFlipped:[controlView isFlipped]];
    [underlayImage_ drawInRect:[self imageRectForBounds:cellFrame]
                      fromRect:imageRect
                     operation:NSCompositeSourceOver
                      fraction:[self isEnabled] ? 1.0 : 0.5];
  }
}

@end
