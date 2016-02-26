// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/keep_alive_registry.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test the WillKeepAlive state and when we interact with the browser with
// a KeepAlive registered.
TEST(KeepAliveRegistryTest, BasicKeepAliveTest) {
  const int base_keep_alive_count = chrome::GetKeepAliveCountForTesting();
  KeepAliveRegistry* registry = KeepAliveRegistry::GetInstance();

  EXPECT_FALSE(registry->WillKeepAlive());

  {
    // Arbitrarily chosen Origin
    ScopedKeepAlive test_keep_alive(KeepAliveOrigin::CHROME_APP_DELEGATE);

    // We should require the browser to stay alive
    EXPECT_EQ(base_keep_alive_count + 1, chrome::GetKeepAliveCountForTesting());
    EXPECT_TRUE(registry->WillKeepAlive());
  }

  // We should be back to normal now.
  EXPECT_EQ(base_keep_alive_count, chrome::GetKeepAliveCountForTesting());
  EXPECT_FALSE(registry->WillKeepAlive());
}

// Test the WillKeepAlive state and when we interact with the browser with
// more than one KeepAlive registered.
TEST(KeepAliveRegistryTest, DoubleKeepAliveTest) {
  const int base_keep_alive_count = chrome::GetKeepAliveCountForTesting();
  KeepAliveRegistry* registry = KeepAliveRegistry::GetInstance();
  scoped_ptr<ScopedKeepAlive> keep_alive_1, keep_alive_2;

  keep_alive_1.reset(new ScopedKeepAlive(KeepAliveOrigin::CHROME_APP_DELEGATE));
  EXPECT_EQ(base_keep_alive_count + 1, chrome::GetKeepAliveCountForTesting());
  EXPECT_TRUE(registry->WillKeepAlive());

  keep_alive_2.reset(new ScopedKeepAlive(KeepAliveOrigin::CHROME_APP_DELEGATE));
  // We should not increment the count twice
  EXPECT_EQ(base_keep_alive_count + 1, chrome::GetKeepAliveCountForTesting());
  EXPECT_TRUE(registry->WillKeepAlive());

  keep_alive_1.reset();
  // We should not decrement the count before the last keep alive is released.
  EXPECT_EQ(base_keep_alive_count + 1, chrome::GetKeepAliveCountForTesting());
  EXPECT_TRUE(registry->WillKeepAlive());

  keep_alive_2.reset();
  EXPECT_EQ(base_keep_alive_count, chrome::GetKeepAliveCountForTesting());
  EXPECT_FALSE(registry->WillKeepAlive());
}
