// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/new_tab_button.h"

#include "base/mac/foundation_util.h"
#include "base/mac/sdk_forward_declarations.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#include "chrome/browser/ui/cocoa/tabs/tab_view.h"
#include "grit/theme_resources.h"
#include "ui/base/cocoa/nsgraphics_context_additions.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"

@class NewTabButtonCell;

namespace {

enum class RenderingOption {
  NORMAL,
  OVERLAY_LIGHTEN,
  OVERLAY_LIGHTEN_INCOGNITO,
  OVERLAY_DARKEN,
  INLAY_LIGHTEN,
};

NSImage* GetMaskImageFromCell(NewTabButtonCell* aCell) {
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    return bundle.GetNativeImageNamed(IDR_NEWTAB_BUTTON_MASK).ToNSImage();
  }
  return [aCell imageForState:image_button_cell::kDefaultState view:nil];
}

// Creates an NSImage with size |size| and bitmap image representations for both
// 1x and 2x scale factors. |drawingHandler| is called once for every scale
// factor.  This is similar to -[NSImage imageWithSize:flipped:drawingHandler:],
// but this function always evaluates drawingHandler eagerly, and it works on
// 10.6 and 10.7.
NSImage* CreateImageWithSize(NSSize size,
                             void (^drawingHandler)(NSSize)) {
  base::scoped_nsobject<NSImage> result([[NSImage alloc] initWithSize:size]);
  [NSGraphicsContext saveGraphicsState];
  for (ui::ScaleFactor scale_factor : ui::GetSupportedScaleFactors()) {
    float scale = GetScaleForScaleFactor(scale_factor);
    NSBitmapImageRep *bmpImageRep = [[[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:size.width * scale
                      pixelsHigh:size.height * scale
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:0
                    bitsPerPixel:0] autorelease];
    [bmpImageRep setSize:size];
    [NSGraphicsContext setCurrentContext:
        [NSGraphicsContext graphicsContextWithBitmapImageRep:bmpImageRep]];
    drawingHandler(size);
    [result addRepresentation:bmpImageRep];
  }
  [NSGraphicsContext restoreGraphicsState];

  return result.release();
}

// Takes a normal bitmap and a mask image and returns an image the size of the
// mask that has pixels from |image| but alpha information from |mask|.
NSImage* ApplyMask(NSImage* image, NSImage* mask) {
  return [CreateImageWithSize([mask size], ^(NSSize size) {
      // Skip a few pixels from the top of the tab background gradient, because
      // the new tab button is not drawn at the very top of the browser window.
      const int kYOffset = 10;
      CGFloat width = size.width;
      CGFloat height = size.height;

      // In some themes, the tab background image is narrower than the
      // new tab button, so tile the background image.
      CGFloat x = 0;
      // The floor() is to make sure images with odd widths don't draw to the
      // same pixel twice on retina displays. (Using NSDrawThreePartImage()
      // caused a startup perf regression, so that cannot be used.)
      CGFloat tileWidth = floor(std::min(width, [image size].width));
      while (x < width) {
        [image drawAtPoint:NSMakePoint(x, 0)
                  fromRect:NSMakeRect(0,
                                      [image size].height - height - kYOffset,
                                      tileWidth,
                                      height)
                 operation:NSCompositeCopy
                  fraction:1.0];
        x += tileWidth;
      }

      [mask drawAtPoint:NSZeroPoint
               fromRect:NSMakeRect(0, 0, width, height)
              operation:NSCompositeDestinationIn
               fraction:1.0];
  }) autorelease];
}

// Paints |overlay| on top of |ground|.
NSImage* Overlay(NSImage* ground, NSImage* overlay, CGFloat alpha) {
  DCHECK_EQ([ground size].width, [overlay size].width);
  DCHECK_EQ([ground size].height, [overlay size].height);

  return [CreateImageWithSize([ground size], ^(NSSize size) {
      CGFloat width = size.width;
      CGFloat height = size.height;
      [ground drawAtPoint:NSZeroPoint
                 fromRect:NSMakeRect(0, 0, width, height)
                operation:NSCompositeCopy
                 fraction:1.0];
      [overlay drawAtPoint:NSZeroPoint
                  fromRect:NSMakeRect(0, 0, width, height)
                 operation:NSCompositeSourceOver
                  fraction:alpha];
  }) autorelease];
}

CGFloat LineWidthFromContext(CGContextRef context) {
  CGRect unitRect = CGRectMake(0.0, 0.0, 1.0, 1.0);
  CGRect deviceRect = CGContextConvertRectToDeviceSpace(context, unitRect);
  return 1.0 / deviceRect.size.height;
}

}  // namespace

@interface NewTabButtonCustomImageRep : NSCustomImageRep
@property (assign, nonatomic) NSView* destView;
@property (copy, nonatomic) NSColor* fillColor;
@property (assign, nonatomic) NSPoint patternPhasePosition;
@property (assign, nonatomic) RenderingOption renderingOption;
@end

@implementation NewTabButtonCustomImageRep

@synthesize destView = destView_;
@synthesize fillColor = fillColor_;
@synthesize patternPhasePosition = patternPhasePosition_;
@synthesize renderingOption = renderingOption_;

- (void)dealloc {
  [fillColor_ release];
  [super dealloc];
}

@end

// A simple override of the ImageButtonCell to disable handling of
// -mouseEntered.
@interface NewTabButtonCell : ImageButtonCell

- (void)mouseEntered:(NSEvent*)theEvent;

@end

@implementation NewTabButtonCell

- (void)mouseEntered:(NSEvent*)theEvent {
  // Ignore this since the NTB enter is handled by the TabStripController.
}

- (void)drawFocusRingMaskWithFrame:(NSRect)cellFrame inView:(NSView*)view {
  // Match the button's shape.
  [self drawImage:GetMaskImageFromCell(self) withFrame:cellFrame inView:view];
}

@end


@implementation NewTabButton

+ (Class)cellClass {
  return [NewTabButtonCell class];
}

- (BOOL)pointIsOverButton:(NSPoint)point {
  NSPoint localPoint = [self convertPoint:point fromView:[self superview]];
  NSRect pointRect = NSMakeRect(localPoint.x, localPoint.y, 1, 1);
  NSImage* buttonMask = GetMaskImageFromCell([self cell]);
  NSRect bounds = self.bounds;
  NSSize buttonMaskSize = [buttonMask size];
  NSRect destinationRect = NSMakeRect(
      (NSWidth(bounds) - buttonMaskSize.width) / 2,
      (NSHeight(bounds) - buttonMaskSize.height) / 2,
      buttonMaskSize.width, buttonMaskSize.height);
  return [buttonMask hitTestRect:pointRect
        withImageDestinationRect:destinationRect
                         context:nil
                           hints:nil
                         flipped:YES];
}

// Override to only accept clicks within the bounds of the defined path, not
// the entire bounding box. |aPoint| is in the superview's coordinate system.
- (NSView*)hitTest:(NSPoint)aPoint {
  if ([self pointIsOverButton:aPoint])
    return [super hitTest:aPoint];
  return nil;
}

// ThemedWindowDrawing implementation.

- (void)windowDidChangeTheme {
  [self setNeedsDisplay:YES];
}

- (void)windowDidChangeActive {
  [self setNeedsDisplay:YES];
}

- (void)viewDidMoveToWindow {
  NewTabButtonCell* cell = base::mac::ObjCCast<NewTabButtonCell>([self cell]);

  if ([self window] &&
      ![cell imageForState:image_button_cell::kDefaultState view:self]) {
    [self setImages];
  }
}

- (void)setImages {
  const ui::ThemeProvider* theme = [[self window] themeProvider];
  if (!theme) {
    return;
  }

  // The old way of doing things.
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    NSImage* mask = rb.GetNativeImageNamed(IDR_NEWTAB_BUTTON_MASK).ToNSImage();
    NSImage* normal = rb.GetNativeImageNamed(IDR_NEWTAB_BUTTON).ToNSImage();
    NSImage* hover = rb.GetNativeImageNamed(IDR_NEWTAB_BUTTON_H).ToNSImage();
    NSImage* pressed = rb.GetNativeImageNamed(IDR_NEWTAB_BUTTON_P).ToNSImage();

    NSImage* foreground = ApplyMask(
        theme->GetNSImageNamed(IDR_THEME_TAB_BACKGROUND), mask);

    [[self cell] setImage:Overlay(foreground, normal, 1.0)
                    forButtonState:image_button_cell::kDefaultState];
    [[self cell] setImage:Overlay(foreground, hover, 1.0)
                    forButtonState:image_button_cell::kHoverState];
    [[self cell] setImage:Overlay(foreground, pressed, 1.0)
                    forButtonState:image_button_cell::kPressedState];

    // IDR_THEME_TAB_BACKGROUND_INACTIVE is only used with the default theme.
    if (theme->UsingSystemTheme()) {
      const CGFloat alpha = tabs::kImageNoFocusAlpha;
      NSImage* background = ApplyMask(
          theme->GetNSImageNamed(IDR_THEME_TAB_BACKGROUND_INACTIVE), mask);
      [[self cell] setImage:Overlay(background, normal, alpha)
                      forButtonState:
                          image_button_cell::kDefaultStateBackground];
      [[self cell] setImage:Overlay(background, hover, alpha)
                      forButtonState:image_button_cell::kHoverStateBackground];
    } else {
      [[self cell] setImage:nil
                      forButtonState:
                          image_button_cell::kDefaultStateBackground];
      [[self cell] setImage:nil
                      forButtonState:image_button_cell::kHoverStateBackground];
    }
    return;
  }

  NSImage* mask = [self imageWithFillColor:[NSColor whiteColor]];
  NSImage* normal =
      [self imageForState:image_button_cell::kDefaultState theme:theme];
  NSImage* hover =
      [self imageForState:image_button_cell::kHoverState theme:theme];
  NSImage* pressed =
      [self imageForState:image_button_cell::kPressedState theme:theme];
  NSImage* normalBackground = nil;
  NSImage* hoverBackground = nil;

  // If using a custom theme, overlay the default image with the theme's custom
  // tab background image.
  if (!theme->UsingSystemTheme()) {
    NSImage* foreground =
        ApplyMask(theme->GetNSImageNamed(IDR_THEME_TAB_BACKGROUND), mask);
    normal = Overlay(foreground, normal, 1.0);
    hover = Overlay(foreground, hover, 1.0);
    pressed = Overlay(foreground, pressed, 1.0);

    NSImage* background = ApplyMask(
        theme->GetNSImageNamed(IDR_THEME_TAB_BACKGROUND_INACTIVE), mask);
    normalBackground = Overlay(background, normal, tabs::kImageNoFocusAlpha);
    hoverBackground = Overlay(background, hover, tabs::kImageNoFocusAlpha);
  }

  NewTabButtonCell* cell = base::mac::ObjCCast<NewTabButtonCell>([self cell]);
  [cell setImage:normal forButtonState:image_button_cell::kDefaultState];
  [cell setImage:hover forButtonState:image_button_cell::kHoverState];
  [cell setImage:pressed forButtonState:image_button_cell::kPressedState];
  [cell setImage:normalBackground
      forButtonState:image_button_cell::kDefaultStateBackground];
  [cell setImage:hoverBackground
      forButtonState:image_button_cell::kHoverStateBackground];
}

- (NSImage*)imageForState:(image_button_cell::ButtonState)state
                    theme:(const ui::ThemeProvider*)theme {
  NSColor* fillColor = nil;
  RenderingOption renderingOption = RenderingOption::NORMAL;

  switch (state) {
    case image_button_cell::kDefaultState:
      fillColor = theme->GetNSImageColorNamed(IDR_THEME_TAB_BACKGROUND);
      break;

    case image_button_cell::kHoverState:
      fillColor = theme->GetNSImageColorNamed(IDR_THEME_TAB_BACKGROUND);
      // When a custom theme highlight the entire area, otherwise only
      // highlight the interior and not the border.
      if (theme->HasCustomImage(IDR_THEME_TAB_BACKGROUND)) {
        renderingOption = RenderingOption::OVERLAY_LIGHTEN;
      } else if (theme->InIncognitoMode()) {
        renderingOption = RenderingOption::OVERLAY_LIGHTEN_INCOGNITO;
      } else {
        renderingOption = RenderingOption::INLAY_LIGHTEN;
      }

      break;

    case image_button_cell::kPressedState:
      fillColor = theme->GetNSImageColorNamed(IDR_THEME_TAB_BACKGROUND);
      break;

    case image_button_cell::kDefaultStateBackground:
    case image_button_cell::kHoverStateBackground:
      fillColor =
          theme->GetNSImageColorNamed(IDR_THEME_TAB_BACKGROUND_INACTIVE);
      break;

    default:
      fillColor = [NSColor redColor];
      // All states should be accounted for above.
      NOTREACHED();
  }

  base::scoped_nsobject<NewTabButtonCustomImageRep> imageRep =
      [[NewTabButtonCustomImageRep alloc]
          initWithDrawSelector:@selector(drawNewTabButtonImage:)
                      delegate:[NewTabButton class]];
  [imageRep setDestView:self];
  [imageRep setFillColor:fillColor];
  [imageRep setPatternPhasePosition:
      [[self window]
          themeImagePositionForAlignment:THEME_IMAGE_ALIGN_WITH_TAB_STRIP]];
  [imageRep setRenderingOption:renderingOption];

  NSImage* newTabButtonImage =
      [[[NSImage alloc] initWithSize:NSMakeSize(34, 17)] autorelease];
  [newTabButtonImage setCacheMode:NSImageCacheAlways];
  [newTabButtonImage addRepresentation:imageRep];

  return newTabButtonImage;
}

+ (NSBezierPath*)newTabButtonBezierPathWithInset:(CGFloat)inset {
  NSBezierPath* bezierPath = [NSBezierPath bezierPath];

  // Bottom edge.
  [bezierPath moveToPoint:NSMakePoint(19 - inset, 1.2825 + inset)];
  [bezierPath lineToPoint:NSMakePoint(10.45 + inset, 1.2825 + inset)];

  // Lower-left corner.
  [bezierPath curveToPoint:NSMakePoint(6.08 + inset, 2.85 + inset)
             controlPoint1:NSMakePoint(10.1664 + inset, 1.3965 + inset)
             controlPoint2:NSMakePoint(7.89222 + inset, 0.787708 + inset)];

  // Left side.
  [bezierPath lineToPoint:NSMakePoint(0.7125 + inset, 14.25 - inset)];

  // Upper-left corner.
  [bezierPath curveToPoint:NSMakePoint(1.71 + inset, 16.2688 - inset)
             controlPoint1:NSMakePoint(0.246496 + inset, 15.2613 - inset)
             controlPoint2:NSMakePoint(0.916972 + inset, 16.3489 - inset)];

  // Top edge.
  [bezierPath lineToPoint:NSMakePoint(23.275 - inset, 16.2688 - inset)];

  // Upper right corner.
  [bezierPath curveToPoint:NSMakePoint(27.645 - inset, 14.7012 - inset)
             controlPoint1:NSMakePoint(26.4376 - inset, 16.3305 - inset)
             controlPoint2:NSMakePoint(26.9257 - inset, 15.8059 - inset)];

  // Right side.
  [bezierPath lineToPoint:NSMakePoint(32.9543 - inset, 3.62561 + inset)];

  // Lower right corner.
  [bezierPath curveToPoint:NSMakePoint(32.015 - inset, 1.2825 + inset)
             controlPoint1:NSMakePoint(34.069 - inset, 1.45303 + inset)
             controlPoint2:NSMakePoint(31.0348 - inset, 1.31455 + inset)];

  [bezierPath closePath];

  return bezierPath;
}

+ (void)drawNewTabButtonImage:(NewTabButtonCustomImageRep*)imageRep {
  [[NSGraphicsContext currentContext]
      cr_setPatternPhase:[imageRep patternPhasePosition]
      forView:[imageRep destView]];

  NSBezierPath* bezierPath = [self newTabButtonBezierPathWithInset:0];
  CGContextRef context = static_cast<CGContextRef>(
      [[NSGraphicsContext currentContext] graphicsPort]);
  CGFloat lineWidth = LineWidthFromContext(context);
  [bezierPath setLineWidth:lineWidth];

  if ([imageRep fillColor]) {
    [[imageRep fillColor] set];
    [bezierPath fill];
  }

  static NSColor* strokeColor =
      [[NSColor colorWithCalibratedWhite:0 alpha:0.4] retain];
  [strokeColor set];
  [bezierPath stroke];

  // Bottom edge.
  bezierPath = [NSBezierPath bezierPath];
  [bezierPath moveToPoint:NSMakePoint(31, 1.2825)];
  [bezierPath lineToPoint:NSMakePoint(9, 1.2825)];
  static NSColor* bottomEdgeColor =
      [[NSColor colorWithCalibratedWhite:0.25 alpha:0.3] retain];
  [bottomEdgeColor set];
  [bezierPath setLineWidth:lineWidth];
  [bezierPath setLineCapStyle:NSRoundLineCapStyle];
  [bezierPath stroke];

  // Shadow beneath the bottom edge.
  NSAffineTransform* translateTransform = [NSAffineTransform transform];
  [translateTransform translateXBy:0 yBy:-lineWidth];
  [bezierPath transformUsingAffineTransform:translateTransform];
  static NSColor* shadowColor =
      [[NSColor colorWithCalibratedWhite:0.5 alpha:0.3] retain];
  [shadowColor set];
  [bezierPath stroke];

  static NSColor* lightColor =
      [[NSColor colorWithCalibratedWhite:1 alpha:0.35] retain];
  static NSColor* lightIncognitoColor =
      [[NSColor colorWithCalibratedWhite:1 alpha:0.15] retain];
  static NSColor* darkColor =
      [[NSColor colorWithCalibratedWhite:0 alpha:0.08] retain];

  CGFloat inset = -1;
  switch ([imageRep renderingOption]) {
    case RenderingOption::OVERLAY_LIGHTEN:
      [lightColor set];
      inset = 0;
      break;

    case RenderingOption::OVERLAY_LIGHTEN_INCOGNITO:
      [lightIncognitoColor set];
      inset = 0;
      break;

    case RenderingOption::OVERLAY_DARKEN:
      [darkColor set];
      NSRectFillUsingOperation(NSMakeRect(0, 0, 34, 17), NSCompositeSourceAtop);
      break;

    case RenderingOption::INLAY_LIGHTEN:
      [lightColor set];
      inset = 1;
      break;

    case RenderingOption::NORMAL:
      break;
  }
  if (inset != -1) {
    bezierPath = [self newTabButtonBezierPathWithInset:inset];
    [bezierPath setLineWidth:lineWidth];
    [bezierPath fill];
  }
}

- (NSImage*)imageWithFillColor:(NSColor*)fillColor {
  NSImage* image =
      [[[NSImage alloc] initWithSize:NSMakeSize(34, 17)] autorelease];

  [image lockFocus];
  [fillColor set];
  [[NewTabButton newTabButtonBezierPathWithInset:0] fill];
  [image unlockFocus];
  return image;
}

@end
