// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include "base/run_loop.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeExternalProtocolHandlerWorker
    : public shell_integration::DefaultProtocolClientWorker {
 public:
  FakeExternalProtocolHandlerWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol,
      shell_integration::DefaultWebClientState os_state)
      : shell_integration::DefaultProtocolClientWorker(callback, protocol),
        os_state_(os_state) {}

 private:
  ~FakeExternalProtocolHandlerWorker() override = default;

  shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
    return os_state_;
  }

  void SetAsDefaultImpl(const base::Closure& on_finished_callback) override {
    on_finished_callback.Run();
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

  scoped_refptr<shell_integration::DefaultProtocolClientWorker>
  CreateShellWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol) override {
    return new FakeExternalProtocolHandlerWorker(callback, protocol, os_state_);
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return block_state_;
  }

  void BlockRequest() override {
    EXPECT_TRUE(block_state_ == ExternalProtocolHandler::BLOCK ||
                os_state_ == shell_integration::IS_DEFAULT);
    has_blocked_ = true;
  }

  void RunExternalProtocolDialog(const GURL& url,
                                 int render_process_host_id,
                                 int routing_id,
                                 ui::PageTransition page_transition,
                                 bool has_user_gesture) override {
    EXPECT_EQ(block_state_, ExternalProtocolHandler::UNKNOWN);
    EXPECT_NE(os_state_, shell_integration::IS_DEFAULT);
    has_prompted_ = true;
  }

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    EXPECT_EQ(block_state_, ExternalProtocolHandler::DONT_BLOCK);
    EXPECT_NE(os_state_, shell_integration::IS_DEFAULT);
    has_launched_ = true;
  }

  void FinishedProcessingCheck() override {
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
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
  ExternalProtocolHandlerTest() {}

  void SetUp() override {
    local_state_.reset(new TestingPrefServiceSimple);
    profile_.reset(new TestingProfile());
    chrome::RegisterLocalState(local_state_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(local_state_.get());
  }

  void TearDown() override {
    // Ensure that g_accept_requests gets set back to true after test execution.
    ExternalProtocolHandler::PermitLaunchUrl();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
  }

  enum class Action { PROMPT, LAUNCH, BLOCK };

  void DoTest(ExternalProtocolHandler::BlockState block_state,
              shell_integration::DefaultWebClientState os_state,
              Action expected_action) {
    GURL url("mailto:test@test.com");
    EXPECT_FALSE(delegate_.has_prompted());
    EXPECT_FALSE(delegate_.has_launched());
    EXPECT_FALSE(delegate_.has_blocked());

    delegate_.set_block_state(block_state);
    delegate_.set_os_state(os_state);
    ExternalProtocolHandler::LaunchUrlWithDelegate(
        url, 0, 0, ui::PAGE_TRANSITION_LINK, true, &delegate_);
    content::RunAllTasksUntilIdle();

    EXPECT_EQ(expected_action == Action::PROMPT, delegate_.has_prompted());
    EXPECT_EQ(expected_action == Action::LAUNCH, delegate_.has_launched());
    EXPECT_EQ(expected_action == Action::BLOCK, delegate_.has_blocked());
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  FakeExternalProtocolHandlerDelegate delegate_;

  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeDefault) {
  DoTest(ExternalProtocolHandler::BLOCK, shell_integration::IS_DEFAULT,
         Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeNotDefault) {
  DoTest(ExternalProtocolHandler::BLOCK, shell_integration::NOT_DEFAULT,
         Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeUnknown) {
  DoTest(ExternalProtocolHandler::BLOCK, shell_integration::UNKNOWN_DEFAULT,
         Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest,
       TestLaunchSchemeBlockedChromeOtherModeDefault) {
  DoTest(ExternalProtocolHandler::BLOCK,
         shell_integration::OTHER_MODE_IS_DEFAULT, Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK, shell_integration::IS_DEFAULT,
         Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeNotDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK, shell_integration::NOT_DEFAULT,
         Action::LAUNCH);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeUnknown) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK,
         shell_integration::UNKNOWN_DEFAULT, Action::LAUNCH);
}

// TODO(crbug.com/765733): Fix the test.
TEST_F(ExternalProtocolHandlerTest,
       DISABLED_TestLaunchSchemeUnBlockedChromeOtherModeDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK,
         shell_integration::OTHER_MODE_IS_DEFAULT, Action::LAUNCH);
}

TEST_F(ExternalProtocolHandlerTest,
       DISABLED_TestLaunchSchemeUnknownChromeDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::IS_DEFAULT,
         Action::BLOCK);
}

// TODO(crbug.com/765733): Fix the test.
TEST_F(ExternalProtocolHandlerTest,
       DISABLED_TestLaunchSchemeUnknownChromeNotDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::NOT_DEFAULT,
         Action::PROMPT);
}

// TODO(crbug.com/765733): Fix the test.
TEST_F(ExternalProtocolHandlerTest,
       DISABLED_TestLaunchSchemeUnknownChromeUnknown) {
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::UNKNOWN_DEFAULT,
         Action::PROMPT);
}

// TODO(crbug.com/765733): Fix the test.
TEST_F(ExternalProtocolHandlerTest,
       DISABLED_TestLaunchSchemeUnknownChromeOtherModeDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN,
         shell_integration::OTHER_MODE_IS_DEFAULT, Action::PROMPT);
}

TEST_F(ExternalProtocolHandlerTest, TestGetBlockStateUnknown) {
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("tel", profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  EXPECT_TRUE(local_state_->GetDictionary(prefs::kExcludedSchemes)->empty());
  EXPECT_TRUE(
      profile_->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());
}

TEST_F(ExternalProtocolHandlerTest, TestGetBlockStateDefaultBlock) {
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("afp", profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::BLOCK, block_state);
  EXPECT_TRUE(local_state_->GetDictionary(prefs::kExcludedSchemes)->empty());
  EXPECT_TRUE(
      profile_->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());
}

TEST_F(ExternalProtocolHandlerTest, TestGetBlockStateDefaultDontBlock) {
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("mailto", profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);
  EXPECT_TRUE(local_state_->GetDictionary(prefs::kExcludedSchemes)->empty());
  EXPECT_TRUE(
      profile_->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());
}
