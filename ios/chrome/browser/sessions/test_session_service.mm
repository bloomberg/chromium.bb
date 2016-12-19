// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/test_session_service.h"

@implementation TestSessionService
- (void)saveWindow:(SessionWindowIOS*)window
    forBrowserState:(ios::ChromeBrowserState*)browserState
        immediately:(BOOL)immediately {
  [NSKeyedArchiver archivedDataWithRootObject:window];
}
@end
