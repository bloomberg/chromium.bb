// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/suspicious_site_trigger.h"

#include "base/test/test_simple_task_runner.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/triggers/mock_trigger_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;

using testing::_;
using testing::Return;

namespace safe_browsing {

namespace {
const char kSuspiciousUrl[] = "https://suspicious.com/";
const char kCleanUrl[] = "https://foo.com/";
}  // namespace

class SuspiciousSiteTriggerTest : public content::RenderViewHostTestHarness {
 public:
  SuspiciousSiteTriggerTest() : task_runner_(new base::TestSimpleTaskRunner) {}
  ~SuspiciousSiteTriggerTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Enable any prefs required for the trigger to run.
    safe_browsing::RegisterProfilePrefs(prefs_.registry());
    prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, true);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
  }

  void CreateTrigger() {
    safe_browsing::SuspiciousSiteTrigger::CreateForWebContents(
        web_contents(), &trigger_manager_, &prefs_, nullptr, nullptr);
    safe_browsing::SuspiciousSiteTrigger* trigger =
        safe_browsing::SuspiciousSiteTrigger::FromWebContents(web_contents());
    // Give the trigger a test task runner that we can synchronize on.
    trigger->SetTaskRunnerForTest(task_runner_);
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateFrame(const std::string& url,
                                 RenderFrameHost* frame) {
    GURL gurl(url);
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(gurl, frame);
    navigation_simulator->Commit();
    RenderFrameHost* final_frame_host =
        navigation_simulator->GetFinalRenderFrameHost();
    return final_frame_host;
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetMainFrame());
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             RenderFrameHost* parent) {
    RenderFrameHost* subframe =
        RenderFrameHostTester::For(parent)->AppendChild("subframe");
    return NavigateFrame(url, subframe);
  }

  void FinishAllNavigations() {
    // Call the trigger's DidStopLoading event handler directly since it is not
    // called as part of the navigating individual frames.
    safe_browsing::SuspiciousSiteTrigger::FromWebContents(web_contents())
        ->DidStopLoading();
  }

  void TriggerSuspiciousSite() {
    // Notify the trigger that a suspicious site was detected.
    safe_browsing::SuspiciousSiteTrigger::FromWebContents(web_contents())
        ->SuspiciousSiteDetected();
  }

  void WaitForTaskRunnerIdle() {
    task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  MockTriggerManager* get_trigger_manager() { return &trigger_manager_; }

 private:
  TestingPrefServiceSimple prefs_;
  MockTriggerManager trigger_manager_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

TEST_F(SuspiciousSiteTriggerTest, RegularPageNonSuspicious) {
  // In a normal case where there are no suspicious URLs on the page, the
  // trigger should not fire.
  CreateTrigger();

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  FinishAllNavigations();
}

TEST_F(SuspiciousSiteTriggerTest, SuspiciousHitDuringLoad) {
  // When a suspicious site is detected in the middle of a page load, a report
  // is created after the page load has finished.
  CreateTrigger();

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(_, _, _, _, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(1);

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kSuspiciousUrl, main_frame);
  TriggerSuspiciousSite();
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  FinishAllNavigations();

  WaitForTaskRunnerIdle();
}

TEST_F(SuspiciousSiteTriggerTest, SuspiciousHitAfterLoad) {
  // When a suspicious site is detected in after a page load, a report is
  // created immediately.
  CreateTrigger();

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(_, _, _, _, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(1);

  RenderFrameHost* main_frame = NavigateMainFrame(kCleanUrl);
  CreateAndNavigateSubFrame(kSuspiciousUrl, main_frame);
  CreateAndNavigateSubFrame(kCleanUrl, main_frame);
  FinishAllNavigations();
  TriggerSuspiciousSite();

  WaitForTaskRunnerIdle();
}

}  // namespace safe_browsing