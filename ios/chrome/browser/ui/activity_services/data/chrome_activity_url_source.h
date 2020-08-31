// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_URL_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_URL_SOURCE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_item_source.h"

@class ChromeActivityItemThumbnailGenerator;

// This UIActivityItemSource-conforming object conforms to UTType public.url so
// it can be used with other Social Sharing Extensions as well. The |shareURL|
// is the URL shared with Social Sharing Extensions. The |subject| is used by
// Mail applications to pre-fill in the subject line. The |thumbnailGenerator|
// is used to provide thumbnails to extensions that request one.
@interface ChromeActivityURLSource : NSObject <ChromeActivityItemSource>

// Default initializer. |shareURL|, |subject|, and |thumbnailGenerator| must not
// be nil.
- (instancetype)initWithShareURL:(NSURL*)shareURL
                         subject:(NSString*)subject
              thumbnailGenerator:
                  (ChromeActivityItemThumbnailGenerator*)thumbnailGenerator;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_URL_SOURCE_H_
