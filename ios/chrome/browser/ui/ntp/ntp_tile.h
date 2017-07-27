// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NTP_TILE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NTP_TILE_H_

#import <UIKit/UIKit.h>

// This class stores all the data associated with an NTP Most Visited tile
// suggestion in an NSCoding-enabled format.
@interface NTPTile : NSObject<NSCoding>

// The most visited site's title.
@property(readonly, atomic) NSString* title;
// The most visited site's URL.
@property(readonly, atomic) NSURL* URL;
// The filename of the most visited site's favicon on disk, if it exists.
@property(strong, atomic) NSString* faviconPath;
// The fallback text color for the most visited site, if it exists.
@property(strong, atomic) UIColor* fallbackTextColor;
// The fallback background color for the most visited site, if it exists.
@property(strong, atomic) UIColor* fallbackBackgroundColor;
// Whether the fallback background color for the most visited site is the
// default color.
@property(assign, atomic) BOOL fallbackIsDefaultColor;
// Whether the favicon has been fetched for the most visited site. This can be
// YES with no fallback values or favicon path.
@property(assign, atomic) BOOL faviconFetched;

- (instancetype)initWithTitle:(NSString*)title URL:(NSURL*)URL;
- (instancetype)initWithTitle:(NSString*)title
                          URL:(NSURL*)URL
                  faviconPath:(NSString*)faviconPath
            fallbackTextColor:(UIColor*)fallbackTextColor
      fallbackBackgroundColor:(UIColor*)fallbackTextColor
       fallbackIsDefaultColor:(BOOL)fallbackIsDefaultColor
               faviconFetched:(BOOL)faviconFetched;
@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NTP_TILE_H_
