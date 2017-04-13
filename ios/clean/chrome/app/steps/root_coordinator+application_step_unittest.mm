// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/root_coordinator+application_step.h"

#include "base/macros.h"
#import "ios/clean/chrome/app/application_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class RootCoordinatorApplicationStepTest : public PlatformTest {
 public:
  RootCoordinatorApplicationStepTest() {
    coordinator_ = [[RootCoordinator alloc] init];
    state_ = [[ApplicationState alloc] init];
    window_ = OCMClassMock([UIWindow class]);
  }

 protected:
  RootCoordinator* coordinator_;
  ApplicationState* state_;
  id window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RootCoordinatorApplicationStepTest);
};

// Tests that the coordinator can run if the window is key and application is
// foregrounded.
TEST_F(RootCoordinatorApplicationStepTest, TestRunsInKeyWindow) {
  state_.phase = APPLICATION_FOREGROUNDED;
  state_.window = window_;
  OCMStub([window_ isKeyWindow]).andReturn(YES);
  EXPECT_TRUE([coordinator_ canRunInState:state_]);
}

// Tests that the coordinator cannot run if the window is not key.
TEST_F(RootCoordinatorApplicationStepTest, TestCannotRunWithoutKeyWindow) {
  state_.phase = APPLICATION_FOREGROUNDED;
  state_.window = window_;
  OCMStub([window_ isKeyWindow]).andReturn(NO);
  EXPECT_FALSE([coordinator_ canRunInState:state_]);
}

// Tests that the coordinator cannot run if application is not foregrounded.
TEST_F(RootCoordinatorApplicationStepTest, TestCannotRunIfNotForegrounded) {
  state_.phase = APPLICATION_COLD;
  state_.window = window_;
  OCMStub([window_ isKeyWindow]).andReturn(YES);
  EXPECT_FALSE([coordinator_ canRunInState:state_]);
}

}  // namespace
