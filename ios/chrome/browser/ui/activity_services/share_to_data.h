// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/activity_services/chrome_activity_item_thumbnail_generator.h"
#include "url/gurl.h"

@interface ShareToData : NSObject

// Designated initializer.
- (id)initWithURL:(const GURL&)url
                 title:(NSString*)title
       isOriginalTitle:(BOOL)isOriginalTitle
       isPagePrintable:(BOOL)isPagePrintable
    thumbnailGenerator:(ThumbnailGeneratorBlock)thumbnailGenerator;

@property(nonatomic, readonly) const GURL& url;
// NSURL version of 'url'. Use only for passing to libraries that take NSURL.
@property(strong, nonatomic, readonly) NSURL* nsurl;
@property(nonatomic, readonly, copy) NSString* title;
@property(nonatomic, readonly, assign) BOOL isOriginalTitle;
@property(nonatomic, readonly, assign) BOOL isPagePrintable;
@property(nonatomic, strong) UIImage* image;
@property(nonatomic, copy) ThumbnailGeneratorBlock thumbnailGenerator;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_H_
