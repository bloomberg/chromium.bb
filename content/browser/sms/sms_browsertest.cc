// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_provider.h"
#include "content/browser/sms/sms_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace content {

namespace {

class MockSmsProvider : public SmsProvider {
 public:
  MockSmsProvider() = default;
  ~MockSmsProvider() override = default;

  MOCK_METHOD0(Retrieve, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsProvider);
};

class SmsBrowserTest : public ContentBrowserTest {
 public:
  SmsBrowserTest() = default;
  ~SmsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "SmsReceiver");
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Start) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  auto* mock = new NiceMock<MockSmsProvider>();

  auto* sms_service = static_cast<SmsServiceImpl*>(
      shell()->web_contents()->GetBrowserContext()->GetSmsService());
  sms_service->SetSmsProviderForTest(base::WrapUnique(mock));

  // Test that SMS content can be retrieved after SmsManager.start().
  std::string script = R"(
    (async () => {
      let receiver = new SMSReceiver({timeout: 60});
      await receiver.start();
      return new Promise(function(resolve) {
        receiver.addEventListener('change', e => {
          resolve(receiver.sms.content);
        })
      });
    }) ();
  )";

  EXPECT_CALL(*mock, Retrieve()).WillOnce(Invoke([&mock, &url]() {
    mock->NotifyReceive(url::Origin::Create(url), "hello");
  }));

  EXPECT_EQ("hello", EvalJs(shell(), script));
}

}  // namespace content
