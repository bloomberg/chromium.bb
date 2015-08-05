// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"

#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_delegate.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_test.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/common/extension_messages.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

using testing::HasSubstr;
using testing::UnorderedElementsAreArray;

class DeclarativeContentCssConditionTrackerTest
    : public DeclarativeContentConditionTrackerTest {
 protected:
  DeclarativeContentCssConditionTrackerTest() {}

  class Delegate : public DeclarativeContentConditionTrackerDelegate {
   public:
    Delegate() : evaluation_requests_(0) {}

    int evaluation_requests() { return evaluation_requests_; }

    // DeclarativeContentConditionTrackerDelegate:
    void RequestEvaluation(content::WebContents* contents) override {
      ++evaluation_requests_;
    }

    bool ShouldManageConditionsForBrowserContext(
        content::BrowserContext* context) override {
      return true;
    }

   private:
    int evaluation_requests_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Expect an ExtensionMsg_WatchPages message in |sink| with |selectors| as the
  // param, after invoking |func|.
  template <class Func>
  void ExpectWatchPagesMessage(content::WebContents* tab,
                               const std::set<std::string>& selectors,
                               const Func& func) {
    IPC::TestSink& sink = GetMockRenderProcessHost(tab)->sink();
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
    EXPECT_TRUE(GetMockRenderProcessHost(tab)->OnMessageReceived(page_change));
  }

  Delegate delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentCssConditionTrackerTest);
};

TEST(DeclarativeContentCssPredicateTest, WrongCssDatatype) {
  std::string error;
  scoped_ptr<DeclarativeContentCssPredicate> predicate =
      DeclarativeContentCssPredicate::Create(
          *base::test::ParseJson("\"selector\""),
          &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(predicate);
}

TEST(DeclarativeContentCssPredicateTest, CssPredicate) {
  std::string error;
  scoped_ptr<DeclarativeContentCssPredicate> predicate =
      DeclarativeContentCssPredicate::Create(
          *base::test::ParseJson("[\"input\"]"),
          &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(predicate);

  base::hash_set<std::string> matched_css_selectors;
  matched_css_selectors.insert("input");

  EXPECT_TRUE(predicate->Evaluate(matched_css_selectors));

  matched_css_selectors.clear();
  matched_css_selectors.insert("body");
  EXPECT_FALSE(predicate->Evaluate(matched_css_selectors));
}

// Tests the basic flow of operations on the
// DeclarativeContentCssConditionTracker.
TEST_F(DeclarativeContentCssConditionTrackerTest, Basic) {
  DeclarativeContentCssConditionTracker tracker(profile(), &delegate_);
  int expected_evaluation_requests = 0;

  const scoped_ptr<content::WebContents> tab = MakeTab();
  tracker.TrackForWebContents(tab.get());
  EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());

  // Check that calling SetWatchedCssSelectors sends a WatchPages message with
  // the selectors to the tab's RenderProcessHost.
  std::set<std::string> watched_selectors;
  watched_selectors.insert("a");
  watched_selectors.insert("div");
  ExpectWatchPagesMessage(tab.get(), watched_selectors,
                          [&tracker, &watched_selectors]() {
    tracker.SetWatchedCssSelectors(watched_selectors);
  });
  EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());

  // Check that receiving an OnWatchedPageChange message from the tab results in
  // a request for condition evaluation.
  const std::vector<std::string> matched_selectors(1, "div");
  SendOnWatchedPageChangeMessage(tab.get(), matched_selectors);
  EXPECT_EQ(++expected_evaluation_requests, delegate_.evaluation_requests());

  // Check that GetMatchingCssSelectors produces the same matched selectors as
  // were sent by the OnWatchedPageChange message.
  base::hash_set<std::string> matching_selectors;
  tracker.GetMatchingCssSelectors(tab.get(), &matching_selectors);
  EXPECT_THAT(matching_selectors,
              UnorderedElementsAreArray(matched_selectors));
  EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());

  // Check that an in-page navigation has no effect on the matching selectors.
  {
    content::LoadCommittedDetails details;
    details.is_in_page = true;
    content::FrameNavigateParams params;
    tracker.OnWebContentsNavigation(tab.get(), details, params);
    matching_selectors.clear();
    tracker.GetMatchingCssSelectors(tab.get(), &matching_selectors);
    EXPECT_THAT(matching_selectors,
                UnorderedElementsAreArray(matched_selectors));
    EXPECT_EQ(expected_evaluation_requests, delegate_.evaluation_requests());
  }

  // Check that a non in-page navigation clears the matching selectors and
  // requests condition evaluation.
  {
    content::LoadCommittedDetails details;
    details.is_in_page = false;
    content::FrameNavigateParams params;
    tracker.OnWebContentsNavigation(tab.get(), details, params);
    matching_selectors.clear();
    tracker.GetMatchingCssSelectors(tab.get(), &matching_selectors);
    EXPECT_TRUE(matching_selectors.empty());
    EXPECT_EQ(++expected_evaluation_requests,
              delegate_.evaluation_requests());
  }
}

// https://crbug.com/497586
TEST_F(DeclarativeContentCssConditionTrackerTest, WebContentsOutlivesTracker) {
  const scoped_ptr<content::WebContents> tab = MakeTab();

  {
    DeclarativeContentCssConditionTracker tracker(profile(), &delegate_);
    tracker.TrackForWebContents(tab.get());
  }
}

}  // namespace extensions
