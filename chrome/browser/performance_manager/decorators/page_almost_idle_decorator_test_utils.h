// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_ALMOST_IDLE_DECORATOR_TEST_UTILS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_ALMOST_IDLE_DECORATOR_TEST_UTILS_H_

namespace performance_manager {

class PageNodeImpl;

namespace testing {

class PageAlmostIdleDecoratorTestUtils {
 public:
  // Drives the state transition machinery through the entire state machine to
  // the end point of being "loaded and idle".
  static void DrivePageToLoadedAndIdle(PageNodeImpl* page_node);
};

}  // namespace testing
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_ALMOST_IDLE_DECORATOR_TEST_UTILS_H_
