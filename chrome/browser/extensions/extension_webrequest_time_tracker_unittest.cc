// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_time_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
const base::TimeDelta kRequestDelta = base::TimeDelta::FromMilliseconds(100);
const base::TimeDelta kTinyDelay = base::TimeDelta::FromMilliseconds(1);
const base::TimeDelta kModerateDelay = base::TimeDelta::FromMilliseconds(25);
const base::TimeDelta kExcessiveDelay = base::TimeDelta::FromMilliseconds(75);
}  // namespace

//class ExtensionWebRequestTimeTrackerTest : public testing::Test {};

TEST(ExtensionWebRequestTimeTrackerTest, Basic) {
  ExtensionWebRequestTimeTracker tracker;
  base::Time start;

  tracker.LogRequestStartTime(42, start, GURL());
  EXPECT_EQ(1u, tracker.request_time_logs_.size());
  ASSERT_EQ(1u, tracker.request_ids_.size());
  EXPECT_EQ(42, tracker.request_ids_.front());
  tracker.LogRequestEndTime(42, start + kRequestDelta);
  EXPECT_EQ(1u, tracker.request_time_logs_.size());
  EXPECT_EQ(0u, tracker.moderate_delays_.size());
  EXPECT_EQ(0u, tracker.excessive_delays_.size());
}

TEST(ExtensionWebRequestTimeTrackerTest, CancelOrRedirect) {
  ExtensionWebRequestTimeTracker tracker;
  base::Time start;

  tracker.LogRequestStartTime(1, start, GURL());
  EXPECT_EQ(1u, tracker.request_time_logs_.size());
  tracker.SetRequestCanceled(1);
  tracker.LogRequestEndTime(1, start + kRequestDelta);
  EXPECT_EQ(0u, tracker.request_time_logs_.size());

  tracker.LogRequestStartTime(2, start, GURL());
  EXPECT_EQ(1u, tracker.request_time_logs_.size());
  tracker.SetRequestRedirected(2);
  tracker.LogRequestEndTime(2, start + kRequestDelta);
  EXPECT_EQ(0u, tracker.request_time_logs_.size());
}

TEST(ExtensionWebRequestTimeTrackerTest, Delays) {
  ExtensionWebRequestTimeTracker tracker;
  base::Time start;
  std::string extension1_id("1");
  std::string extension2_id("2");

  // Start 3 requests with different amounts of delay from 2 extensions.
  tracker.LogRequestStartTime(1, start, GURL());
  tracker.LogRequestStartTime(2, start, GURL());
  tracker.LogRequestStartTime(3, start, GURL());
  tracker.IncrementExtensionBlockTime(extension1_id, 1, kTinyDelay);
  tracker.IncrementExtensionBlockTime(extension1_id, 2, kTinyDelay);
  tracker.IncrementExtensionBlockTime(extension1_id, 3, kTinyDelay);
  tracker.IncrementExtensionBlockTime(extension2_id, 2, kModerateDelay);
  tracker.IncrementExtensionBlockTime(extension2_id, 3, kExcessiveDelay);
  tracker.IncrementTotalBlockTime(1, kTinyDelay);
  tracker.IncrementTotalBlockTime(2, kModerateDelay);
  tracker.IncrementTotalBlockTime(3, kExcessiveDelay);
  tracker.LogRequestEndTime(1, start + kRequestDelta);
  tracker.LogRequestEndTime(2, start + kRequestDelta);
  tracker.LogRequestEndTime(3, start + kRequestDelta);
  EXPECT_EQ(3u, tracker.request_time_logs_.size());
  EXPECT_EQ(1u, tracker.moderate_delays_.size());
  EXPECT_EQ(1u, tracker.moderate_delays_.count(2));
  EXPECT_EQ(1u, tracker.excessive_delays_.size());
  EXPECT_EQ(1u, tracker.excessive_delays_.count(3));

  // Now issue a bunch more requests and ensure that the old delays are
  // forgotten.
  for (int64 i = 4; i < 500; ++i) {
    tracker.LogRequestStartTime(i, start, GURL());
    tracker.LogRequestEndTime(i, start + kRequestDelta);
  }
  EXPECT_EQ(0u, tracker.moderate_delays_.size());
  EXPECT_EQ(0u, tracker.excessive_delays_.size());
}
