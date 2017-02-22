// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_CHROME_ACTIVITY_ITEM_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_CHROME_ACTIVITY_ITEM_SOURCE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_thumbnail_generator.h"

// Returns a text to the UIActivities that can take advantage of it.
@interface UIActivityTextSource : NSObject<UIActivityItemSource>

// Default initializer. |text| must not be nil.
- (instancetype)initWithText:(NSString*)text;

@end

// Returns an image to the UIActivities that can take advantage of it.
@interface UIActivityImageSource : NSObject<UIActivityItemSource>

// Default initializer. |image| must not be nil.
- (instancetype)initWithImage:(UIImage*)image;

@end

// This UIActivityItemSource-conforming object communicates with Password
// Management App Extensions by returning a NSDictionary with the URL of the
// current page *and* also conforms to UTType public.url so it can be used
// with other Social Sharing Extensions as well. The |subject| is used by
// Mail applications to pre-fill in the subject line. The |thumbnailGenerator|
// is used to provide thumbnails to extensions that request one.
@interface UIActivityURLSource : NSObject<UIActivityItemSource>

// Default initializer. |subject|, |url|, and |thumbnailGenerator| must not be
// nil.
- (instancetype)initWithURL:(NSURL*)url
                    subject:(NSString*)subject
         thumbnailGenerator:(ThumbnailGeneratorBlock)thumbnailGenerator;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_CHROME_ACTIVITY_ITEM_SOURCE_H_
