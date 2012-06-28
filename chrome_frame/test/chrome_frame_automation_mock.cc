// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/chrome_frame_automation_mock.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

const base::TimeDelta kLongWaitTimeout = base::TimeDelta::FromSeconds(25);

TEST(ChromeFrame, Launch) {
  MessageLoopForUI loop;
  AutomationMockLaunch mock_launch(&loop,
                                   kLongWaitTimeout.InMilliseconds());

  loop.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(), kLongWaitTimeout);

  mock_launch.Navigate("about:blank");
  base::RunLoop run_loop(NULL);
  run_loop.Run();
  EXPECT_TRUE(mock_launch.launch_result());
}

TEST(ChromeFrame, Navigate) {
  MessageLoopForUI loop;
  AutomationMockNavigate mock_navigate(&loop,
                                       kLongWaitTimeout.InMilliseconds());

  loop.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(), kLongWaitTimeout);

  mock_navigate.NavigateRelativeFile(L"postmessage_basic_frame.html");
  base::RunLoop run_loop(NULL);
  run_loop.Run();
  EXPECT_FALSE(mock_navigate.navigation_result());
}

TEST(ChromeFrame, PostMessage) {
  MessageLoopForUI loop;
  AutomationMockPostMessage mock_postmessage(&loop,
                                             kLongWaitTimeout.InMilliseconds());

  loop.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(), kLongWaitTimeout);

  mock_postmessage.NavigateRelativeFile(L"postmessage_basic_frame.html");
  base::RunLoop run_loop(NULL);
  run_loop.Run();
  EXPECT_FALSE(mock_postmessage.postmessage_result());
}

TEST(ChromeFrame, RequestStart) {
  MessageLoopForUI loop;
  AutomationMockHostNetworkRequestStart mock_request_start(
      &loop, kLongWaitTimeout.InMilliseconds());

  loop.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(), kLongWaitTimeout);

  mock_request_start.NavigateRelative(L"postmessage_basic_frame.html");
  base::RunLoop run_loop(NULL);
  run_loop.Run();
  EXPECT_TRUE(mock_request_start.request_start_result());
}

