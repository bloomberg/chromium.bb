// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_REQUEST_H_
#define IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_REQUEST_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/physical_web/physical_web_types.h"

// This class will perform a metadata service web request to expand the given
// URL and return the metadata of the web page such as the title.

@interface PhysicalWebRequest : NSObject

// Initializes a metadata service web request with information about the
// physical web device.
- (instancetype)initWithDevice:(PhysicalWebDevice*)device
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns the URL sent in the query to the Physical Web service.
- (NSURL*)requestURL;

// Starts the request and call the given |block| when finished.
// |block| should not be nil.
// If an error occurred, the block will be called with a non-nil error.
// When no error occurred, the result will be available in a PhysicalWebDevice
// data structure.
- (void)start:(physical_web::RequestFinishedBlock)block;

// Cancels the request.
- (void)cancel;

@end

#endif  // IOS_CHROME_COMMON_PHYSICAL_WEB_PHYSICAL_WEB_REQUEST_H_
