// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/automation/chrome_frame_automation_provider.h"
#include "ipc/ipc_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockChromeFrameAutomationProvider
    : public ChromeFrameAutomationProvider {
 public:
  explicit MockChromeFrameAutomationProvider(Profile* profile)
      : ChromeFrameAutomationProvider(profile) {}

  virtual ~MockChromeFrameAutomationProvider() {}

  MOCK_METHOD1(OnUnhandledMessage,
               void (const IPC::Message& message));  // NOLINT
};

TEST(AutomationProviderTest, TestInvalidChromeFrameMessage) {
  MessageLoop message_loop;
  BrowserThread ui_thread(BrowserThread::UI, &message_loop);

  IPC::Message bad_msg(1, -1, IPC::Message::PRIORITY_NORMAL);

  scoped_refptr<MockChromeFrameAutomationProvider>
      mock(new MockChromeFrameAutomationProvider(NULL));

  EXPECT_CALL(*mock, OnUnhandledMessage(testing::Property(&IPC::Message::type,
                                        -1))).Times(1);
  mock->OnMessageReceived(bad_msg);
}

