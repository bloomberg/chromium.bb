// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/favicon_util_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

const CGFloat kVectorIconSize = 16;

}  // namespace

// A temporary class that draws the default favicon using vector commands.
// This class will be removed once a more general solution that works for all
// platforms is developed.
@interface DefaultFaviconImageRep : NSCustomImageRep
@property(retain, nonatomic) NSColor* strokeColor;

+ (NSImage*)imageForColor:(SkColor)strokeColor;

// NSCustomImageRep delegate method that performs the drawing.
+ (void)drawDefaultFavicon:(DefaultFaviconImageRep*)imageRep;

@end

@implementation DefaultFaviconImageRep

@synthesize strokeColor = strokeColor_;

- (void)dealloc {
  [strokeColor_ release];
  [super dealloc];
}

+ (NSImage*)imageForColor:(SkColor)strokeColor {
  base::scoped_nsobject<DefaultFaviconImageRep> imageRep(
      [[DefaultFaviconImageRep alloc]
          initWithDrawSelector:@selector(drawDefaultFavicon:)
                      delegate:[DefaultFaviconImageRep class]]);
  [imageRep setStrokeColor:skia::SkColorToSRGBNSColor(strokeColor)];

  // Create the image from the image rep.
  const NSSize imageSize = NSMakeSize(kVectorIconSize, kVectorIconSize);
  NSImage* faviconImage =
      [[[NSImage alloc] initWithSize:imageSize] autorelease];
  [faviconImage setCacheMode:NSImageCacheAlways];
  [faviconImage addRepresentation:imageRep];

  [imageRep setSize:imageSize];

  return faviconImage;
}

+ (void)drawDefaultFavicon:(DefaultFaviconImageRep*)imageRep {
  // Translate by 1/2pt to ensure crisp lines.
  CGContextRef context = static_cast<CGContextRef>(
      [[NSGraphicsContext currentContext] graphicsPort]);
  CGContextTranslateCTM(context, 0.5, 0.5);

  NSBezierPath* iconPath = [NSBezierPath bezierPath];

  // Create the horizontal and vertical parts of the shape.
  [iconPath moveToPoint:NSMakePoint(3, 1)];
  [iconPath relativeLineToPoint:NSMakePoint(0, 13)];
  [iconPath relativeLineToPoint:NSMakePoint(5, 0)];
  [iconPath relativeLineToPoint:NSMakePoint(0, -4)];
  [iconPath relativeLineToPoint:NSMakePoint(4, 0)];
  [iconPath relativeLineToPoint:NSMakePoint(0, -9)];
  [iconPath closePath];

  // Add the diagonal line.
  [iconPath moveToPoint:NSMakePoint(8, 14)];
  [iconPath relativeLineToPoint:NSMakePoint(4, -4)];

  // Draw it in the desired color.
  [[imageRep strokeColor] set];
  [iconPath stroke];
}

@end

namespace mac {

NSImage* FaviconForWebContents(content::WebContents* contents, SkColor color) {
  favicon::FaviconDriver* favicon_driver =
      contents ? favicon::ContentFaviconDriver::FromWebContents(contents)
               : nullptr;
  if (favicon_driver && favicon_driver->FaviconIsValid()) {
    NSImage* image = favicon_driver->GetFavicon().AsNSImage();
    // The |image| could be nil if the bitmap is null. In that case, fallback
    // to the default image.
    if (image) {
      return image;
    }
  }

  if (ui::MaterialDesignController::IsModeMaterial())
    return [DefaultFaviconImageRep imageForColor:color];

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToNSImage();
}

}  // namespace mac
