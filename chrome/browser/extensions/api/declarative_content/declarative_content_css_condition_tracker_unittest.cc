// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension_messages.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using testing::UnorderedElementsAreArray;

class DeclarativeContentCssConditionTrackerTest : public testing::Test {
 public:
  DeclarativeContentCssConditionTrackerTest()
      : profile_(new TestingProfile),
        delegate_(web_contents_) {
  }

  ~DeclarativeContentCssConditionTrackerTest() override {
    web_contents_.clear();
    // MockRenderProcessHosts are deleted from the message loop, and their
    // deletion must complete before RenderViewHostTestEnabler's destructor is
    // run.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  class Delegate : public DeclarativeContentCssConditionTrackerDelegate {
   public:
    Delegate(const ScopedVector<content::WebContents>& web_contents)
        : web_contents_(web_contents),
          evaluation_requests_(0) {
    }

    int evaluation_requests() { return evaluation_requests_; }

    // DeclarativeContentCssConditionTrackerDelegate:
    void RequestEvaluation(content::WebContents* contents) override {
      ++evaluation_requests_;
    }

    bool ShouldManageConditionsForBrowserContext(
        content::BrowserContext* context) override {
      return true;
    }

   private:
    // Reference to the test's WebContents vector.
    const ScopedVector<content::WebContents>& web_contents_;
    int evaluation_requests_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Creates a new DeclarativeContentCssConditionTracker and retains ownership.
  DeclarativeContentCssConditionTracker* MakeTracker() {
    trackers_.push_back(
        new DeclarativeContentCssConditionTracker(profile(), delegate()));
    return trackers_.back();
  }

  // Creates a new WebContents and retains ownership.
  content::WebContents* MakeTab() {
    web_contents_.push_back(
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr));
    return web_contents_.back();
  }

  // Expect an ExtensionMsg_WatchPages message in |sink| with |selectors| as the
  // param, after invoking |func|.
  template <class Func>
  void ExpectWatchPagesMessage(IPC::TestSink& sink,
                               const std::set<std::string>& selectors,
                               const Func& func) {
    sink.ClearMessages();
    func();
    EXPECT_EQ(1u, sink.message_count());
    const IPC::Message* message =
        sink.GetUniqueMessageMatching(ExtensionMsg_WatchPages::ID);
    ASSERT_TRUE(message);
    ExtensionMsg_WatchPages::Param params;
    ExtensionMsg_WatchPages::Read(message, &params);
    EXPECT_THAT(base::get<0>(params), UnorderedElementsAreArray(selectors));
  }

  // Sends an OnWatchedPageChange message to the tab.
  void SendOnWatchedPageChangeMessage(
      content::WebContents* tab,
      const std::vector<std::string>& selectors) {
    ExtensionHostMsg_OnWatchedPageChange page_change(tab->GetRoutingID(),
                                                     selectors);
    content::MockRenderProcessHost* process =
        static_cast<content::MockRenderProcessHost*>(
            tab->GetRenderViewHost()->GetProcess());

    EXPECT_TRUE(process->OnMessageReceived(page_change));
  }

  Profile* profile() { return profile_.get(); }
  Delegate* delegate() { return &delegate_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  // Enables MockRenderProcessHosts.
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  const scoped_ptr<TestingProfile> profile_;
  ScopedVector<DeclarativeContentCssConditionTracker> trackers_;
  ScopedVector<content::WebContents> web_contents_;
  Delegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentCssConditionTrackerTest);
};

// Tests the basic flow of operations on the
// DeclarativeContentCssConditionTracker.
TEST_F(DeclarativeContentCssConditionTrackerTest, Basic) {
  DeclarativeContentCssConditionTracker *tracker = MakeTracker();
  int expected_evaluation_requests = 0;

  content::WebContents* const tab = MakeTab();
  tracker->TrackForWebContents(tab);
  EXPECT_EQ(expected_evaluation_requests, delegate()->evaluation_requests());

  content::MockRenderProcessHost* process =
          static_cast<content::MockRenderProcessHost*>(
              tab->GetRenderViewHost()->GetProcess());

  // Check that calling SetWatchedCssSelectors sends a WatchPages message with
  // the selectors to the tab's RenderProcessHost.
  std::set<std::string> watched_selectors;
  watched_selectors.insert("a");
  watched_selectors.insert("div");
  ExpectWatchPagesMessage(process->sink(), watched_selectors,
                          [&tracker, &watched_selectors]() {
    tracker->SetWatchedCssSelectors(watched_selectors);
  });
  EXPECT_EQ(expected_evaluation_requests, delegate()->evaluation_requests());

  // Check that receiving an OnWatchedPageChange message from the tab results in
  // a request for condition evaluation.
  const std::vector<std::string> matched_selectors(1, "div");
  SendOnWatchedPageChangeMessage(tab, matched_selectors);
  EXPECT_EQ(++expected_evaluation_requests, delegate()->evaluation_requests());

  // Check that GetMatchingCssSelectors produces the same matched selectors as
  // were sent by the OnWatchedPageChange message.
  base::hash_set<std::string> matching_selectors;
  tracker->GetMatchingCssSelectors(tab, &matching_selectors);
  EXPECT_THAT(matching_selectors,
              UnorderedElementsAreArray(matched_selectors));
  EXPECT_EQ(expected_evaluation_requests, delegate()->evaluation_requests());

  // Check that an in-page navigation has no effect on the matching selectors.
  {
    content::LoadCommittedDetails details;
    details.is_in_page = true;
    content::FrameNavigateParams params;
    tracker->OnWebContentsNavigation(tab, details, params);
    matching_selectors.clear();
    tracker->GetMatchingCssSelectors(tab, &matching_selectors);
    EXPECT_THAT(matching_selectors,
                UnorderedElementsAreArray(matched_selectors));
    EXPECT_EQ(expected_evaluation_requests, delegate()->evaluation_requests());
  }

  // Check that a non in-page navigation clears the matching selectors and
  // requests condition evaluation.
  {
    content::LoadCommittedDetails details;
    details.is_in_page = false;
    content::FrameNavigateParams params;
    tracker->OnWebContentsNavigation(tab, details, params);
    matching_selectors.clear();
    tracker->GetMatchingCssSelectors(tab, &matching_selectors);
    EXPECT_TRUE(matching_selectors.empty());
    EXPECT_EQ(++expected_evaluation_requests,
              delegate()->evaluation_requests());
  }
}

}  // namespace extensions
