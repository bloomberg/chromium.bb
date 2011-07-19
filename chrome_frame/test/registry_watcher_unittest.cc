// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/registry_watcher.h"

#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

const wchar_t kTestRoot[] = L"CFRegistryWatcherTest";
const wchar_t kTestWindowClass[] = L"TestWndClass";
const wchar_t kTestWindowName[] = L"TestWndName";

// Give notifications 30 seconds to stick. Hopefully long enough to avoid
// flakiness.
const int64 kWaitSeconds = 30;

class RegistryWatcherUnittest : public testing::Test {
 public:
  void SetUp() {
    // Create a temporary key for testing
    temp_key_.Open(HKEY_CURRENT_USER, L"", KEY_QUERY_VALUE);
    temp_key_.DeleteKey(kTestRoot);
    ASSERT_NE(ERROR_SUCCESS, temp_key_.Open(HKEY_CURRENT_USER,
                                            kTestRoot,
                                            KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS,
        temp_key_.Create(HKEY_CURRENT_USER, kTestRoot, KEY_READ));

    reg_change_count_ = 0;
  }

  void TearDown() {
    // Clean up the temporary key
    temp_key_.Open(HKEY_CURRENT_USER, L"", KEY_QUERY_VALUE);
    ASSERT_EQ(ERROR_SUCCESS, temp_key_.DeleteKey(kTestRoot));
    temp_key_.Close();

    reg_change_count_ = 0;
  }

  static void WaitCallback() {
    reg_change_count_++;
    event_.Signal();
  }

 protected:
  RegKey temp_key_;
  static unsigned int reg_change_count_;
  static base::WaitableEvent event_;
};

unsigned int RegistryWatcherUnittest::reg_change_count_ = 0;
base::WaitableEvent RegistryWatcherUnittest::event_(
    false,   // auto reset
    false);  // initially unsignalled

TEST_F(RegistryWatcherUnittest, Basic) {
  RegistryWatcher watcher(HKEY_CURRENT_USER,
                          kTestRoot,
                          &RegistryWatcherUnittest::WaitCallback);
  ASSERT_TRUE(watcher.StartWatching());
  EXPECT_EQ(0, reg_change_count_);

  EXPECT_EQ(ERROR_SUCCESS, temp_key_.CreateKey(L"foo", KEY_ALL_ACCESS));
  EXPECT_TRUE(event_.TimedWait(base::TimeDelta::FromSeconds(kWaitSeconds)));
  EXPECT_EQ(1, reg_change_count_);
}
