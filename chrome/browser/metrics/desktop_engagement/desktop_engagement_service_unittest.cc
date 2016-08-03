// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_engagement/desktop_engagement_service.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

// Mock class for |DesktopEngagementService| for testing.
class MockDesktopEngagementService : public metrics::DesktopEngagementService {
 public:
  MockDesktopEngagementService() {}

  bool is_timeout() const { return time_out_; }

  using metrics::DesktopEngagementService::OnAudioStart;
  using metrics::DesktopEngagementService::OnAudioEnd;

 protected:
  void OnTimerFired() override {
    DesktopEngagementService::OnTimerFired();
    time_out_ = true;
  }

 private:
  bool time_out_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockDesktopEngagementService);
};

TEST(DesktopEngagementServiceTest, TestVisibility) {
  base::MessageLoop loop(base::MessageLoop::TYPE_DEFAULT);
  base::HistogramTester histogram_tester;

  MockDesktopEngagementService instance;

  // The browser becomes visible but it shouldn't start the session.
  instance.OnVisibilityChanged(true);
  EXPECT_FALSE(instance.in_session());
  EXPECT_TRUE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 0);

  instance.OnUserEvent();
  EXPECT_TRUE(instance.in_session());
  EXPECT_TRUE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 0);

  // Even if there is a recent user event visibility change should end the
  // session.
  instance.OnUserEvent();
  instance.OnUserEvent();
  instance.OnVisibilityChanged(false);
  EXPECT_FALSE(instance.in_session());
  EXPECT_FALSE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 1);

  // For the second time only visibility change should start the session.
  instance.OnVisibilityChanged(true);
  EXPECT_TRUE(instance.in_session());
  EXPECT_TRUE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 1);
  instance.OnVisibilityChanged(false);
  EXPECT_FALSE(instance.in_session());
  EXPECT_FALSE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 2);
}

TEST(DesktopEngagementServiceTest, TestUserEvent) {
  base::MessageLoop loop(base::MessageLoop::TYPE_DEFAULT);
  base::HistogramTester histogram_tester;

  MockDesktopEngagementService instance;
  instance.SetInactivityTimeoutForTesting(1);

  EXPECT_FALSE(instance.in_session());
  EXPECT_FALSE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 0);

  // User event doesn't go through if nothing is visible.
  instance.OnUserEvent();
  EXPECT_FALSE(instance.in_session());
  EXPECT_FALSE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 0);

  instance.OnVisibilityChanged(true);
  instance.OnUserEvent();
  EXPECT_TRUE(instance.in_session());
  EXPECT_TRUE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 0);

  // Wait until the session expires.
  while (!instance.is_timeout()) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(instance.in_session());
  EXPECT_TRUE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 1);
}

TEST(DesktopEngagementServiceTest, TestAudioEvent) {
  base::MessageLoop loop(base::MessageLoop::TYPE_DEFAULT);
  base::HistogramTester histogram_tester;

  MockDesktopEngagementService instance;
  instance.SetInactivityTimeoutForTesting(1);

  instance.OnVisibilityChanged(true);
  instance.OnAudioStart();
  EXPECT_TRUE(instance.in_session());
  EXPECT_TRUE(instance.is_visible());
  EXPECT_TRUE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 0);

  instance.OnVisibilityChanged(false);
  EXPECT_TRUE(instance.in_session());
  EXPECT_FALSE(instance.is_visible());
  EXPECT_TRUE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 0);

  instance.OnAudioEnd();
  EXPECT_TRUE(instance.in_session());
  EXPECT_FALSE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 0);

  // Wait until the session expires.
  while (!instance.is_timeout()) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(instance.in_session());
  EXPECT_FALSE(instance.is_visible());
  EXPECT_FALSE(instance.is_audio_playing());
  histogram_tester.ExpectTotalCount("Session.TotalDuration", 1);
}
