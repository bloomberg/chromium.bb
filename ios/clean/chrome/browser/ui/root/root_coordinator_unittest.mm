// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/root/root_coordinator.h"

#import "ios/clean/chrome/browser/url_opening.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using RootCoordinatorTest = PlatformTest;

// Tests that RootCoordinator conforms to the URLOpening protocol.
TEST_F(RootCoordinatorTest, TestConformsToURLOpening) {
  EXPECT_TRUE([RootCoordinator conformsToProtocol:@protocol(URLOpening)]);
}

}  // namespace
