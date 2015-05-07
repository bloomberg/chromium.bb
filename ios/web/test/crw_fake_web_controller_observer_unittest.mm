// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/crw_fake_web_controller_observer.h"

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace web {
namespace {
class CRWFakeWebControllerObserverTest : public PlatformTest {
 public:
  void SetUp() override {
    fake_web_controller_observer_.reset([[CRWFakeWebControllerObserver alloc]
        initWithCommandPrefix:@"test"]);
  }
 protected:
  base::scoped_nsobject<CRWFakeWebControllerObserver>
      fake_web_controller_observer_;
};

// Tests that a CRWFakeWebControllerObserver can be correctly initialized with
// a command prefix.
TEST_F(CRWFakeWebControllerObserverTest, CommandPrefix) {
  EXPECT_NSEQ(@"test", [fake_web_controller_observer_ commandPrefix]);
}

// Tests that the CRWFakeWebControllerObserver correctly stores a command
// received.
TEST_F(CRWFakeWebControllerObserverTest, Command) {
  // Arbitrary values.
  base::DictionaryValue command;
  command.SetBoolean("samp", true);
  [fake_web_controller_observer_ handleCommand:command
                                 webController:nil
                             userIsInteracting:NO
                                     originURL:GURL("http://google.com")];

  ScopedVector<base::DictionaryValue>& commands_received =
      [fake_web_controller_observer_ commandsReceived];
  EXPECT_EQ(1U, commands_received.size());
  bool samp = false;
  commands_received[0]->GetBoolean("samp", &samp);
  EXPECT_TRUE(samp);
}

}  // namespace
}  // namespace web
