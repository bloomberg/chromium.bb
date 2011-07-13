// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/hover_close_button.h"

#import <QuartzCore/QuartzCore.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/animation_utils.h"
#include "grit/generated_resources.h"
#import "third_party/molokocacao/NSBezierPath+MCAdditions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace  {
const CGFloat kButtonWidth = 16;
const CGFloat kCircleRadius = 0.415 * kButtonWidth;
const CGFloat kCircleHoverWhite = 0.565;
const CGFloat kCircleClickWhite = 0.396;
const CGFloat kXShadowAlpha = 0.75;
const CGFloat kXShadowCircleAlpha = 0.1;

// Images that are used for all close buttons. Set up in +initialize.
CGImageRef gHoverNoneImage = NULL;
CGImageRef gHoverMouseOverImage = NULL;
CGImageRef gHoverMouseDownImage = NULL;

// Strings that are used for all close buttons. Set up in +initialize.
NSString* gTooltip = nil;
NSString* gDescription = nil;
}  // namespace

@interface HoverCloseButton ()
// Sets up the button's tracking areas and accessibility info when instantiated
// via initWithFrame or awakeFromNib.
- (void)commonInit;

// Draws the close button into a CGImageRef in a given state.
+ (CGImageRef)imageForBounds:(NSRect)bounds
                       xPath:(NSBezierPath*)xPath
                  circlePath:(NSBezierPath*)circlePath
                  hoverState:(HoverState)hoverState;
@end

@implementation HoverCloseButton

+ (void)initialize {
  if ([self class] == [HoverCloseButton class]) {
    // Set up the paths for our images. They are centered around the origin.
    NSRect bounds = NSMakeRect(0, 0, kButtonWidth, kButtonWidth);
    NSBezierPath* circlePath = [NSBezierPath bezierPath];
    [circlePath appendBezierPathWithArcWithCenter:NSZeroPoint
                                           radius:kCircleRadius
                                       startAngle:0.0
                                         endAngle:365.0];

    // Construct an 'x' by drawing two intersecting rectangles in the shape of a
    // cross and then rotating the path by 45 degrees.
    NSBezierPath* xPath = [NSBezierPath bezierPath];
    [xPath appendBezierPathWithRect:NSMakeRect(-4.5, -1.0, 9.0, 2.0)];
    [xPath appendBezierPathWithRect:NSMakeRect(-1.0, -4.5, 2.0, 9.0)];

    NSAffineTransform* transform = [NSAffineTransform transform];
    [transform rotateByDegrees:45.0];
    [xPath transformUsingAffineTransform:transform];

    // Move the paths into the center of the given bounds rectangle.
    transform = [NSAffineTransform transform];
    NSPoint xCenter = NSMakePoint(NSWidth(bounds) / 2.0,
                                  NSHeight(bounds) / 2.0);
    [transform translateXBy:xCenter.x yBy:xCenter.y];
    [circlePath transformUsingAffineTransform:transform];
    [xPath transformUsingAffineTransform:transform];

    CGImageRef image = [self imageForBounds:bounds
                                      xPath:xPath
                                 circlePath:circlePath
                                 hoverState:kHoverStateNone];
    gHoverNoneImage = CGImageRetain(image);
    image = [self imageForBounds:bounds
                           xPath:xPath
                      circlePath:circlePath
                      hoverState:kHoverStateMouseOver];
    gHoverMouseOverImage = CGImageRetain(image);
    image = [self imageForBounds:bounds
                           xPath:xPath
                      circlePath:circlePath
                      hoverState:kHoverStateMouseDown];
    gHoverMouseDownImage = CGImageRetain(image);

    // Grab some strings that are used by all close buttons.
    gDescription = [l10n_util::GetNSStringWithFixup(IDS_ACCNAME_CLOSE) copy];
    gTooltip = [l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_CLOSE_TAB) copy];
  }
}

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self commonInit];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self commonInit];
}

- (void)setHoverState:(HoverState)state {
  [super setHoverState:state];

  // Only animate the HoverStateNone case.
  scoped_ptr<ScopedCAActionDisabler> disabler;
  if (state != kHoverStateNone) {
    disabler.reset(new ScopedCAActionDisabler);
  }
  [hoverNoneLayer_ setHidden:state != kHoverStateNone];
  [hoverMouseDownLayer_ setHidden:state != kHoverStateMouseDown];
  [hoverMouseOverLayer_ setHidden:state != kHoverStateMouseOver];
}

- (void)commonInit {
  // Set accessibility description.
  NSCell* cell = [self cell];
  [cell accessibilitySetOverrideValue:gDescription
                         forAttribute:NSAccessibilityDescriptionAttribute];

  // Add a tooltip.
  [self setToolTip:gTooltip];

  // Set up layers
  CALayer* viewLayer = [self layer];

  // Layers will be contrained to be 0 pixels from left edge horizontally,
  // and centered vertically.
  viewLayer.layoutManager = [CAConstraintLayoutManager layoutManager];
  CAConstraint* xConstraint =
      [CAConstraint constraintWithAttribute:kCAConstraintMinX
                                 relativeTo:@"superlayer"
                                  attribute:kCAConstraintMinX
                                     offset:0];
  CAConstraint* yConstraint =
      [CAConstraint constraintWithAttribute:kCAConstraintMidY
                                 relativeTo:@"superlayer"
                                  attribute:kCAConstraintMidY];

  hoverNoneLayer_.reset([[CALayer alloc] init]);
  [hoverNoneLayer_ setDelegate:self];

  // .get() is being used here (and below) because if it isn't used, the
  // compiler doesn't realize that the call to setFrame: is being performed
  // on a CALayer, and assumes that the call is being performed on a NSView.
  // setFrame: on NSView takes an NSRect, setFrame: on CALayer takes a CGRect.
  // The difference in arguments causes a compile error.
  [hoverNoneLayer_.get() setFrame:viewLayer.frame];
  [viewLayer addSublayer:hoverNoneLayer_];
  [hoverNoneLayer_ addConstraint:xConstraint];
  [hoverNoneLayer_ addConstraint:yConstraint];
  [hoverNoneLayer_ setContents:reinterpret_cast<id>(gHoverNoneImage)];
  [hoverNoneLayer_ setHidden:NO];

  hoverMouseOverLayer_.reset([[CALayer alloc] init]);
  [hoverMouseOverLayer_.get() setFrame:viewLayer.frame];
  [viewLayer addSublayer:hoverMouseOverLayer_];
  [hoverMouseOverLayer_ addConstraint:xConstraint];
  [hoverMouseOverLayer_ addConstraint:yConstraint];
  [hoverMouseOverLayer_ setContents:reinterpret_cast<id>(gHoverMouseOverImage)];
  [hoverMouseOverLayer_ setHidden:YES];

  hoverMouseDownLayer_.reset([[CALayer alloc] init]);
  [hoverMouseDownLayer_.get() setFrame:viewLayer.frame];
  [viewLayer addSublayer:hoverMouseDownLayer_];
  [hoverMouseDownLayer_ addConstraint:xConstraint];
  [hoverMouseDownLayer_ addConstraint:yConstraint];
  [hoverMouseDownLayer_ setContents:reinterpret_cast<id>(gHoverMouseDownImage)];
  [hoverMouseDownLayer_ setHidden:YES];
}

+ (CGImageRef)imageForBounds:(NSRect)bounds
                       xPath:(NSBezierPath*)xPath
                  circlePath:(NSBezierPath*)circlePath
                  hoverState:(HoverState)hoverState {
  gfx::ScopedNSGraphicsContextSaveGState graphicsStateSaver;

  scoped_nsobject<NSBitmapImageRep> imageRep(
      [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:NSWidth(bounds)
                      pixelsHigh:NSHeight(bounds)
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSCalibratedRGBColorSpace
                    bitmapFormat:0
                     bytesPerRow:kButtonWidth * 4
                    bitsPerPixel:32]);
  NSGraphicsContext* gc =
      [NSGraphicsContext graphicsContextWithBitmapImageRep:imageRep];
  [NSGraphicsContext setCurrentContext:gc];

  [[NSColor clearColor] set];
  NSRectFill(bounds);

  // If the user is hovering over the button, a light/dark gray circle is drawn
  // behind the 'x'.
  if (hoverState != kHoverStateNone) {
    // Adjust the darkness of the circle depending on whether it is being
    // clicked.
    CGFloat white = (hoverState == kHoverStateMouseOver) ?
        kCircleHoverWhite : kCircleClickWhite;
    [[NSColor colorWithCalibratedWhite:white alpha:1.0] set];
    [circlePath fill];
  }

  [[NSColor whiteColor] set];
  [xPath fill];

  // Give the 'x' an inner shadow for depth. If the button is in a hover state
  // (circle behind it), then adjust the shadow accordingly (not as harsh).
  NSShadow* shadow = [[[NSShadow alloc] init] autorelease];
  CGFloat alpha = (hoverState != kHoverStateNone) ?
      kXShadowCircleAlpha : kXShadowAlpha;
  [shadow setShadowColor:[NSColor colorWithCalibratedWhite:0.15 alpha:alpha]];
  [shadow setShadowOffset:NSMakeSize(0.0, 0.0)];
  [shadow setShadowBlurRadius:2.5];
  [xPath fillWithInnerShadow:shadow];

  // CGImage returns an autoreleased CGImageRef.
  return [imageRep CGImage];
}

@end
