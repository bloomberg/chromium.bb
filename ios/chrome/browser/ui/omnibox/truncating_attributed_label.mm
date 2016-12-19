// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/truncating_attributed_label.h"

#include <algorithm>

#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_cftyperef.h"

@interface OmniboxPopupTruncatingLabel ()
- (void)setup;
- (UIImage*)getLinearGradient:(CGRect)rect;
@end

@implementation OmniboxPopupTruncatingLabel {
  // Attributed text.
  base::scoped_nsobject<CATextLayer> textLayer_;
  // Gradient used to create fade effect. Changes based on view.frame size.
  base::scoped_nsobject<UIImage> gradient_;

  base::mac::ObjCPropertyReleaser propertyReleaser_OmniboxPopupTruncatingLabel_;
}

@synthesize truncateMode = truncateMode_;
@synthesize attributedText = attributedText_;
@synthesize highlighted = highlighted_;
@synthesize highlightedText = highlightedText_;
@synthesize textAlignment = textAlignment_;

- (void)setup {
  self.backgroundColor = [UIColor clearColor];
  self.contentMode = UIViewContentModeRedraw;
  truncateMode_ = OmniboxPopupTruncatingTail;

  // Disable animations in CATextLayer.
  textLayer_.reset([[CATextLayer layer] retain]);
  base::scoped_nsobject<NSDictionary> actions([[NSDictionary alloc]
      initWithObjectsAndKeys:[NSNull null], @"contents", nil]);
  [textLayer_ setActions:actions];
  [textLayer_ setFrame:self.bounds];
  [textLayer_ setContentsScale:[[UIScreen mainScreen] scale]];
}

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    propertyReleaser_OmniboxPopupTruncatingLabel_.Init(
        self, [OmniboxPopupTruncatingLabel class]);
    [self setup];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self setup];
}

- (void)setFrame:(CGRect)frame {
  [super setFrame:frame];
  [textLayer_ setFrame:self.bounds];

  // Cache the fade gradient when the frame changes.
  if (!CGRectIsEmpty(frame) &&
      (!gradient_.get() || !CGSizeEqualToSize([gradient_ size], frame.size))) {
    CGRect rect = CGRectMake(0, 0, frame.size.width, frame.size.height);
    gradient_.reset([[self getLinearGradient:rect] retain]);
  }
}

- (void)drawRect:(CGRect)rect {
  if ([attributedText_ length] == 0)
    return;

  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSaveGState(context);
  [textLayer_ setString:highlighted_ ? highlightedText_ : attributedText_];
  CGContextClipToMask(context, self.bounds, [gradient_ CGImage]);
  [textLayer_ renderInContext:context];

  CGContextRestoreGState(context);
}

- (void)setTextAlignment:(NSTextAlignment)textAlignment {
  if (textAlignment == NSTextAlignmentLeft) {
    [textLayer_ setAlignmentMode:kCAAlignmentLeft];
    self.truncateMode = OmniboxPopupTruncatingTail;
  } else if (textAlignment == NSTextAlignmentRight) {
    [textLayer_ setAlignmentMode:kCAAlignmentRight];
    self.truncateMode = OmniboxPopupTruncatingHead;
  } else if (textAlignment == NSTextAlignmentNatural) {
    [textLayer_ setAlignmentMode:kCAAlignmentNatural];
    self.truncateMode = OmniboxPopupTruncatingTail;
  } else {
    NOTREACHED();
  }
  if (textAlignment != textAlignment_)
    gradient_.reset();
  textAlignment_ = textAlignment;
}

// Create gradient opacity mask based on direction.
- (UIImage*)getLinearGradient:(CGRect)rect {
  // Create an opaque context.
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
  CGContextRef context =
      CGBitmapContextCreate(NULL, rect.size.width, rect.size.height, 8,
                            4 * rect.size.width, colorSpace, kCGImageAlphaNone);

  // White background will mask opaque, black gradient will mask transparent.
  CGContextSetFillColorWithColor(context, [UIColor whiteColor].CGColor);
  CGContextFillRect(context, rect);

  // Create gradient from white to black.
  CGFloat locs[2] = {0.0f, 1.0f};
  CGFloat components[4] = {1.0f, 1.0f, 0.0f, 1.0f};
  CGGradientRef gradient =
      CGGradientCreateWithColorComponents(colorSpace, components, locs, 2);
  CGColorSpaceRelease(colorSpace);

  // Draw head and/or tail gradient.
  CGFloat fadeWidth =
      std::min(rect.size.height * 2, (CGFloat)floor(rect.size.width / 4));
  CGFloat minX = CGRectGetMinX(rect);
  CGFloat maxX = CGRectGetMaxX(rect);
  if (self.truncateMode & OmniboxPopupTruncatingTail) {
    CGFloat startX = maxX - fadeWidth;
    CGPoint startPoint = CGPointMake(startX, CGRectGetMidY(rect));
    CGPoint endPoint = CGPointMake(maxX, CGRectGetMidY(rect));
    CGContextDrawLinearGradient(context, gradient, startPoint, endPoint, 0);
  }
  if (self.truncateMode & OmniboxPopupTruncatingHead) {
    CGFloat startX = minX + fadeWidth;
    CGPoint startPoint = CGPointMake(startX, CGRectGetMidY(rect));
    CGPoint endPoint = CGPointMake(minX, CGRectGetMidY(rect));
    CGContextDrawLinearGradient(context, gradient, startPoint, endPoint, 0);
  }
  CGGradientRelease(gradient);

  // Clean up, return image.
  CGImageRef ref = CGBitmapContextCreateImage(context);
  UIImage* image = [UIImage imageWithCGImage:ref];
  CGImageRelease(ref);
  CGContextRelease(context);
  return image;
}

@end
