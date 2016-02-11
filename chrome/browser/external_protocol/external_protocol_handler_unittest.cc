// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include "base/message_loop/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class FakeExternalProtocolHandlerWorker
    : public shell_integration::DefaultProtocolClientWorker {
 public:
  FakeExternalProtocolHandlerWorker(
      shell_integration::DefaultWebClientObserver* observer,
      const std::string& protocol,
      shell_integration::DefaultWebClientState os_state)
      : shell_integration::DefaultProtocolClientWorker(
            observer,
            protocol,
            /*delete_observer=*/true),
        os_state_(os_state) {}

 private:
  ~FakeExternalProtocolHandlerWorker() override {}

  void CheckIsDefault() override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FakeExternalProtocolHandlerWorker::OnCheckIsDefaultComplete,
                   this, os_state_));
  }

  void SetAsDefault() override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &FakeExternalProtocolHandlerWorker::OnSetAsDefaultAttemptComplete,
            this, AttemptResult::SUCCESS));
  }

  shell_integration::DefaultWebClientState os_state_;
};

class FakeExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  FakeExternalProtocolHandlerDelegate()
      : block_state_(ExternalProtocolHandler::BLOCK),
        os_state_(shell_integration::UNKNOWN_DEFAULT),
        has_launched_(false),
        has_prompted_(false),
        has_blocked_(false) {}

  shell_integration::DefaultProtocolClientWorker* CreateShellWorker(
      shell_integration::DefaultWebClientObserver* observer,
      const std::string& protocol) override {
    return new FakeExternalProtocolHandlerWorker(observer, protocol, os_state_);
  }

  ExternalProtocolHandler::BlockState GetBlockState(
      const std::string& scheme) override {
    return block_state_;
  }

  void BlockRequest() override {
    ASSERT_TRUE(block_state_ == ExternalProtocolHandler::BLOCK ||
                os_state_ == shell_integration::IS_DEFAULT);
    has_blocked_ = true;
  }

  void RunExternalProtocolDialog(const GURL& url,
                                 int render_process_host_id,
                                 int routing_id,
                                 ui::PageTransition page_transition,
                                 bool has_user_gesture) override {
    ASSERT_EQ(block_state_, ExternalProtocolHandler::UNKNOWN);
    ASSERT_NE(os_state_, shell_integration::IS_DEFAULT);
    has_prompted_ = true;
  }

  void LaunchUrlWithoutSecurityCheck(const GURL& url) override {
    ASSERT_EQ(block_state_, ExternalProtocolHandler::DONT_BLOCK);
    ASSERT_NE(os_state_, shell_integration::IS_DEFAULT);
    has_launched_ = true;
  }

  void FinishedProcessingCheck() override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void set_os_state(shell_integration::DefaultWebClientState value) {
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
  shell_integration::DefaultWebClientState os_state_;
  bool has_launched_;
  bool has_prompted_;
  bool has_blocked_;
};

class ExternalProtocolHandlerTest : public testing::Test {
 protected:
  ExternalProtocolHandlerTest()
      : ui_thread_(BrowserThread::UI, base::MessageLoop::current()),
        file_thread_(BrowserThread::FILE) {}

  void SetUp() override { file_thread_.Start(); }

  void TearDown() override {
    // Ensure that g_accept_requests gets set back to true after test execution.
    ExternalProtocolHandler::PermitLaunchUrl();
  }

  void DoTest(ExternalProtocolHandler::BlockState block_state,
              shell_integration::DefaultWebClientState os_state,
              bool should_prompt,
              bool should_launch,
              bool should_block) {
    GURL url("mailto:test@test.com");
    ASSERT_FALSE(delegate_.has_prompted());
    ASSERT_FALSE(delegate_.has_launched());
    ASSERT_FALSE(delegate_.has_blocked());

    delegate_.set_block_state(block_state);
    delegate_.set_os_state(os_state);
    ExternalProtocolHandler::LaunchUrlWithDelegate(
        url, 0, 0, ui::PAGE_TRANSITION_LINK, true, &delegate_);
    if (block_state != ExternalProtocolHandler::BLOCK)
      base::MessageLoop::current()->Run();

    ASSERT_EQ(should_prompt, delegate_.has_prompted());
    ASSERT_EQ(should_launch, delegate_.has_launched());
    ASSERT_EQ(should_block, delegate_.has_blocked());
  }

  base::MessageLoopForUI ui_message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  FakeExternalProtocolHandlerDelegate delegate_;
};

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeDefault) {
  DoTest(ExternalProtocolHandler::BLOCK, shell_integration::IS_DEFAULT, false,
         false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeNotDefault) {
  DoTest(ExternalProtocolHandler::BLOCK, shell_integration::NOT_DEFAULT, false,
         false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeUnknown) {
  DoTest(ExternalProtocolHandler::BLOCK, shell_integration::UNKNOWN_DEFAULT,
         false, false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK, shell_integration::IS_DEFAULT,
         false, false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeNotDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK, shell_integration::NOT_DEFAULT,
         false, true, false);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeUnknown) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK,
         shell_integration::UNKNOWN_DEFAULT, false, true, false);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnknownChromeDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::IS_DEFAULT, false,
         false, true);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnknownChromeNotDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::NOT_DEFAULT, true,
         false, false);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnknownChromeUnknown) {
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::UNKNOWN_DEFAULT,
         true, false, false);
}
