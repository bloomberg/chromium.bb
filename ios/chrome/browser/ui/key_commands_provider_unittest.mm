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

using KeyCommandsProviderTest = PlatformTest;

TEST_F(KeyCommandsProviderTest, NoTabs_EditingText_ReturnsObjects) {
  KeyCommandsProvider* provider = [[KeyCommandsProvider alloc] init];
  id mockConsumer =
      [OCMockObject mockForProtocol:@protocol(KeyCommandsPlumbing)];
  id<ApplicationCommands, BrowserCommands> dispatcher = nil;
  [[[mockConsumer expect] andReturnUnsignedInteger:0] tabsCount];

  EXPECT_NE(nil, [provider keyCommandsForConsumer:mockConsumer
                                       dispatcher:dispatcher
                                      editingText:YES]);
}

TEST_F(KeyCommandsProviderTest, ReturnsKeyCommandsObjects) {
  KeyCommandsProvider* provider = [[KeyCommandsProvider alloc] init];
  id mockConsumer =
      [OCMockObject mockForProtocol:@protocol(KeyCommandsPlumbing)];
  id<ApplicationCommands, BrowserCommands> dispatcher = nil;

  [[[mockConsumer expect] andReturnUnsignedInteger:0] tabsCount];

  for (id element in [provider keyCommandsForConsumer:mockConsumer
                                           dispatcher:dispatcher
                                          editingText:YES]) {
    EXPECT_TRUE([element isKindOfClass:[UIKeyCommand class]]);
  }
}

TEST_F(KeyCommandsProviderTest, MoreKeyboardCommandsWhenTabs) {
  KeyCommandsProvider* provider = [[KeyCommandsProvider alloc] init];
  id mockConsumer =
      [OCMockObject mockForProtocol:@protocol(KeyCommandsPlumbing)];
  id<ApplicationCommands, BrowserCommands> dispatcher = nil;

  // No tabs.
  [[[mockConsumer expect] andReturnUnsignedInteger:0] tabsCount];
  NSUInteger numberOfKeyCommandsWithoutTabs =
      [[provider keyCommandsForConsumer:mockConsumer
                             dispatcher:dispatcher
                            editingText:NO] count];

  // Tabs.
  [[[mockConsumer expect] andReturnUnsignedInteger:1] tabsCount];
  NSUInteger numberOfKeyCommandsWithTabs =
      [[provider keyCommandsForConsumer:mockConsumer
                             dispatcher:dispatcher
                            editingText:NO] count];

  EXPECT_GT(numberOfKeyCommandsWithTabs, numberOfKeyCommandsWithoutTabs);
}

TEST_F(KeyCommandsProviderTest, LessKeyCommandsWhenTabsAndEditingText) {
  KeyCommandsProvider* provider = [[KeyCommandsProvider alloc] init];
  id mockConsumer =
      [OCMockObject mockForProtocol:@protocol(KeyCommandsPlumbing)];
  id<ApplicationCommands, BrowserCommands> dispatcher = nil;

  // Not editing text.
  [[[mockConsumer expect] andReturnUnsignedInteger:1] tabsCount];
  NSUInteger numberOfKeyCommandsWhenNotEditingText =
      [[provider keyCommandsForConsumer:mockConsumer
                             dispatcher:dispatcher
                            editingText:NO] count];

  // Editing text.
  [[[mockConsumer expect] andReturnUnsignedInteger:1] tabsCount];
  NSUInteger numberOfKeyCommandsWhenEditingText =
      [[provider keyCommandsForConsumer:mockConsumer
                             dispatcher:dispatcher
                            editingText:YES] count];

  EXPECT_LT(numberOfKeyCommandsWhenEditingText,
            numberOfKeyCommandsWhenNotEditingText);
}

}  // namespace
