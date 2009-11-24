// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/gradient_button_cell.h"
#import "third_party/GTM/AppKit/GTMTheme.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"

@interface GradientButtonCell (Private)
- (void)sharedInit;
- (void)drawUnderlayImageWithFrame:(NSRect)cellFrame
                            inView:(NSView*)controlView;
@end

static const NSTimeInterval kAnimationShowDuration = 0.2;
static const NSTimeInterval kAnimationHideDuration = 0.4;

@implementation GradientButtonCell
@synthesize hoverAlpha = hoverAlpha_;

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

  [[self controlView] setNeedsDisplay:YES];
}

- (void)setMouseInside:(BOOL)flag animate:(BOOL)animated {
  isMouseInside_ = flag;
  if (animated) {
    lastHoverUpdate_ = [NSDate timeIntervalSinceReferenceDate];
    [self adjustHoverValue];
  } else {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self setHoverAlpha:flag ? 1.0 : 0.0];
  }
  [[self controlView] setNeedsDisplay:YES];
}

// For nib instantiations
- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    [self sharedInit];
  }
  return self;
}

// For programmatic instantiations
- (id)initTextCell:(NSString*)string {
  if ((self = [super initTextCell:string])) {
    [self sharedInit];
  }
  return self;
}

- (NSGradient*)gradientForHoverAlpha:(CGFloat)hoverAlpha
                            isThemed:(BOOL)themed {
  CGFloat startAlpha = 0.6 + 0.3 * hoverAlpha;
  CGFloat endAlpha = 0.333 * hoverAlpha;

  if (themed) {
    startAlpha = 0.2 + 0.35 * hoverAlpha;
    endAlpha = 0.333 * hoverAlpha;
  }

  NSColor* startColor =
      [NSColor colorWithCalibratedWhite:1.0
                                  alpha:startAlpha];
  NSColor* endColor =
      [NSColor colorWithCalibratedWhite:1.0 - 0.15 * hoverAlpha
                                  alpha:endAlpha];
  NSGradient* gradient = [[NSGradient alloc] initWithColorsAndLocations:
                          startColor, hoverAlpha * 0.33,
                          endColor, 1.0, nil];

  return [gradient autorelease];
}

- (void)sharedInit {
  shouldTheme_ = YES;
  gradient_.reset([[self gradientForHoverAlpha:0.0 isThemed:NO] retain]);
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
  // Never lower the interior, since that just leads to a weird shadow which can
  // often interact badly with the theme.
  return NSBackgroundStyleRaised;
}

- (void)mouseEntered:(NSEvent*)theEvent {
  [self setMouseInside:YES animate:YES];
}

- (void)mouseExited:(NSEvent*)theEvent {
  [self setMouseInside:NO animate:YES];
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

// TODO(viettrungluu): clean up/reorganize.
- (void)drawBorderAndFillForTheme:(GTMTheme*)theme
                      controlView:(NSView*)controlView
                        outerPath:(NSBezierPath*)outerPath
                        innerPath:(NSBezierPath*)innerPath
              showClickedGradient:(BOOL)showClickedGradient
            showHighlightGradient:(BOOL)showHighlightGradient
                       hoverAlpha:(CGFloat)hoverAlpha
                           active:(BOOL)active
                        cellFrame:(NSRect)cellFrame
                  defaultGradient:(NSGradient*)defaultGradient {
  BOOL isFlatButton = [self showsBorderOnlyWhileMouseInside];

  // For flat (unbordered when not hovered) buttons, never use the toolbar
  // button background image, but the modest gradient used for themed buttons.
  // To make things even more modest, scale the hover alpha down by 40 percent
  // unless clicked.
  NSImage* backgroundImage;
  BOOL useThemeGradient;
  if (isFlatButton) {
    backgroundImage = nil;
    useThemeGradient = YES;
    if (!showClickedGradient)
      hoverAlpha *= 0.6;
  } else {
    backgroundImage = [theme backgroundImageForStyle:GTMThemeStyleToolBarButton
                                               state:YES];
    useThemeGradient = backgroundImage ? YES : NO;
  }

  // The basic gradient shown inside; see above.
  NSGradient* gradient;
  if (hoverAlpha == 0 && !useThemeGradient) {
    gradient = defaultGradient ? defaultGradient
                               : gradient_;
  } else {
    gradient = [self gradientForHoverAlpha:hoverAlpha
                                  isThemed:useThemeGradient];
  }

  // If we're drawing a background image, show that; else possibly show the
  // clicked gradient.
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
      NSGradient* clickedGradient;
      if (isFlatButton) {
        clickedGradient = gradient;
      } else {
        clickedGradient =
            [theme gradientForStyle:GTMThemeStyleToolBarButtonPressed
                              state:active];
      }
      [clickedGradient drawInBezierPath:innerPath angle:90.0];
    }
  }

  // Visually indicate unclicked, enabled buttons.
  if (!showClickedGradient && [self isEnabled]) {
    [NSGraphicsContext saveGraphicsState];
    [innerPath addClip];

    // Draw the inner glow.
    if (hoverAlpha > 0) {
      [innerPath setLineWidth:2];
      [[NSColor colorWithCalibratedWhite:1.0 alpha:0.2 * hoverAlpha] setStroke];
      [innerPath stroke];
    }

    // Draw the top inner highlight.
    NSAffineTransform* highlightTransform = [NSAffineTransform transform];
    [highlightTransform translateXBy:1 yBy:1];
    scoped_nsobject<NSBezierPath> highlightPath([innerPath copy]);
    [highlightPath transformUsingAffineTransform:highlightTransform];
    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.2] setStroke];
    [highlightPath stroke];

    // Draw the gradient inside.
    [gradient drawInBezierPath:innerPath angle:90.0];

    [NSGraphicsContext restoreGraphicsState];
  }

  // Draw the outer stroke.
  NSColor* strokeColor = showClickedGradient ?
      [NSColor colorWithCalibratedWhite:0.0 alpha:0.3] :
      [theme strokeColorForStyle:GTMThemeStyleToolBarButton state:active];
  [strokeColor setStroke];

  [innerPath setLineWidth:1];
  [innerPath stroke];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  // Constants from Cole.  Will kConstant them once the feedback loop
  // is complete.
  NSRect drawFrame = NSInsetRect(cellFrame, 1.5, 1.5);
  NSRect innerFrame = NSInsetRect(cellFrame, 2, 1);
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

  GTMTheme* theme = [controlView gtm_theme];

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
                showClickedGradient:pressed
              showHighlightGradient:[self isHighlighted]
                         hoverAlpha:[self hoverAlpha]
                             active:active
                          cellFrame:cellFrame
                    defaultGradient:nil];
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

    NSColor* color = [theme iconColorForStyle:GTMThemeStyleToolBarButton
                                        state:YES];

    if (isTemplate) {
      scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
      NSColor* shadowColor = [color gtm_legibleTextColor];
      shadowColor = [shadowColor colorWithAlphaComponent:0.25];
      [shadow.get() setShadowColor:shadowColor];
      [shadow.get() setShadowOffset:NSMakeSize(0, -1.0)];
      [shadow setShadowBlurRadius:1.0];
      [shadow set];
    }

    [self drawUnderlayImageWithFrame:cellFrame inView:controlView];

    CGContextBeginTransparencyLayer(context, 0);
    NSRect imageRect = NSZeroRect;
    imageRect.size = [[self image] size];
    NSRect drawRect = [self imageRectForBounds:cellFrame];
    [[self image] setFlipped:[controlView isFlipped]];
    [[self image] drawInRect:drawRect
                    fromRect:imageRect
                   operation:NSCompositeSourceOver
                    fraction:[self isEnabled] ? 1.0 : 0.5];
    if (isTemplate) {
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
