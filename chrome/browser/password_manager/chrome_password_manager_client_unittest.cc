// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/password_manager_logger.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
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

  testing::StrictMock<MockPasswordManagerLogger> logger;
};

void ChromePasswordManagerClientTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ChromePasswordManagerClient::CreateForWebContents(web_contents());
}

ChromePasswordManagerClient* ChromePasswordManagerClientTest::GetClient() {
  return ChromePasswordManagerClient::FromWebContents(web_contents());
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressNoLogger) {
  ChromePasswordManagerClient* client = GetClient();

  EXPECT_CALL(logger, LogSavePasswordProgress(kTestText)).Times(0);
  // Before attaching the logger, no text should be passed.
  client->LogSavePasswordProgress(kTestText);
  EXPECT_FALSE(client->IsLoggingActive());
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressAttachLogger) {
  ChromePasswordManagerClient* client = GetClient();

  // After attaching the logger, text should be passed.
  client->SetLogger(&logger);
  EXPECT_CALL(logger, LogSavePasswordProgress(kTestText)).Times(1);
  client->LogSavePasswordProgress(kTestText);
  EXPECT_TRUE(client->IsLoggingActive());
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressDetachLogger) {
  ChromePasswordManagerClient* client = GetClient();

  client->SetLogger(&logger);
  // After detaching the logger, no text should be passed.
  client->SetLogger(NULL);
  EXPECT_CALL(logger, LogSavePasswordProgress(kTestText)).Times(0);
  client->LogSavePasswordProgress(kTestText);
  EXPECT_FALSE(client->IsLoggingActive());
}
