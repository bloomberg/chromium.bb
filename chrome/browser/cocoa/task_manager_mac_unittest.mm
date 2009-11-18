// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/task_manager_mac.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class TaskManagerWindowControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_ = [[TaskManagerWindowController alloc] init];
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

  TaskManagerWindowController *controller_;
};

// Test creation, to ensure nothing leaks or crashes
TEST_F(TaskManagerWindowControllerTest, Init) {
}

// TODO(thakis): Add tests for more methods as they become implemented
// (TaskManager::Show() etc).

}  // namespace
