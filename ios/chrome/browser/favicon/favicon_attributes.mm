// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/favicon_attributes.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FaviconAttributes
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
    _faviconImage = image;
    _monogramString = [monogram copy];
    _textColor = textColor;
    _backgroundColor = backgroundColor;
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
  return [[self alloc] initWithImage:image];
}

+ (instancetype)attributesWithMonogram:(NSString*)monogram
                             textColor:(UIColor*)textColor
                       backgroundColor:(UIColor*)backgroundColor {
  return [[self alloc] initWithMonogram:monogram
                              textColor:textColor
                        backgroundColor:backgroundColor];
}

@end
