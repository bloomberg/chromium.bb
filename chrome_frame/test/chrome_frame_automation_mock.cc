// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/chrome_frame_automation_mock.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

const base::TimeDelta kLongWaitTimeout = base::TimeDelta::FromSeconds(25);

TEST(ChromeFrame, Launch) {
  base::MessageLoopForUI loop;
  AutomationMockLaunch mock_launch(&loop,
                                   kLongWaitTimeout.InMilliseconds());

  loop.PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitClosure(), kLongWaitTimeout);

  mock_launch.Navigate("about:blank");
  base::RunLoop run_loop(NULL);
  run_loop.Run();
  EXPECT_TRUE(mock_launch.launch_result());
}
