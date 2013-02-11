// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/test/chromedriver/navigation_tracker.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/stub_devtools_client.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NavigationTracker, IsPendingNavigation) {
  StubDevToolsClient client;
  NavigationTracker tracker(&client);
  ASSERT_FALSE(tracker.IsPendingNavigation("f"));

  base::DictionaryValue params;
  params.SetString("frameId", "f");
  tracker.OnEvent("Page.frameStartedLoading", params);
  ASSERT_TRUE(tracker.IsPendingNavigation("f"));
  tracker.OnEvent("Page.frameStoppedLoading", params);
  ASSERT_FALSE(tracker.IsPendingNavigation("f"));
}

TEST(NavigationTracker, NavigationScheduledThenLoaded) {
  StubDevToolsClient client;
  NavigationTracker tracker(&client);
  base::DictionaryValue params;
  params.SetString("frameId", "f");
  base::DictionaryValue params_scheduled;
  params_scheduled.SetString("frameId", "f");
  params_scheduled.SetInteger("delay", 10);

  tracker.OnEvent("Page.frameScheduledNavigation", params_scheduled);
  ASSERT_TRUE(tracker.IsPendingNavigation("f"));
  tracker.OnEvent("Page.frameStartedLoading", params);
  ASSERT_TRUE(tracker.IsPendingNavigation("f"));
  tracker.OnEvent("Page.frameClearedScheduledNavigation", params);
  ASSERT_TRUE(tracker.IsPendingNavigation("f"));
  tracker.OnEvent("Page.frameStoppedLoading", params);
  ASSERT_FALSE(tracker.IsPendingNavigation("f"));
}

TEST(NavigationTracker, NavigationScheduledThenCancelled) {
  StubDevToolsClient client;
  NavigationTracker tracker(&client);
  base::DictionaryValue params;
  params.SetString("frameId", "f");
  base::DictionaryValue params_scheduled;
  params_scheduled.SetString("frameId", "f");
  params_scheduled.SetInteger("delay", 10);

  tracker.OnEvent("Page.frameScheduledNavigation", params_scheduled);
  ASSERT_TRUE(tracker.IsPendingNavigation("f"));
  tracker.OnEvent("Page.frameClearedScheduledNavigation", params);
  ASSERT_FALSE(tracker.IsPendingNavigation("f"));
}

TEST(NavigationTracker, InterleavedMultipleFrameNavigations) {
  StubDevToolsClient client;
  NavigationTracker tracker(&client);
  base::DictionaryValue params_f1;
  params_f1.SetString("frameId", "f1");
  base::DictionaryValue params_f1_scheduled;
  params_f1_scheduled.SetString("frameId", "f1");
  params_f1_scheduled.SetInteger("delay", 10);
  base::DictionaryValue params_f2;
  params_f2.SetString("frameId", "f2");
  base::DictionaryValue params_f2_scheduled;
  params_f2_scheduled.SetString("frameId", "f2");
  params_f2_scheduled.SetInteger("delay", 10);

  tracker.OnEvent("Page.frameScheduledNavigation", params_f1_scheduled);
  tracker.OnEvent("Page.frameScheduledNavigation", params_f2_scheduled);
  ASSERT_TRUE(tracker.IsPendingNavigation("f1"));
  ASSERT_TRUE(tracker.IsPendingNavigation("f2"));
  tracker.OnEvent("Page.frameClearedScheduledNavigation", params_f1);
  ASSERT_FALSE(tracker.IsPendingNavigation("f1"));
  ASSERT_TRUE(tracker.IsPendingNavigation("f2"));
  tracker.OnEvent("Page.frameClearedScheduledNavigation", params_f2);
  ASSERT_FALSE(tracker.IsPendingNavigation("f2"));
}
