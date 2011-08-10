// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/hover_close_button.h"

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/animation_utils.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/AppKit/GTMKeyValueAnimation.h"
#import "third_party/molokocacao/NSBezierPath+MCAdditions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace  {
const CGFloat kButtonWidth = 16;
const CGFloat kFramesPerSecond = 16; // Determined experimentally to look good.
const CGFloat kCircleRadius = 0.415 * kButtonWidth;
const CGFloat kCircleHoverWhite = 0.565;
const CGFloat kCircleClickWhite = 0.396;
const CGFloat kXShadowAlpha = 0.75;
const CGFloat kXShadowCircleAlpha = 0.1;
const CGFloat kCloseAnimationDuration = 0.1;

// Images that are used for all close buttons. Set up in +initialize.
NSImage* gHoverNoneImage = nil;
NSImage* gHoverMouseOverImage = nil;
NSImage* gHoverMouseDownImage = nil;

// Strings that are used for all close buttons. Set up in +initialize.
NSString* gTooltip = nil;
NSString* gDescription = nil;

// If this string is changed, the setter (currently setFadeOutValue:) must
// be changed as well to match.
NSString* const kFadeOutValueKeyPath = @"fadeOutValue";
}  // namespace

@interface HoverCloseButton ()

// Common initialization routine called from initWithFrame and awakeFromNib.
- (void)commonInit;

// Called by |fadeOutAnimation_| when animated value changes.
- (void)setFadeOutValue:(CGFloat)value;

// Returns an autoreleased NSImage of the close button in a given state.
+ (NSImage*)imageForBounds:(NSRect)bounds
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

    NSImage* image = [self imageForBounds:bounds
                                    xPath:xPath
                               circlePath:circlePath
                               hoverState:kHoverStateNone];
    gHoverNoneImage = [image retain];
    image = [self imageForBounds:bounds
                           xPath:xPath
                      circlePath:circlePath
                      hoverState:kHoverStateMouseOver];
    gHoverMouseOverImage = [image retain];
    image = [self imageForBounds:bounds
                           xPath:xPath
                      circlePath:circlePath
                      hoverState:kHoverStateMouseDown];
    gHoverMouseDownImage = [image retain];

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

- (void)removeFromSuperview {
  // -stopAnimation will call the animationDidStop: delegate method
  // which will release our animation.
  [fadeOutAnimation_ stopAnimation];
  [super removeFromSuperview];
}

- (void)animationDidStop:(NSAnimation*)animation {
  DCHECK(animation == fadeOutAnimation_);
  [fadeOutAnimation_ setDelegate:nil];
  [fadeOutAnimation_ release];
  fadeOutAnimation_ = nil;
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [self animationDidStop:animation];
}

- (void)drawRect:(NSRect)dirtyRect {
  // Close boxes align left horizontally, and align center vertically.
  // http:crbug.com/14739 requires this.
  NSRect imageRect = NSZeroRect;
  imageRect.size = [gHoverMouseOverImage size];

  NSRect destRect = [self bounds];
  destRect.origin.y = floor((NSHeight(destRect) / 2)
                            - (NSHeight(imageRect) / 2));
  destRect.size = imageRect.size;

  switch(self.hoverState) {
    case kHoverStateMouseOver:
      [gHoverMouseOverImage drawInRect:destRect
                              fromRect:imageRect
                             operation:NSCompositeSourceOver
                              fraction:1.0];
      break;

    case kHoverStateMouseDown:
      [gHoverMouseDownImage drawInRect:destRect
                              fromRect:imageRect
                             operation:NSCompositeSourceOver
                              fraction:1.0];
      break;

    default:
    case kHoverStateNone: {
      CGFloat value = 1.0;
      if (fadeOutAnimation_) {
        value = [fadeOutAnimation_ currentValue];
        NSImage* previousImage = nil;
        if (previousState_ == kHoverStateMouseOver) {
          previousImage = gHoverMouseOverImage;
        } else {
          previousImage =  gHoverMouseDownImage;
        }
        [previousImage drawInRect:destRect
                         fromRect:imageRect
                        operation:NSCompositeSourceOver
                         fraction:1.0 - value];
      }
      [gHoverNoneImage drawInRect:destRect
                         fromRect:imageRect
                        operation:NSCompositeSourceOver
                         fraction:value];
      break;
    }
  }
}

- (void)setFadeOutValue:(CGFloat)value {
  [self setNeedsDisplay];
}

- (void)setHoverState:(HoverState)state {
  if (state != self.hoverState) {
    previousState_ = self.hoverState;
    [super setHoverState:state];
    // Only animate the HoverStateNone case.
    if (state == kHoverStateNone) {
      DCHECK(fadeOutAnimation_ == nil);
      fadeOutAnimation_ =
          [[GTMKeyValueAnimation alloc] initWithTarget:self
                                               keyPath:kFadeOutValueKeyPath];
      [fadeOutAnimation_ setDuration:kCloseAnimationDuration];
      [fadeOutAnimation_ setFrameRate:kFramesPerSecond];
      [fadeOutAnimation_ setDelegate:self];
      [fadeOutAnimation_ startAnimation];
    } else {
      // -stopAnimation will call the animationDidStop: delegate method
      // which will clean up the animation.
      [fadeOutAnimation_ stopAnimation];
    }
  }
}

- (void)commonInit {
  // Set accessibility description.
  NSCell* cell = [self cell];
  [cell accessibilitySetOverrideValue:gDescription
                         forAttribute:NSAccessibilityDescriptionAttribute];

  // Add a tooltip.
  [self setToolTip:gTooltip];

  // Initialize previousState.
  previousState_ = kHoverStateNone;
}

+ (NSImage*)imageForBounds:(NSRect)bounds
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
  NSImage* image = [[[NSImage alloc] initWithSize:bounds.size] autorelease];
  [image addRepresentation:imageRep];
  return image;
}

@end
