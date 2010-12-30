// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

// The test is somewhat silly, because the Vista bots some have UAC enabled
// and some have it disabled. At least we check that it does not crash.
TEST(BaseWinUtilTest, TestIsUACEnabled) {
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    win_util::UserAccountControlIsEnabled();
  } else {
    EXPECT_TRUE(win_util::UserAccountControlIsEnabled());
  }
}

TEST(BaseWinUtilTest, TestGetUserSidString) {
  std::wstring user_sid;
  EXPECT_TRUE(win_util::GetUserSidString(&user_sid));
  EXPECT_TRUE(!user_sid.empty());
}

TEST(BaseWinUtilTest, TestGetNonClientMetrics) {
  NONCLIENTMETRICS metrics = {0};
  win_util::GetNonClientMetrics(&metrics);
  EXPECT_TRUE(metrics.cbSize > 0);
  EXPECT_TRUE(metrics.iScrollWidth > 0);
  EXPECT_TRUE(metrics.iScrollHeight > 0);
}

namespace {

// Saves the current thread's locale ID when initialized, and restores it when
// the instance is going out of scope.
class ThreadLocaleSaver {
 public:
  ThreadLocaleSaver() : original_locale_id_(GetThreadLocale()) {}
  ~ThreadLocaleSaver() { SetThreadLocale(original_locale_id_); }

 private:
  LCID original_locale_id_;

  DISALLOW_COPY_AND_ASSIGN(ThreadLocaleSaver);
};

}  // namespace
