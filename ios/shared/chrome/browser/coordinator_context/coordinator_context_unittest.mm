// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

TEST(CoordinatorContextTest, Initialization) {
  CoordinatorContext* context = [[CoordinatorContext alloc] init];
  EXPECT_TRUE(context.animated);
}

}  // namespace
