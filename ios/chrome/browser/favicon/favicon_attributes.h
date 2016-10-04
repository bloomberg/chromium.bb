// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_FAVICON_ATTRIBUTES_H_
#define IOS_CHROME_BROWSER_FAVICON_FAVICON_ATTRIBUTES_H_

#import <UIKit/UIKit.h>

// Attributes of a favicon. A favicon is represented either with an image or
// with a fallback monogram of a given color and background color.
@interface FaviconAttributes : NSObject

// Favicon image. Can be nil. If it is nil, monogram string and color are
// guaranteed to be not nil.
@property(nonatomic, readonly, strong) UIImage* faviconImage;
// Favicon monogram. Only available when there is no image.
@property(nonatomic, readonly, copy) NSString* monogramString;
// Favicon monogram color. Only available when there is no image.
@property(nonatomic, readonly, strong) UIColor* textColor;
// Favicon monogram background color. Only available when there is no image.
@property(nonatomic, readonly, strong) UIColor* backgroundColor;

+ (instancetype)attributesWithImage:(UIImage*)image;
+ (instancetype)attributesWithMonogram:(NSString*)monogram
                             textColor:(UIColor*)textColor
                       backgroundColor:(UIColor*)backgroundColor;

// Designated initializer. Either |image| or all of |textColor|,
// |backgroundColor| and |monogram| must be not nil.
- (instancetype)initWithImage:(UIImage*)image
                     monogram:(NSString*)monogram
                    textColor:(UIColor*)textColor
              backgroundColor:(UIColor*)backgroundColor
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithImage:(UIImage*)image;
- (instancetype)initWithMonogram:(NSString*)monogram
                       textColor:(UIColor*)textColor
                 backgroundColor:(UIColor*)backgroundColor;

- (instancetype)init NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_FAVICON_FAVICON_ATTRIBUTES_H_
