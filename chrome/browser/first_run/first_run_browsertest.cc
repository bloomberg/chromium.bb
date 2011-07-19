// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef InProcessBrowserTest FirstRunBrowserTest;

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShowFirstRunBubblePref) {
  EXPECT_FALSE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowFirstRunBubble));
  EXPECT_TRUE(FirstRun::SetShowFirstRunBubblePref(true));
  ASSERT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowFirstRunBubble));
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kShouldShowFirstRunBubble));
}

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShowWelcomePagePref) {
  EXPECT_FALSE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowWelcomePage));
  EXPECT_TRUE(FirstRun::SetShowWelcomePagePref());
  ASSERT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldShowWelcomePage));
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kShouldShowWelcomePage));
}

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetOEMFirstRunBubblePref) {
  EXPECT_FALSE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldUseOEMFirstRunBubble));
  EXPECT_TRUE(FirstRun::SetOEMFirstRunBubblePref());
  ASSERT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldUseOEMFirstRunBubble));
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kShouldUseOEMFirstRunBubble));
}

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetMinimalFirstRunBubblePref) {
  EXPECT_FALSE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldUseMinimalFirstRunBubble));
  EXPECT_TRUE(FirstRun::SetMinimalFirstRunBubblePref());
  ASSERT_TRUE(g_browser_process->local_state()->FindPreference(
      prefs::kShouldUseMinimalFirstRunBubble));
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kShouldUseMinimalFirstRunBubble));
}
