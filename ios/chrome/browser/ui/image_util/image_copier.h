// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_IMAGE_UTIL_IMAGE_COPIER_H_
#define IOS_CHROME_BROWSER_UI_IMAGE_UTIL_IMAGE_COPIER_H_

#import <UIKit/UIKit.h>

#include "components/image_fetcher/core/request_metadata.h"

typedef int64_t ImageCopierSessionId;

// Object copying images to the system's pasteboard.
// TODO(crbug.com/163201): Current implementation won't terminate the
// downloading process when user clicks "Cancel", because
// IOSImageDataFetcherWrapper doesn't provide such API. And a progress bar is
// preferred for this, but no API for that either. Future steps would be using
// NSURLSessionTask to provide functionality of reporting progress.
@interface ImageCopier : NSObject

// Init the ImageCopier with a |baseViewController| used to display alerts.
- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController;

// Begins a session of downloading image, and returns an identifier for this
// session.
- (ImageCopierSessionId)beginSession;

// Ends the downloading session. The image data won't be copied if the user has
// canceled copying.
- (void)endSession:(ImageCopierSessionId)image_id withImageData:(NSData*)data;

@end

#endif  // IOS_CHROME_BROWSER_UI_IMAGE_UTIL_IMAGE_COPIER_H_
