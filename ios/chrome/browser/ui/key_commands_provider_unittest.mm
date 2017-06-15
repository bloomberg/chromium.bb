// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/key_commands_provider.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef PlatformTest KeyCommandsProviderTest;

TEST(KeyCommandsProviderTest, NoTabs_EditingText_ReturnsObjects) {
  KeyCommandsProvider* provider = [[KeyCommandsProvider alloc] init];
  id mockConsumer =
      [OCMockObject mockForProtocol:@protocol(KeyCommandsPlumbing)];
  [[[mockConsumer expect] andReturnUnsignedInteger:0] tabsCount];

  EXPECT_NE(nil,
            [provider keyCommandsForConsumer:mockConsumer editingText:YES]);
}

TEST(KeyCommandsProviderTest, ReturnsKeyCommandsObjects) {
  KeyCommandsProvider* provider = [[KeyCommandsProvider alloc] init];
  id mockConsumer =
      [OCMockObject mockForProtocol:@protocol(KeyCommandsPlumbing)];
  [[[mockConsumer expect] andReturnUnsignedInteger:0] tabsCount];

  for (id element in
       [provider keyCommandsForConsumer:mockConsumer editingText:YES]) {
    EXPECT_TRUE([element isKindOfClass:[UIKeyCommand class]]);
  }
}

TEST(KeyCommandsProviderTest, MoreKeyboardCommandsWhenTabs) {
  KeyCommandsProvider* provider = [[KeyCommandsProvider alloc] init];
  id mockConsumer =
      [OCMockObject mockForProtocol:@protocol(KeyCommandsPlumbing)];

  // No tabs.
  [[[mockConsumer expect] andReturnUnsignedInteger:0] tabsCount];
  NSUInteger numberOfKeyCommandsWithoutTabs =
      [[provider keyCommandsForConsumer:mockConsumer editingText:NO] count];

  // Tabs.
  [[[mockConsumer expect] andReturnUnsignedInteger:1] tabsCount];
  NSUInteger numberOfKeyCommandsWithTabs =
      [[provider keyCommandsForConsumer:mockConsumer editingText:NO] count];

  EXPECT_GT(numberOfKeyCommandsWithTabs, numberOfKeyCommandsWithoutTabs);
}

TEST(KeyCommandsProviderTest, LessKeyCommandsWhenTabsAndEditingText) {
  KeyCommandsProvider* provider = [[KeyCommandsProvider alloc] init];
  id mockConsumer =
      [OCMockObject mockForProtocol:@protocol(KeyCommandsPlumbing)];

  // Not editing text.
  [[[mockConsumer expect] andReturnUnsignedInteger:1] tabsCount];
  NSUInteger numberOfKeyCommandsWhenNotEditingText =
      [[provider keyCommandsForConsumer:mockConsumer editingText:NO] count];

  // Editing text.
  [[[mockConsumer expect] andReturnUnsignedInteger:1] tabsCount];
  NSUInteger numberOfKeyCommandsWhenEditingText =
      [[provider keyCommandsForConsumer:mockConsumer editingText:YES] count];

  EXPECT_LT(numberOfKeyCommandsWhenEditingText,
            numberOfKeyCommandsWhenNotEditingText);
}

}  // namespace
