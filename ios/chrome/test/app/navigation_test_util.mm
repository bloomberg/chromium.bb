// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/app/navigation_test_util.h"

#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/navigation_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::WaitUntilConditionOrTimeout;

namespace chrome_test_util {

void LoadUrl(const GURL& url) {
  web::test::LoadUrl(GetCurrentWebState(), url);
}

bool IsLoading() {
  return GetCurrentWebState()->IsLoading();
}

bool WaitForPageToFinishLoading() {
  return WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
    return !IsLoading();
  });
}

}  // namespace chrome_test_util
