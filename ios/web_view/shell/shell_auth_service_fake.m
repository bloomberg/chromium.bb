// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_auth_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Fake implementation of ShellAuthService.
@implementation ShellAuthService

- (NSArray<CWVIdentity*>*)identities {
  return @[];
}

#pragma mark CWVSyncControllerDataSource

- (void)syncController:(CWVSyncController*)syncController
    getAccessTokenForScopes:(NSArray<NSString*>*)scopes
          completionHandler:(void (^)(NSString* accessToken,
                                      NSDate* expirationDate,
                                      NSError* error))completionHandler {
  // Always returns an error.
  if (completionHandler) {
    completionHandler(
        nil, nil,
        [NSError errorWithDomain:@"org.chromium.chromewebview.shell"
                            code:0
                        userInfo:nil]);
  }
}

@end
