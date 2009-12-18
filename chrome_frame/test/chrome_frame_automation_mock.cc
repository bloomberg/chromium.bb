// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/chrome_frame_automation_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

const int kLongWaitTimeout = 25 * 1000;
const int kShortWaitTimeout = 5 * 1000;

TEST(ChromeFrame, Launch) {
  MessageLoopForUI loop;
  AutomationMockLaunch mock_launch(&loop, kLongWaitTimeout);

  mock_launch.Navigate("about:blank");
  loop.Run(NULL);
  EXPECT_EQ(true, mock_launch.launch_result());
}

TEST(ChromeFrame, Navigate) {
  MessageLoopForUI loop;
  AutomationMockNavigate mock_navigate(&loop, kLongWaitTimeout);

  mock_navigate.NavigateRelativeFile(L"postmessage_basic_frame.html");
  loop.Run(NULL);
  EXPECT_EQ(false, mock_navigate.navigation_result());
}

TEST(ChromeFrame, PostMessage) {
  MessageLoopForUI loop;
  AutomationMockPostMessage mock_postmessage(&loop, kLongWaitTimeout);

  mock_postmessage.NavigateRelativeFile(L"postmessage_basic_frame.html");
  loop.Run(NULL);
  EXPECT_EQ(false, mock_postmessage.postmessage_result());
}

TEST(ChromeFrame, RequestStart) {
  MessageLoopForUI loop;
  AutomationMockHostNetworkRequestStart mock_request_start(&loop,
                                                           kLongWaitTimeout);

  mock_request_start.NavigateRelative(L"postmessage_basic_frame.html");
  loop.Run(NULL);
  EXPECT_EQ(true, mock_request_start.request_start_result());
}

