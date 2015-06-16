// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_page_url_condition_tracker.h"

#include <set>

#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

class DeclarativeContentPageUrlConditionTrackerTest
    : public DeclarativeContentConditionTrackerTest {
 protected:
  class Delegate : public DeclarativeContentConditionTrackerDelegate {
   public:
    Delegate() {}

    std::set<content::WebContents*>& evaluation_requests() {
      return evaluation_requests_;
    }

    // DeclarativeContentConditionTrackerDelegate:
    void RequestEvaluation(content::WebContents* contents) override {
      EXPECT_FALSE(ContainsKey(evaluation_requests_, contents));
      evaluation_requests_.insert(contents);
    }

    bool ShouldManageConditionsForBrowserContext(
        content::BrowserContext* context) override {
      return true;
    }

   private:
    std::set<content::WebContents*> evaluation_requests_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  DeclarativeContentPageUrlConditionTrackerTest() {}

  void LoadURL(content::WebContents* tab, const GURL& url) {
    tab->GetController().LoadURL(url, content::Referrer(),
                                 ui::PAGE_TRANSITION_LINK, std::string());
  }

  Delegate delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentPageUrlConditionTrackerTest);
};

// Tests that adding and removing condition sets trigger evaluation requests for
// the matching WebContents.
TEST_F(DeclarativeContentPageUrlConditionTrackerTest,
       AddAndRemoveConditionSets) {
  DeclarativeContentPageUrlConditionTracker tracker(browser_context(),
                                                    &delegate_);

  // Create two tabs.
  ScopedVector<content::WebContents> tabs;
  for (int i = 0; i < 2; ++i) {
    tabs.push_back(MakeTab());
    delegate_.evaluation_requests().clear();
    tracker.TrackForWebContents(tabs.back());
    EXPECT_THAT(delegate_.evaluation_requests(),
                UnorderedElementsAre(tabs.back()));
  }

  // Navigate one of them to a URL that will match with the rule we're about to
  // add.
  LoadURL(tabs[0], GURL("http://test1/"));

  const int condition_set_id = 100;
  std::set<url_matcher::URLMatcherCondition> conditions;
  conditions.insert(
      tracker.condition_factory()->CreateHostPrefixCondition("test1"));
  std::vector<scoped_refptr<url_matcher::URLMatcherConditionSet>>
      condition_sets(1,
                     new url_matcher::URLMatcherConditionSet(
                         condition_set_id, conditions));
  delegate_.evaluation_requests().clear();
  tracker.AddConditionSets(condition_sets);
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAreArray(tabs));
  std::set<int> match_ids;
  tracker.GetMatches(tabs[0], &match_ids);
  EXPECT_THAT(match_ids, UnorderedElementsAre(condition_set_id));
  tracker.GetMatches(tabs[1], &match_ids);
  EXPECT_TRUE(match_ids.empty());

  delegate_.evaluation_requests().clear();
  tracker.RemoveConditionSets(std::vector<int>(1, condition_set_id));
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAreArray(tabs));
}

// Tests that tracking WebContents triggers evaluation requests for matching
// rules.
TEST_F(DeclarativeContentPageUrlConditionTrackerTest, TrackWebContents) {
  DeclarativeContentPageUrlConditionTracker tracker(browser_context(),
                                                    &delegate_);

  const int condition_set_id = 100;
  std::set<url_matcher::URLMatcherCondition> conditions;
  conditions.insert(
      tracker.condition_factory()->CreateHostPrefixCondition("test1"));
  std::vector<scoped_refptr<url_matcher::URLMatcherConditionSet>>
      condition_sets(1,
                     new url_matcher::URLMatcherConditionSet(
                         condition_set_id, conditions));
  tracker.AddConditionSets(condition_sets);
  EXPECT_TRUE(delegate_.evaluation_requests().empty());

  const scoped_ptr<content::WebContents> matching_tab = MakeTab();
  LoadURL(matching_tab.get(), GURL("http://test1/"));

  tracker.TrackForWebContents(matching_tab.get());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(matching_tab.get()));

  delegate_.evaluation_requests().clear();
  const scoped_ptr<content::WebContents> non_matching_tab = MakeTab();
  tracker.TrackForWebContents(non_matching_tab.get());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(non_matching_tab.get()));
}

// Tests that notifying WebContents navigation triggers evaluation requests for
// matching rules.
TEST_F(DeclarativeContentPageUrlConditionTrackerTest,
       NotifyWebContentsNavigation) {
  DeclarativeContentPageUrlConditionTracker tracker(browser_context(),
                                                    &delegate_);

  const int condition_set_id = 100;
  std::set<url_matcher::URLMatcherCondition> conditions;
  conditions.insert(
      tracker.condition_factory()->CreateHostPrefixCondition("test1"));
  std::vector<scoped_refptr<url_matcher::URLMatcherConditionSet>>
      condition_sets(1,
                     new url_matcher::URLMatcherConditionSet(
                         condition_set_id, conditions));
  tracker.AddConditionSets(condition_sets);
  EXPECT_TRUE(delegate_.evaluation_requests().empty());

  const scoped_ptr<content::WebContents> tab = MakeTab();
  tracker.TrackForWebContents(tab.get());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));

  // Check that navigation notification to a matching URL results in an
  // evaluation request.
  LoadURL(tab.get(), GURL("http://test1/"));
  delegate_.evaluation_requests().clear();
  tracker.OnWebContentsNavigation(tab.get(), content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));

  // Check that navigation notification from a matching URL to another matching
  // URL results in an evaluation request.
  LoadURL(tab.get(), GURL("http://test1/a"));
  delegate_.evaluation_requests().clear();
  tracker.OnWebContentsNavigation(tab.get(), content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));

  // Check that navigation notification from a matching URL to a non-matching
  // URL results in an evaluation request.
  delegate_.evaluation_requests().clear();
  LoadURL(tab.get(), GURL("http://test2/"));
  tracker.OnWebContentsNavigation(tab.get(), content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));

  // Check that navigation notification from a non-matching URL to another
  // non-matching URL results in an evaluation request.
  delegate_.evaluation_requests().clear();
  LoadURL(tab.get(), GURL("http://test2/a"));
  tracker.OnWebContentsNavigation(tab.get(), content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(),
              UnorderedElementsAre(tab.get()));
}

}  // namespace extensions
