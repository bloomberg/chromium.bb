// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/log_source_access_manager.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api_unittest_base_chromeos.h"

namespace extensions {

namespace {

using api::feedback_private::LOG_SOURCE_MESSAGES;
using api::feedback_private::LOG_SOURCE_UILATEST;
using api::feedback_private::ReadLogSourceResult;
using api::feedback_private::ReadLogSourceParams;

}  // namespace

using LogSourceAccessManagerTest = FeedbackPrivateApiUnittestBase;

TEST_F(LogSourceAccessManagerTest, MaxNumberOfOpenLogSources) {
  const base::TimeDelta timeout(base::TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  LogSourceAccessManager manager(profile());

  // Create a dummy callback to pass to FetchFromSource().
  LogSourceAccessManager::ReadLogSourceCallback callback =
      base::Bind([](const ReadLogSourceResult&) {});

  int count = 0;

  // Open 10 readers for LOG_SOURCE_MESSAGES.
  ReadLogSourceParams messages_params;
  messages_params.incremental = false;
  messages_params.source = LOG_SOURCE_MESSAGES;
  for (int i = 0; i < 10; ++i, ++count) {
    EXPECT_TRUE(manager.FetchFromSource(
        messages_params, base::StringPrintf("extension %d", count), callback))
        << count;
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(0U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Open 10 readers for LOG_SOURCE_UILATEST.
  ReadLogSourceParams ui_latest_params;
  ui_latest_params.incremental = false;
  ui_latest_params.source = LOG_SOURCE_UILATEST;
  for (int i = 0; i < 10; ++i, ++count) {
    EXPECT_TRUE(manager.FetchFromSource(
        ui_latest_params, base::StringPrintf("extension %d", count), callback))
        << count;
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Can't open more readers for LOG_SOURCE_MESSAGES.
  for (int i = 0; i < 10; ++i, ++count) {
    EXPECT_FALSE(manager.FetchFromSource(
        messages_params, base::StringPrintf("extension %d", count), callback))
        << count;
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Can't open more readers for LOG_SOURCE_UILATEST.
  for (int i = 0; i < 10; ++i, ++count) {
    EXPECT_FALSE(manager.FetchFromSource(
        ui_latest_params, base::StringPrintf("extension %d", count), callback))
        << count;
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Wait for all asynchronous operations to complete.
  base::RunLoop().RunUntilIdle();
}

}  // namespace extensions
