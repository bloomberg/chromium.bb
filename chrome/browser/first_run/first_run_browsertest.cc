// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef InProcessBrowserTest FirstRunBrowserTest;

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShowFirstRunBubblePref) {
  EXPECT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowFirstRunBubble));
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(true));
  ASSERT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowFirstRunBubble));
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kShouldShowFirstRunBubble));
  // Test that toggling the value works in either direction after it's been set.
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(false));
  EXPECT_FALSE(g_browser_process->local_state()->GetBoolean(
      prefs::kShouldShowFirstRunBubble));
  EXPECT_TRUE(first_run::SetShowFirstRunBubblePref(true));
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kShouldShowFirstRunBubble));
}

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShowWelcomePagePref) {
  EXPECT_FALSE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowWelcomePage));
  EXPECT_TRUE(first_run::SetShowWelcomePagePref());
  ASSERT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowWelcomePage));
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kShouldShowWelcomePage));
}
