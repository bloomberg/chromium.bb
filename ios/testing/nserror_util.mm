// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/nserror_util.h"

#import <Foundation/Foundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace testing {

NSError* NSErrorWithLocalizedDescription(NSString* error_description) {
  NSDictionary* userInfo = @{
    NSLocalizedDescriptionKey : error_description,
  };

  return [[NSError alloc] initWithDomain:@"com.google.chrome.errorDomain"
                                    code:0
                                userInfo:userInfo];
}

}  // namespace testing
