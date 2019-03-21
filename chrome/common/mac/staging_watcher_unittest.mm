// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/staging_watcher.h"

#include <dispatch/dispatch.h>

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

enum class KVOOrNot { kUseKVO, kDontUseKVO };

class StagingKeyWatcherTest : public testing::TestWithParam<KVOOrNot> {
 public:
  StagingKeyWatcherTest() = default;
  ~StagingKeyWatcherTest() = default;

 protected:
  void SetUp() override {
    testingBundleID_.reset([[NSString alloc]
        initWithFormat:@"org.chromium.StagingKeyWatcherTest.%d", getpid()]);
    defaults_.reset(
        [[NSUserDefaults alloc] initWithSuiteName:testingBundleID_]);

    [defaults_ removeObjectForKey:[CrStagingKeyWatcher stagingKeyForTesting]];
  }

  void TearDown() override {
    [defaults_ removeObjectForKey:[CrStagingKeyWatcher stagingKeyForTesting]];
  }

  CrStagingKeyWatcher* CreateKeyWatcher() {
    keyWatcher_.reset(
        [[CrStagingKeyWatcher alloc] initWithUserDefaults:defaults_]);
    if (GetParam() == KVOOrNot::kDontUseKVO)
      [keyWatcher_ disableKVOForTesting];

    return keyWatcher_;
  }

  void SetDefaultsValue(id value) {
    [defaults_ setObject:value
                  forKey:[CrStagingKeyWatcher stagingKeyForTesting]];
  }

  void ClearDefaultsValueInSeparateProcess() {
    [NSTask launchedTaskWithLaunchPath:@"/usr/bin/defaults"
                             arguments:@[
                               @"delete", testingBundleID_.get(),
                               [CrStagingKeyWatcher stagingKeyForTesting]
                             ]];
  }

 private:
  base::scoped_nsobject<CrStagingKeyWatcher> keyWatcher_;
  base::scoped_nsobject<NSString> testingBundleID_;
  base::scoped_nsobject<NSUserDefaults> defaults_;
};

INSTANTIATE_TEST_SUITE_P(KVOandNot,
                         StagingKeyWatcherTest,
                         testing::Values(KVOOrNot::kUseKVO,
                                         KVOOrNot::kDontUseKVO));

}  // namespace

TEST_P(StagingKeyWatcherTest, NoBlockingWhenNoKey) {
  CrStagingKeyWatcher* watcher = CreateKeyWatcher();
  [watcher waitForStagingKey];
  ASSERT_FALSE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, NoBlockingWhenWrongKeyType) {
  SetDefaultsValue(@"this is not an string array");

  CrStagingKeyWatcher* watcher = CreateKeyWatcher();
  [watcher waitForStagingKey];
  ASSERT_FALSE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, NoBlockingWhenWrongArrayType) {
  SetDefaultsValue(@[ @3, @1, @4, @1, @5 ]);

  CrStagingKeyWatcher* watcher = CreateKeyWatcher();
  [watcher waitForStagingKey];
  ASSERT_FALSE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, NoBlockingWhenEmptyArray) {
  SetDefaultsValue(@[]);

  CrStagingKeyWatcher* watcher = CreateKeyWatcher();
  [watcher waitForStagingKey];
  ASSERT_FALSE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, BlockFunctionality) {
  NSString* appPath = [base::mac::OuterBundle() bundlePath];
  SetDefaultsValue(@[ appPath ]);

  NSRunLoop* runloop = [NSRunLoop currentRunLoop];
  ASSERT_EQ(nil, [runloop currentMode]);

  dispatch_async(dispatch_get_main_queue(), ^{
    ASSERT_NE(nil, [runloop currentMode]);
    ClearDefaultsValueInSeparateProcess();
  });

  CrStagingKeyWatcher* watcher = CreateKeyWatcher();
  [watcher waitForStagingKey];
  ASSERT_TRUE([watcher lastWaitWasBlockedForTesting]);
}
