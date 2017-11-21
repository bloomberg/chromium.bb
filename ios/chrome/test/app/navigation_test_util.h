// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_NAVIGATION_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_NAVIGATION_TEST_UTIL_H_

#include "base/compiler_specific.h"
#include "url/gurl.h"

namespace chrome_test_util {

// Loads |url| in the current WebState with transition of type
// ui::PAGE_TRANSITION_TYPED.
void LoadUrl(const GURL& url);

// Returns true if the current page in the current WebState is loading.
bool IsLoading();

// Returns true if the current page in the current WebState finishes loading
// within a timeout.
bool WaitForPageToFinishLoading() WARN_UNUSED_RESULT;

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_NAVIGATION_TEST_UTIL_H_
