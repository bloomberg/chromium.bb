// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/page_not_available_controller.h"

namespace {

TEST(PageNotAvailableControllerTest, TestInitWithURL) {
  GURL url = GURL("http://foo.bar.com");
  base::scoped_nsobject<PageNotAvailableController> controller(
      [[PageNotAvailableController alloc] initWithUrl:url]);
  EXPECT_EQ(url, controller.get().url);
  EXPECT_TRUE(controller.get().view);
}

}  // namespace
