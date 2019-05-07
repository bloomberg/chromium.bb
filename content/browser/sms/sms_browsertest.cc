// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
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

  void Retrieve(base::TimeDelta timeout, SmsCallback callback) {
    DoRetrieve(timeout, &callback);
  }

  MOCK_METHOD2(DoRetrieve, void(base::TimeDelta, SmsCallback*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsProvider);
};

class SmsTest : public ContentBrowserTest {
 public:
  SmsTest() = default;
  ~SmsTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "SmsReceiver");
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SmsTest, Start) {
  NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html"));

  auto mock_sms_provider = std::make_unique<NiceMock<MockSmsProvider>>();
  auto* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  SmsManager* sms_mgr =
      static_cast<StoragePartitionImpl*>(rph->GetStoragePartition())
          ->GetSmsManager();

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

  EXPECT_CALL(*mock_sms_provider,
              DoRetrieve(base::TimeDelta::FromSeconds(60), _))
      .WillOnce(Invoke(
          [](base::TimeDelta timeout,
             base::OnceCallback<void(bool, base::Optional<std::string>)>*
                 callback) { std::move(*callback).Run(true, "hello"); }));

  sms_mgr->SetSmsProviderForTest(std::move(mock_sms_provider));

  EXPECT_EQ("hello", EvalJs(shell(), script));
}

}  // namespace content
