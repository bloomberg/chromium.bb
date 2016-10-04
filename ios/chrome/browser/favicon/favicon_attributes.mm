// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/favicon_attributes.h"

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"

@implementation FaviconAttributes {
  base::mac::ObjCPropertyReleaser _propertyReleaser_FaviconAttributes;
}
@synthesize faviconImage = _faviconImage;
@synthesize monogramString = _monogramString;
@synthesize textColor = _textColor;
@synthesize backgroundColor = _backgroundColor;

- (instancetype)initWithImage:(UIImage*)image
                     monogram:(NSString*)monogram
                    textColor:(UIColor*)textColor
              backgroundColor:(UIColor*)backgroundColor {
  DCHECK(image || (monogram && textColor && backgroundColor));
  self = [super init];
  if (self) {
    _propertyReleaser_FaviconAttributes.Init(self, [FaviconAttributes class]);
    _faviconImage = [image retain];
    _monogramString = [monogram copy];
    _textColor = [textColor retain];
    _backgroundColor = [backgroundColor retain];
  }

  return self;
}

- (instancetype)initWithImage:(UIImage*)image {
  DCHECK(image);
  return
      [self initWithImage:image monogram:nil textColor:nil backgroundColor:nil];
}

- (instancetype)initWithMonogram:(NSString*)monogram
                       textColor:(UIColor*)textColor
                 backgroundColor:(UIColor*)backgroundColor {
  DCHECK(monogram && textColor && backgroundColor);
  return [self initWithImage:nil
                    monogram:monogram
                   textColor:textColor
             backgroundColor:backgroundColor];
}

+ (instancetype)attributesWithImage:(UIImage*)image {
  return [[[self alloc] initWithImage:image] autorelease];
}

+ (instancetype)attributesWithMonogram:(NSString*)monogram
                             textColor:(UIColor*)textColor
                       backgroundColor:(UIColor*)backgroundColor {
  return [[[self alloc] initWithMonogram:monogram
                               textColor:textColor
                         backgroundColor:backgroundColor] autorelease];
}

@end
