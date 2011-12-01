// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class FakeExternalProtocolHandlerWorker
    : public ShellIntegration::DefaultProtocolClientWorker {
 public:
  FakeExternalProtocolHandlerWorker(
      ShellIntegration::DefaultWebClientObserver* observer,
      const std::string& protocol,
      ShellIntegration::DefaultWebClientState os_state)
      : ShellIntegration::DefaultProtocolClientWorker(observer, protocol),
        os_state_(os_state) {}

 private:
  virtual ShellIntegration::DefaultWebClientState CheckIsDefault() {
    return os_state_;
  }

  virtual void SetAsDefault() {}

  ShellIntegration::DefaultWebClientState os_state_;
};

class FakeExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  FakeExternalProtocolHandlerDelegate()
      : block_state_(ExternalProtocolHandler::BLOCK),
        os_state_(ShellIntegration::UNKNOWN_DEFAULT_WEB_CLIENT),
        has_launched_(false),
        has_prompted_(false),
        has_blocked_ (false) {}

  virtual ShellIntegration::DefaultProtocolClientWorker* CreateShellWorker(
      ShellIntegration::DefaultWebClientObserver* observer,
      const std::string& protocol) {
    return new FakeExternalProtocolHandlerWorker(observer, protocol, os_state_);
  }

  virtual ExternalProtocolHandler::BlockState GetBlockState(
      const std::string& scheme) { return block_state_; }

  virtual void BlockRequest() {
    ASSERT_TRUE(block_state_ == ExternalProtocolHandler::BLOCK ||
                os_state_ == ShellIntegration::IS_DEFAULT_WEB_CLIENT);
    has_blocked_ = true;
  }

  virtual void RunExternalProtocolDialog(const GURL& url,
                                         int render_process_host_id,
                                         int routing_id) {
    ASSERT_EQ(block_state_, ExternalProtocolHandler::UNKNOWN);
    ASSERT_NE(os_state_, ShellIntegration::IS_DEFAULT_WEB_CLIENT);
    has_prompted_ = true;
  }

  virtual void LaunchUrlWithoutSecurityCheck(const GURL& url) {
    ASSERT_EQ(block_state_, ExternalProtocolHandler::DONT_BLOCK);
    ASSERT_NE(os_state_, ShellIntegration::IS_DEFAULT_WEB_CLIENT);
    has_launched_ = true;
  }

  virtual void FinishedProcessingCheck() {
    MessageLoop::current()->Quit();
  }

  void set_os_state(ShellIntegration::DefaultWebClientState value) {
    os_state_ = value;
  }

  void set_block_state(ExternalProtocolHandler::BlockState value) {
    block_state_ = value;
  }

  bool has_launched() { return has_launched_; }
  bool has_prompted() { return has_prompted_; }
  bool has_blocked() { return has_blocked_; }

 private:
  ExternalProtocolHandler::BlockState block_state_;
  ShellIntegration::DefaultWebClientState os_state_;
  bool has_launched_;
  bool has_prompted_;
  bool has_blocked_;
};

class ExternalProtocolHandlerTest : public testing::Test {
 protected:
  ExternalProtocolHandlerTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE) {}

  virtual void SetUp() {
    file_thread_.Start();
  }

  virtual void TearDown() {
    // Ensure that g_accept_requests gets set back to true after test execution.
    ExternalProtocolHandler::PermitLaunchUrl();
  }

  void DoTest(ExternalProtocolHandler::BlockState block_state,
              ShellIntegration::DefaultWebClientState os_state,
              bool should_prompt, bool should_launch, bool should_block) {
    GURL url("mailto:test@test.com");
    ASSERT_FALSE(delegate_.has_prompted());
    ASSERT_FALSE(delegate_.has_launched());
    ASSERT_FALSE(delegate_.has_blocked());

    delegate_.set_block_state(block_state);
    delegate_.set_os_state(os_state);
    ExternalProtocolHandler::LaunchUrlWithDelegate(url, 0, 0, &delegate_);
    if (block_state != ExternalProtocolHandler::BLOCK)
      MessageLoop::current()->Run();

    ASSERT_EQ(should_prompt, delegate_.has_prompted());
    ASSERT_EQ(should_launch, delegate_.has_launched());
    ASSERT_EQ(should_block, delegate_.has_blocked());
  }

  MessageLoopForUI ui_message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  FakeExternalProtocolHandlerDelegate delegate_;
};

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeDefault) {
  DoTest(ExternalProtocolHandler::BLOCK,
         ShellIntegration::IS_DEFAULT_WEB_CLIENT,
         false, false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeNotDefault) {
  DoTest(ExternalProtocolHandler::BLOCK,
         ShellIntegration::NOT_DEFAULT_WEB_CLIENT,
         false, false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeUnknown) {
  DoTest(ExternalProtocolHandler::BLOCK,
         ShellIntegration::UNKNOWN_DEFAULT_WEB_CLIENT,
         false, false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK,
         ShellIntegration::IS_DEFAULT_WEB_CLIENT,
         false, false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeNotDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK,
         ShellIntegration::NOT_DEFAULT_WEB_CLIENT,
         false, true, false);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeUnknown) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK,
         ShellIntegration::UNKNOWN_DEFAULT_WEB_CLIENT,
         false, true, false);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnknownChromeDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN,
         ShellIntegration::IS_DEFAULT_WEB_CLIENT,
         false, false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnknownChromeNotDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN,
         ShellIntegration::NOT_DEFAULT_WEB_CLIENT,
         true, false, false);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnknownChromeUnknown) {
  DoTest(ExternalProtocolHandler::UNKNOWN,
         ShellIntegration::UNKNOWN_DEFAULT_WEB_CLIENT,
         true, false, false);
}
