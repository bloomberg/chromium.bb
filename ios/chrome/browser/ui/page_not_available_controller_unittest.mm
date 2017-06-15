// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include "base/logging.h"
#import "ios/chrome/browser/ui/page_not_available_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

TEST(PageNotAvailableControllerTest, TestInitWithURL) {
  GURL url = GURL("http://foo.bar.com");
  PageNotAvailableController* controller =
      [[PageNotAvailableController alloc] initWithUrl:url];
  EXPECT_EQ(url, controller.url);
  EXPECT_TRUE(controller.view);
}

}  // namespace
