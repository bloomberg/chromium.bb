// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/password_manager/core/browser/password_manager_logger.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::WebContents;

namespace {

const char kTestText[] = "abcd1234";

class MockPasswordManagerLogger
    : public password_manager::PasswordManagerLogger {
 public:
  MockPasswordManagerLogger() {}

  MOCK_METHOD1(LogSavePasswordProgress, void(const std::string&));
};

}  // namespace

class ChromePasswordManagerClientTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() OVERRIDE;

 protected:
  ChromePasswordManagerClient* GetClient();

  // If the test IPC sink contains an AutofillMsg_ChangeLoggingState message,
  // then copies its argument into |activation_flag| and returns true. Otherwise
  // returns false.
  bool WasLoggingActivationMessageSent(bool* activation_flag);

  testing::StrictMock<MockPasswordManagerLogger> logger_;
};

void ChromePasswordManagerClientTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillManagerDelegate(
      web_contents(), NULL);
}

ChromePasswordManagerClient* ChromePasswordManagerClientTest::GetClient() {
  return ChromePasswordManagerClient::FromWebContents(web_contents());
}

bool ChromePasswordManagerClientTest::WasLoggingActivationMessageSent(
    bool* activation_flag) {
  const uint32 kMsgID = AutofillMsg_ChangeLoggingState::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  if (!message)
    return false;
  Tuple1<bool> param;
  AutofillMsg_ChangeLoggingState::Read(message, &param);
  *activation_flag = param.a;
  process()->sink().ClearMessages();
  return true;
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressNoLogger) {
  ChromePasswordManagerClient* client = GetClient();

  EXPECT_CALL(logger_, LogSavePasswordProgress(kTestText)).Times(0);
  // Before attaching the logger, no text should be passed.
  client->LogSavePasswordProgress(kTestText);
  EXPECT_FALSE(client->IsLoggingActive());
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressAttachLogger) {
  ChromePasswordManagerClient* client = GetClient();

  // After attaching the logger, text should be passed.
  client->SetLogger(&logger_);
  EXPECT_CALL(logger_, LogSavePasswordProgress(kTestText)).Times(1);
  client->LogSavePasswordProgress(kTestText);
  EXPECT_TRUE(client->IsLoggingActive());
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressDetachLogger) {
  ChromePasswordManagerClient* client = GetClient();

  client->SetLogger(&logger_);
  // After detaching the logger, no text should be passed.
  client->SetLogger(NULL);
  EXPECT_CALL(logger_, LogSavePasswordProgress(kTestText)).Times(0);
  client->LogSavePasswordProgress(kTestText);
  EXPECT_FALSE(client->IsLoggingActive());
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressNotifyRenderer) {
  ChromePasswordManagerClient* client = GetClient();
  bool logging_active = false;

  // Initially, the logging should be off, so no IPC messages.
  EXPECT_FALSE(WasLoggingActivationMessageSent(&logging_active));

  client->SetLogger(&logger_);
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_TRUE(logging_active);

  client->SetLogger(NULL);
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_FALSE(logging_active);
}
