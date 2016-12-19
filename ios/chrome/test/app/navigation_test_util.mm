// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/app/navigation_test_util.h"

#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/test/navigation_test_util.h"

namespace chrome_test_util {

void LoadUrl(const GURL& url) {
  web::test::LoadUrl(GetCurrentWebState(), url);
}

bool IsLoading() {
  return GetCurrentWebState()->IsLoading();
}

}  // namespace chrome_test_util
