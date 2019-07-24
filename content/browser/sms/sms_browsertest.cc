// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_provider.h"
#include "content/browser/sms/sms_service.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/sms_dialog.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

class MockSmsDialog : public SmsDialog {
 public:
  MockSmsDialog() : SmsDialog() {}
  ~MockSmsDialog() override = default;

  MOCK_METHOD3(Open,
               void(RenderFrameHost*, base::OnceClosure, base::OnceClosure));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(EnableContinueButton, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsDialog);
};

class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD0(CreateSmsDialog, std::unique_ptr<SmsDialog>());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebContentsDelegate);
};

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

  NiceMock<MockWebContentsDelegate> delegate_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Receive) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  shell()->web_contents()->SetDelegate(&delegate_);

  auto* dialog = new NiceMock<MockSmsDialog>();

  base::OnceClosure on_continue_callback;

  EXPECT_CALL(delegate_, CreateSmsDialog())
      .WillOnce(Return(ByMove(base::WrapUnique(dialog))));

  EXPECT_CALL(*dialog, Open(_, _, _))
      .WillOnce(Invoke([&on_continue_callback](content::RenderFrameHost*,
                                               base::OnceClosure on_continue,
                                               base::OnceClosure on_cancel) {
        on_continue_callback = std::move(on_continue);
      }));

  EXPECT_CALL(*dialog, EnableContinueButton())
      .WillOnce(Invoke([&on_continue_callback]() {
        std::move(on_continue_callback).Run();
      }));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  // Test that SMS content can be retrieved after navigator.sms.receive().
  std::string script = R"(
    (async () => {
      let sms = await navigator.sms.receive({timeout: 60});
      return sms.content;
    }) ();
  )";

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&provider, &url]() {
    provider->NotifyReceive(url::Origin::Create(url), "hello");
  }));

  EXPECT_EQ("hello", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, ReceiveMultiple) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  shell()->web_contents()->SetDelegate(&delegate_);

  auto* dialog1 = new NiceMock<MockSmsDialog>();
  auto* dialog2 = new NiceMock<MockSmsDialog>();

  base::OnceClosure on_continue_callback1;
  base::OnceClosure on_continue_callback2;

  EXPECT_CALL(delegate_, CreateSmsDialog())
      .WillOnce(Return(ByMove(base::WrapUnique(dialog1))))
      .WillOnce(Return(ByMove(base::WrapUnique(dialog2))));

  EXPECT_CALL(*dialog1, Open(_, _, _))
      .WillOnce(Invoke([&on_continue_callback1](content::RenderFrameHost*,
                                                base::OnceClosure on_continue,
                                                base::OnceClosure on_cancel) {
        on_continue_callback1 = std::move(on_continue);
      }));

  EXPECT_CALL(*dialog2, Open(_, _, _))
      .WillOnce(Invoke([&on_continue_callback2](content::RenderFrameHost*,
                                                base::OnceClosure on_continue,
                                                base::OnceClosure on_cancel) {
        on_continue_callback2 = std::move(on_continue);
      }));

  EXPECT_CALL(*dialog1, EnableContinueButton())
      .WillOnce(Invoke([&on_continue_callback1]() {
        std::move(on_continue_callback1).Run();
      }));
  EXPECT_CALL(*dialog2, EnableContinueButton())
      .WillOnce(Invoke([&on_continue_callback2]() {
        std::move(on_continue_callback2).Run();
      }));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  // Test that SMS content can retrieve multiple messages.
  std::string script = R"(
    (async () => {
      let sms1 = navigator.sms.receive();
      let sms2 = navigator.sms.receive();

      let msg1 = await sms1;
      let msg2 = await sms2;

      return [msg1.content, msg2.content];
    }) ();
  )";

  EXPECT_CALL(*provider, Retrieve())
      .WillOnce(Invoke([&provider, &url]() {
        provider->NotifyReceive(url::Origin::Create(url), "hello1");
      }))
      .WillOnce(Invoke([&provider, &url]() {
        provider->NotifyReceive(url::Origin::Create(url), "hello2");
      }));

  base::ListValue result = EvalJs(shell(), script).ExtractList();
  ASSERT_EQ(2u, result.GetList().size());
  EXPECT_EQ("hello1", result.GetList()[0].GetString());
  EXPECT_EQ("hello2", result.GetList()[1].GetString());
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, SmsReceivedAfterTabIsClosed) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  NavigateToURL(shell(), url);

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  std::string script = R"(
    // kicks off an sms receiver call, but deliberately leaves it hanging.
    navigator.sms.receive({timeout: 60});
    true
  )";

  base::RunLoop loop;

  EXPECT_CALL(*provider, Retrieve()).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_EQ(true, EvalJs(shell(), script));

  loop.Run();

  shell()->Close();

  provider->NotifyReceive(url::Origin::Create(url), "hello");
}

}  // namespace content
