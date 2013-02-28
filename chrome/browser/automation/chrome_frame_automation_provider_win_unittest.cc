// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/chrome_frame_automation_provider_win.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class MockChromeFrameAutomationProvider
    : public ChromeFrameAutomationProvider {
 public:
  explicit MockChromeFrameAutomationProvider(Profile* profile)
      : ChromeFrameAutomationProvider(profile) {}

  virtual ~MockChromeFrameAutomationProvider() {}

  MOCK_METHOD1(OnUnhandledMessage,
               void (const IPC::Message& message));  // NOLINT
};

typedef testing::Test AutomationProviderTest;

TEST_F(AutomationProviderTest, TestInvalidChromeFrameMessage) {
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);

  IPC::Message bad_msg(1, -1, IPC::Message::PRIORITY_NORMAL);

  scoped_refptr<MockChromeFrameAutomationProvider>
      mock(new MockChromeFrameAutomationProvider(NULL));

  EXPECT_CALL(*mock, OnUnhandledMessage(testing::Property(&IPC::Message::type,
                                        -1))).Times(1);
  mock->OnMessageReceived(bad_msg);
}
