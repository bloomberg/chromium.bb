// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/gradient_button_cell.h"

#include <cmath>

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/rect_path_utils.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "grit/theme_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSColor+Luminance.h"
#import "ui/base/cocoa/nsgraphics_context_additions.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

@interface GradientButtonCell (Private)
- (void)sharedInit;

// Get drawing parameters for a given cell frame in a given view. The inner
// frame is the one required by |-drawInteriorWithFrame:inView:|. The inner and
// outer paths are the ones required by |-drawBorderAndFillForTheme:...|. The
// outer path also gives the area in which to clip. Any of the |return...|
// arguments may be NULL (in which case the given parameter won't be returned).
// If |returnInnerPath| or |returnOuterPath|, |*returnInnerPath| or
// |*returnOuterPath| should be nil, respectively.
- (void)getDrawParamsForFrame:(NSRect)cellFrame
                       inView:(NSView*)controlView
                   innerFrame:(NSRect*)returnInnerFrame
                    innerPath:(NSBezierPath**)returnInnerPath
                     clipPath:(NSBezierPath**)returnClipPath;

- (void)updateTrackingAreas;

@end


static const NSTimeInterval kAnimationShowDuration = 0.2;

// Note: due to a bug (?), drawWithFrame:inView: does not call
// drawBorderAndFillForTheme::::: unless the mouse is inside.  The net
// effect is that our "fade out" when the mouse leaves becaumes
// instantaneous.  When I "fixed" it things looked horrible; the
// hover-overed bookmark button would stay highlit for 0.4 seconds
// which felt like latency/lag.  I'm leaving the "bug" in place for
// now so we don't suck.  -jrg
static const NSTimeInterval kAnimationHideDuration = 0.4;

static const NSTimeInterval kAnimationContinuousCycleDuration = 0.4;

@implementation GradientButtonCell

@synthesize hoverAlpha = hoverAlpha_;

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

- (void)dealloc {
  if (trackingArea_) {
    [[self controlView] removeTrackingArea:trackingArea_];
    trackingArea_.reset();
  }
  [super dealloc];
}

// Return YES if we are pulsing (towards another state or continuously).
- (BOOL)pulsing {
  if ((pulseState_ == gradient_button_cell::kPulsingOn) ||
      (pulseState_ == gradient_button_cell::kPulsingOff) ||
      (pulseState_ == gradient_button_cell::kPulsingContinuous))
    return YES;
  return NO;
}

// Perform one pulse step when animating a pulse.
- (void)performOnePulseStep {
  NSTimeInterval thisUpdate = [NSDate timeIntervalSinceReferenceDate];
  NSTimeInterval elapsed = thisUpdate - lastHoverUpdate_;
  CGFloat opacity = [self hoverAlpha];

  // Update opacity based on state.
  // Adjust state if we have finished.
  switch (pulseState_) {
  case gradient_button_cell::kPulsingOn:
    opacity += elapsed / kAnimationShowDuration;
    if (opacity > 1.0) {
      [self setPulseState:gradient_button_cell::kPulsedOn];
      return;
    }
    break;
  case gradient_button_cell::kPulsingOff:
    opacity -= elapsed / kAnimationHideDuration;
    if (opacity < 0.0) {
      [self setPulseState:gradient_button_cell::kPulsedOff];
      return;
    }
    break;
  case gradient_button_cell::kPulsingContinuous:
    opacity += elapsed / kAnimationContinuousCycleDuration * pulseMultiplier_;
    if (opacity > 1.0) {
      opacity = 1.0;
      pulseMultiplier_ *= -1.0;
    } else if (opacity < 0.0) {
      opacity = 0.0;
      pulseMultiplier_ *= -1.0;
    }
    outerStrokeAlphaMult_ = opacity;
    break;
  default:
    NOTREACHED() << "unknown pulse state";
  }

  // Update our control.
  lastHoverUpdate_ = thisUpdate;
  [self setHoverAlpha:opacity];
  [[self controlView] setNeedsDisplay:YES];

  // If our state needs it, keep going.
  if ([self pulsing]) {
    [self performSelector:_cmd withObject:nil afterDelay:0.02];
  }
}

- (gradient_button_cell::PulseState)pulseState {
  return pulseState_;
}

// Set the pulsing state.  This can either set the pulse to on or off
// immediately (e.g. kPulsedOn, kPulsedOff) or initiate an animated
// state change.
- (void)setPulseState:(gradient_button_cell::PulseState)pstate {
  pulseState_ = pstate;
  pulseMultiplier_ = 0.0;
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  lastHoverUpdate_ = [NSDate timeIntervalSinceReferenceDate];

  switch (pstate) {
  case gradient_button_cell::kPulsedOn:
  case gradient_button_cell::kPulsedOff:
    outerStrokeAlphaMult_ = 1.0;
    [self setHoverAlpha:((pulseState_ == gradient_button_cell::kPulsedOn) ?
                         1.0 : 0.0)];
    [[self controlView] setNeedsDisplay:YES];
    break;
  case gradient_button_cell::kPulsingOn:
  case gradient_button_cell::kPulsingOff:
    outerStrokeAlphaMult_ = 1.0;
    // Set initial value then engage timer.
    [self setHoverAlpha:((pulseState_ == gradient_button_cell::kPulsingOn) ?
                         0.0 : 1.0)];
    [self performOnePulseStep];
    break;
  case gradient_button_cell::kPulsingContinuous:
    // Semantics of continuous pulsing are that we pulse independent
    // of mouse position.
    pulseMultiplier_ = 1.0;
    [self performOnePulseStep];
    break;
  default:
    CHECK(0);
    break;
  }
}

- (void)safelyStopPulsing {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

- (void)setIsContinuousPulsing:(BOOL)continuous {
  if (!continuous && pulseState_ != gradient_button_cell::kPulsingContinuous)
    return;
  if (continuous) {
    [self setPulseState:gradient_button_cell::kPulsingContinuous];
  } else {
    [self setPulseState:(isMouseInside_ ? gradient_button_cell::kPulsedOn :
                         gradient_button_cell::kPulsedOff)];
  }
}

- (BOOL)isContinuousPulsing {
  return (pulseState_ == gradient_button_cell::kPulsingContinuous) ?
      YES : NO;
}

#if 1
// If we are not continuously pulsing, perform a pulse animation to
// reflect our new state.
- (void)setMouseInside:(BOOL)flag animate:(BOOL)animated {
  isMouseInside_ = flag;
  if (pulseState_ != gradient_button_cell::kPulsingContinuous) {
    if (animated) {
      [self setPulseState:(isMouseInside_ ? gradient_button_cell::kPulsingOn :
                           gradient_button_cell::kPulsingOff)];
    } else {
      [self setPulseState:(isMouseInside_ ? gradient_button_cell::kPulsedOn :
                           gradient_button_cell::kPulsedOff)];
    }
  }
}
#else

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



#endif

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
  pulseState_ = gradient_button_cell::kPulsedOff;
  pulseMultiplier_ = 1.0;
  outerStrokeAlphaMult_ = 1.0;
  gradient_.reset([[self gradientForHoverAlpha:0.0 isThemed:NO] retain]);
}

- (void)setShouldTheme:(BOOL)shouldTheme {
  shouldTheme_ = shouldTheme;
}

- (NSImage*)overlayImage {
  return overlayImage_.get();
}

- (void)setOverlayImage:(NSImage*)image {
  overlayImage_.reset([image retain]);
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
    [self updateTrackingAreas];
  } else {
    if (trackingArea_) {
      [[self controlView] removeTrackingArea:trackingArea_];
      trackingArea_.reset(nil);
      if (isMouseInside_) {
        isMouseInside_ = NO;
        [[self controlView] setNeedsDisplay:YES];
      }
    }
  }
}

// TODO(viettrungluu): clean up/reorganize.
- (void)drawBorderAndFillForTheme:(ui::ThemeProvider*)themeProvider
                      controlView:(NSView*)controlView
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
  NSColor* backgroundImageColor;
  BOOL useThemeGradient;
  if (isFlatButton) {
    backgroundImageColor = nil;
    useThemeGradient = YES;
    if (!showClickedGradient)
      hoverAlpha *= 0.6;
  } else {
    backgroundImageColor = nil;
    if (themeProvider &&
        themeProvider->HasCustomImage(IDR_THEME_BUTTON_BACKGROUND)) {
      backgroundImageColor =
          themeProvider->GetNSImageColorNamed(IDR_THEME_BUTTON_BACKGROUND);
    }
    useThemeGradient = backgroundImageColor ? YES : NO;
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
  if (backgroundImageColor) {
    [backgroundImageColor set];
    // Set the phase to match window.
    NSRect trueRect = [controlView convertRect:cellFrame toView:nil];
    [[NSGraphicsContext currentContext]
        cr_setPatternPhase:NSMakePoint(NSMinX(trueRect), NSMaxY(trueRect))
                   forView:controlView];
    [innerPath fill];
  } else {
    if (showClickedGradient) {
      NSGradient* clickedGradient = nil;
      if (isFlatButton &&
          [self tag] == kStandardButtonTypeWithLimitedClickFeedback) {
        clickedGradient = gradient;
      } else {
        clickedGradient = themeProvider ? themeProvider->GetNSGradient(
            active ?
                ThemeProperties::GRADIENT_TOOLBAR_BUTTON_PRESSED :
                ThemeProperties::GRADIENT_TOOLBAR_BUTTON_PRESSED_INACTIVE) :
            nil;
      }
      [clickedGradient drawInBezierPath:innerPath angle:90.0];
    }
  }

  // Visually indicate unclicked, enabled buttons.
  if (!showClickedGradient && [self isEnabled]) {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
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
    base::scoped_nsobject<NSBezierPath> highlightPath([innerPath copy]);
    [highlightPath transformUsingAffineTransform:highlightTransform];
    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.2] setStroke];
    [highlightPath stroke];

    // Draw the gradient inside.
    [gradient drawInBezierPath:innerPath angle:90.0];
  }

  // Don't draw anything else for disabled flat buttons.
  if (isFlatButton && ![self isEnabled])
    return;

  // Draw the outer stroke.
  NSColor* strokeColor = nil;
  if (showClickedGradient) {
    strokeColor = [NSColor
                    colorWithCalibratedWhite:0.0
                                       alpha:0.3 * outerStrokeAlphaMult_];
  } else {
    strokeColor = themeProvider ? themeProvider->GetNSColor(
        active ? ThemeProperties::COLOR_TOOLBAR_BUTTON_STROKE :
                 ThemeProperties::COLOR_TOOLBAR_BUTTON_STROKE_INACTIVE) :
        [NSColor colorWithCalibratedWhite:0.0
                                    alpha:0.3 * outerStrokeAlphaMult_];
  }
  [strokeColor setStroke];

  [innerPath setLineWidth:1];
  [innerPath stroke];
}

// TODO(viettrungluu): clean this up.
// (Private)
- (void)getDrawParamsForFrame:(NSRect)cellFrame
                       inView:(NSView*)controlView
                   innerFrame:(NSRect*)returnInnerFrame
                    innerPath:(NSBezierPath**)returnInnerPath
                     clipPath:(NSBezierPath**)returnClipPath {
  const CGFloat lineWidth = [controlView cr_lineWidth];
  const CGFloat halfLineWidth = lineWidth / 2.0;

  // Constants from Cole.  Will kConstant them once the feedback loop
  // is complete.
  NSRect drawFrame = NSInsetRect(cellFrame, 1.5 * lineWidth, 1.5 * lineWidth);
  NSRect innerFrame = NSInsetRect(cellFrame, lineWidth, lineWidth);
  const CGFloat radius = 3;

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
  if (type == kLeftButtonWithShadowType)
    innerFrame.size.width -= 1.0;

  // Return results if |return...| not null.
  if (returnInnerFrame)
    *returnInnerFrame = innerFrame;
  if (returnInnerPath) {
    DCHECK(*returnInnerPath == nil);
    *returnInnerPath = [NSBezierPath bezierPathWithRoundedRect:drawFrame
                                                       xRadius:radius
                                                       yRadius:radius];
    [*returnInnerPath setLineWidth:lineWidth];
  }
  if (returnClipPath) {
    DCHECK(*returnClipPath == nil);
    NSRect clipPathRect =
        NSInsetRect(drawFrame, -halfLineWidth, -halfLineWidth);
    *returnClipPath = [NSBezierPath
        bezierPathWithRoundedRect:clipPathRect
                          xRadius:radius + halfLineWidth
                          yRadius:radius + halfLineWidth];
  }
}

// TODO(viettrungluu): clean this up.
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  NSRect innerFrame;
  NSBezierPath* innerPath = nil;
  [self getDrawParamsForFrame:cellFrame
                       inView:controlView
                   innerFrame:&innerFrame
                    innerPath:&innerPath
                     clipPath:NULL];

  BOOL pressed = ([((NSControl*)[self controlView]) isEnabled] &&
                  [self isHighlighted]);
  NSWindow* window = [controlView window];
  ui::ThemeProvider* themeProvider = [window themeProvider];
  BOOL active = [window isKeyWindow] || [window isMainWindow];

  // Stroke the borders and appropriate fill gradient. If we're borderless, the
  // only time we want to draw the inner gradient is if we're highlighted or if
  // we're the first responder (when "Full Keyboard Access" is turned on).
  if (([self isBordered] && ![self showsBorderOnlyWhileMouseInside]) ||
      pressed ||
      [self isMouseInside] ||
      [self isContinuousPulsing] ||
      [self showsFirstResponder]) {

    // When pulsing we want the bookmark to stand out a little more.
    BOOL showClickedGradient = pressed ||
        (pulseState_ == gradient_button_cell::kPulsingContinuous);

    [self drawBorderAndFillForTheme:themeProvider
                        controlView:controlView
                          innerPath:innerPath
                showClickedGradient:showClickedGradient
              showHighlightGradient:[self isHighlighted]
                         hoverAlpha:[self hoverAlpha]
                             active:active
                          cellFrame:cellFrame
                    defaultGradient:nil];
  }

  // If this is the left side of a segmented button, draw a slight shadow.
  ButtonType type = [[(NSControl*)controlView cell] tag];
  if (type == kLeftButtonWithShadowType) {
    const CGFloat lineWidth = [controlView cr_lineWidth];
    NSRect borderRect, contentRect;
    NSDivideRect(cellFrame, &borderRect, &contentRect, lineWidth, NSMaxXEdge);
    NSColor* stroke = themeProvider ? themeProvider->GetNSColor(
        active ? ThemeProperties::COLOR_TOOLBAR_BUTTON_STROKE :
                 ThemeProperties::COLOR_TOOLBAR_BUTTON_STROKE_INACTIVE) :
        [NSColor blackColor];

    [[stroke colorWithAlphaComponent:0.2] set];
    NSRectFillUsingOperation(NSInsetRect(borderRect, 0, 2),
                             NSCompositeSourceOver);
  }
  [self drawInteriorWithFrame:innerFrame inView:controlView];

  // Draws the blue focus ring.
  if ([self showsFirstResponder]) {
    gfx::ScopedNSGraphicsContextSaveGState scoped_state;
    const CGFloat lineWidth = [controlView cr_lineWidth];
    // insetX = 1.0 is used for the drawing of blue highlight so that this
    // highlight won't be too near the bookmark toolbar itself, in case we
    // draw bookmark buttons in bookmark toolbar.
    rect_path_utils::FrameRectWithInset(rect_path_utils::RoundedCornerAll,
                                        NSInsetRect(cellFrame, 0, lineWidth),
                                        1.0,            // insetX
                                        0.0,            // insetY
                                        3.0,            // outerRadius
                                        lineWidth * 2,  // lineWidth
                                        [controlView
                                            cr_keyboardFocusIndicatorColor]);
  }
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  const CGFloat lineWidth = [controlView cr_lineWidth];

  if (shouldTheme_) {
    BOOL isTemplate = [[self image] isTemplate];

    gfx::ScopedNSGraphicsContextSaveGState scopedGState;

    CGContextRef context =
        (CGContextRef)([[NSGraphicsContext currentContext] graphicsPort]);

    ThemeService* themeProvider = static_cast<ThemeService*>(
        [[controlView window] themeProvider]);
    NSColor* color = themeProvider ?
        themeProvider->GetNSColorTint(ThemeProperties::TINT_BUTTONS) :
        [NSColor blackColor];

    if (isTemplate && themeProvider && themeProvider->UsingDefaultTheme()) {
      base::scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
      [shadow.get() setShadowColor:themeProvider->GetNSColor(
          ThemeProperties::COLOR_TOOLBAR_BEZEL)];
      [shadow.get() setShadowOffset:NSMakeSize(0.0, -lineWidth)];
      [shadow setShadowBlurRadius:lineWidth];
      [shadow set];
    }

    CGContextBeginTransparencyLayer(context, 0);
    NSRect imageRect = NSZeroRect;
    imageRect.size = [[self image] size];
    NSRect drawRect = [self imageRectForBounds:cellFrame];
    [[self image] drawInRect:drawRect
                    fromRect:imageRect
                   operation:NSCompositeSourceOver
                    fraction:[self isEnabled] ? 1.0 : 0.5
              respectFlipped:YES
                       hints:nil];
    if (isTemplate && color) {
      [color set];
      NSRectFillUsingOperation(cellFrame, NSCompositeSourceAtop);
    }
    CGContextEndTransparencyLayer(context);
  } else {
    // NSCell draws these off-center for some reason, probably because of the
    // positioning of the control in the xib.
    [super drawInteriorWithFrame:NSOffsetRect(cellFrame, 0, lineWidth)
                          inView:controlView];
  }

  if (overlayImage_) {
    NSRect imageRect = NSZeroRect;
    imageRect.size = [overlayImage_ size];
    [overlayImage_ drawInRect:[self imageRectForBounds:cellFrame]
                     fromRect:imageRect
                    operation:NSCompositeSourceOver
                     fraction:[self isEnabled] ? 1.0 : 0.5
               respectFlipped:YES
                        hints:nil];
  }
}

- (int)verticalTextOffset {
  return 1;
}

// Overriden from NSButtonCell so we can display a nice fadeout effect for
// button titles that overflow.
// This method is copied in the most part from GTMFadeTruncatingTextFieldCell,
// the only difference is that here we draw the text ourselves rather than
// calling the super to do the work.
// We can't use GTMFadeTruncatingTextFieldCell because there's no easy way to
// get it to work with NSButtonCell.
// TODO(jeremy): Move this to GTM.
- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)cellFrame
             inView:(NSView*)controlView {
  NSSize size = [title size];

  // Empirically, Cocoa will draw an extra 2 pixels past NSWidth(cellFrame)
  // before it clips the text.
  const CGFloat kOverflowBeforeClip = 2;
  BOOL clipping = YES;
  if (std::floor(size.width) <= (NSWidth(cellFrame) + kOverflowBeforeClip)) {
    cellFrame.origin.y += ([self verticalTextOffset] - 1);
    clipping = NO;
  }

  // Gradient is about twice our line height long.
  CGFloat gradientWidth = MIN(size.height * 2, NSWidth(cellFrame) / 4);

  NSRect solidPart, gradientPart;
  NSDivideRect(cellFrame, &gradientPart, &solidPart, gradientWidth, NSMaxXEdge);

  // Draw non-gradient part without transparency layer, as light text on a dark
  // background looks bad with a gradient layer.
  NSPoint textOffset = NSZeroPoint;
  {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    if (clipping)
      [NSBezierPath clipRect:solidPart];

    // 11 is the magic number needed to make this match the native
    // NSButtonCell's label display.
    CGFloat textLeft = [[self image] size].width + 11;

    // For some reason, the height of cellFrame as passed in is totally bogus.
    // For vertical centering purposes, we need the bounds of the containing
    // view.
    NSRect buttonFrame = [[self controlView] frame];

    // Call the vertical offset to match native NSButtonCell's version.
    textOffset = NSMakePoint(textLeft,
                             (NSHeight(buttonFrame) - size.height) / 2 +
                             [self verticalTextOffset]);
    [title drawAtPoint:textOffset];
  }

  if (!clipping)
    return cellFrame;

  // Draw the gradient part with a transparency layer. This makes the text look
  // suboptimal, but since it fades out, that's ok.
  gfx::ScopedNSGraphicsContextSaveGState scopedGState;
  [NSBezierPath clipRect:gradientPart];
  CGContextRef context = static_cast<CGContextRef>(
      [[NSGraphicsContext currentContext] graphicsPort]);
  CGContextBeginTransparencyLayerWithRect(context,
                                          NSRectToCGRect(gradientPart), 0);
  [title drawAtPoint:textOffset];

  NSColor *color = [NSColor textColor];
  NSColor *alphaColor = [color colorWithAlphaComponent:0.0];
  NSGradient *mask = [[NSGradient alloc] initWithStartingColor:color
                                                   endingColor:alphaColor];

  // Draw the gradient mask
  CGContextSetBlendMode(context, kCGBlendModeDestinationIn);
  [mask drawFromPoint:NSMakePoint(NSMaxX(cellFrame) - gradientWidth,
                                  NSMinY(cellFrame))
              toPoint:NSMakePoint(NSMaxX(cellFrame),
                                  NSMinY(cellFrame))
              options:NSGradientDrawsBeforeStartingLocation];
  [mask release];
  CGContextEndTransparencyLayer(context);

  return cellFrame;
}

- (NSBezierPath*)clipPathForFrame:(NSRect)cellFrame
                           inView:(NSView*)controlView {
  NSBezierPath* boundingPath = nil;
  [self getDrawParamsForFrame:cellFrame
                       inView:controlView
                   innerFrame:NULL
                    innerPath:NULL
                     clipPath:&boundingPath];
  return boundingPath;
}

- (void)resetCursorRect:(NSRect)cellFrame inView:(NSView*)controlView {
  [super resetCursorRect:cellFrame inView:controlView];
  if (trackingArea_)
    [self updateTrackingAreas];
}

- (BOOL)isMouseReallyInside {
  BOOL mouseInView = NO;
  NSView* controlView = [self controlView];
  NSWindow* window = [controlView window];
  NSRect bounds = [controlView bounds];
  if (window) {
    NSPoint mousePoint = [window mouseLocationOutsideOfEventStream];
    mousePoint = [controlView convertPoint:mousePoint fromView:nil];
    mouseInView = [controlView mouse:mousePoint inRect:bounds];
  }
  return mouseInView;
}

- (void)updateTrackingAreas {
  NSView* controlView = [self controlView];
  BOOL mouseInView = [self isMouseReallyInside];

  if (trackingArea_.get())
    [controlView removeTrackingArea:trackingArea_];

  NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited |
                                  NSTrackingActiveInActiveApp;
  if (mouseInView)
    options |= NSTrackingAssumeInside;

  trackingArea_.reset([[NSTrackingArea alloc]
                        initWithRect:[controlView bounds]
                             options:options
                               owner:self
                            userInfo:nil]);
  if (isMouseInside_ != mouseInView) {
    [self setMouseInside:mouseInView animate:NO];
    [controlView setNeedsDisplay:YES];
  }
}

@end
