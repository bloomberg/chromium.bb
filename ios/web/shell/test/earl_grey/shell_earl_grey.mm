// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"

#import <EarlGrey/EarlGrey.h>

#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#include "ios/web/shell/test/app/navigation_test_util.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShellEarlGrey

+ (bool)loadURL:(const GURL&)URL {
  web::shell_test_util::LoadUrl(URL);
  web::WebState* webState = web::shell_test_util::GetCurrentWebState();

  if (!web::test::WaitForPageToFinishLoading(webState))
    return false;

  if (webState->ContentIsHTML()) {
    if (!web::WaitUntilWindowIdInjected(webState)) {
      return false;
    }
  }

  // Ensure any UI elements handled by EarlGrey become idle for any subsequent
  // EarlGrey steps.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  return true;
}

+ (bool)waitForWebViewContainingText:(std::string)text {
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return web::test::IsWebViewContainingText(
        web::shell_test_util::GetCurrentWebState(), text);
  });
}

@end
