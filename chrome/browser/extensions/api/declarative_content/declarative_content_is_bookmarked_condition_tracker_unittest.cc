// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_is_bookmarked_condition_tracker.h"

#include <set>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

scoped_refptr<Extension> CreateExtensionWithBookmarksPermission(
    bool include_bookmarks) {
  ListBuilder permissions;
  permissions.Append("declarativeContent");
  if (include_bookmarks)
    permissions.Append("bookmarks");
  return ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "Test extension")
                   .Set("version", "1.0")
                   .Set("manifest_version", 2)
                   .Set("permissions", permissions))
      .Build();
}

}  // namespace

using testing::HasSubstr;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

class DeclarativeContentIsBookmarkedConditionTrackerTest
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

  DeclarativeContentIsBookmarkedConditionTrackerTest() {
    profile()->CreateBookmarkModel(true);
    bookmarks::test::WaitForBookmarkModelToLoad(
        BookmarkModelFactory::GetForProfile(profile()));
    bookmark_model_ = BookmarkModelFactory::GetForProfile(profile());
  }

  void LoadURL(content::WebContents* tab, const GURL& url) {
    tab->GetController().LoadURL(url, content::Referrer(),
                                 ui::PAGE_TRANSITION_LINK, std::string());
  }

  Delegate delegate_;
  bookmarks::BookmarkModel* bookmark_model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentIsBookmarkedConditionTrackerTest);
};


// Tests that condition with isBookmarked requires "bookmarks" permission.
TEST(DeclarativeContentIsBookmarkedPredicateTest,
     IsBookmarkedPredicateRequiresBookmarkPermissionPermission) {
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(false);
  std::string error;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> predicate =
      CreateIsBookmarkedPredicate(*base::test::ParseJson("true"),
                                  extension.get(),
                                  &error);
  EXPECT_THAT(error, HasSubstr("requires 'bookmarks' permission"));
  EXPECT_FALSE(predicate);
}

// Tests an invalid isBookmarked value type.
TEST(DeclarativeContentIsBookmarkedPredicateTest,
     WrongIsBookmarkedPredicateDatatype) {
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);
  std::string error;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> predicate =
      CreateIsBookmarkedPredicate(*base::test::ParseJson("[]"),
                                  extension.get(),
                                  &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(predicate);
}

// Tests isBookmark: true.
TEST(DeclarativeContentIsBookmarkedPredicateTest, IsBookmarkedPredicateTrue) {
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);
  std::string error;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> predicate =
      CreateIsBookmarkedPredicate(*base::test::ParseJson("true"),
                                  extension.get(),
                                  &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(predicate);

  EXPECT_TRUE(predicate->Evaluate(true /* url_is_bookmarked */));
  EXPECT_FALSE(predicate->Evaluate(false /* url_is_bookmarked */));
}

// Tests isBookmark: false.
TEST(DeclarativeContentIsBookmarkedPredicateTest, IsBookmarkedPredicateFalse) {
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);
  std::string error;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> predicate =
      CreateIsBookmarkedPredicate(*base::test::ParseJson("false"),
                                  extension.get(),
                                  &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(predicate);

  EXPECT_FALSE(predicate->Evaluate(true /* url_is_bookmarked */));
  EXPECT_TRUE(predicate->Evaluate(false /* url_is_bookmarked */));
}

// Tests that starting tracking for a WebContents that has a bookmarked URL
// results in the proper IsUrlBookmarked state.
TEST_F(DeclarativeContentIsBookmarkedConditionTrackerTest,
       BookmarkedAtStartOfTracking) {
  DeclarativeContentIsBookmarkedConditionTracker tracker(profile(), &delegate_);

  scoped_ptr<content::WebContents> tab = MakeTab();
  LoadURL(tab.get(), GURL("http://bookmarked/"));
  EXPECT_TRUE(delegate_.evaluation_requests().empty());

  bookmark_model_->AddURL(bookmark_model_->other_node(), 0,
                          base::ASCIIToUTF16("title"),
                          GURL("http://bookmarked/"));

  tracker.TrackForWebContents(tab.get());
  EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tab.get()));
  EXPECT_TRUE(tracker.IsUrlBookmarked(tab.get()));
}

// Tests that adding and removing bookmarks triggers evaluation requests for the
// matching WebContents.
TEST_F(DeclarativeContentIsBookmarkedConditionTrackerTest,
       AddAndRemoveBookmark) {
  DeclarativeContentIsBookmarkedConditionTracker tracker(profile(), &delegate_);

  // Create two tabs.
  ScopedVector<content::WebContents> tabs;
  for (int i = 0; i < 2; ++i) {
    tabs.push_back(MakeTab());
    delegate_.evaluation_requests().clear();
    tracker.TrackForWebContents(tabs.back());
    EXPECT_THAT(delegate_.evaluation_requests(),
                UnorderedElementsAre(tabs.back()));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs.back()));
  }

  // Navigate the first tab to a URL that we will bookmark.
  delegate_.evaluation_requests().clear();
  LoadURL(tabs[0], GURL("http://bookmarked/"));
  tracker.OnWebContentsNavigation(tabs[0], content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));

  // Bookmark the first tab's URL.
  delegate_.evaluation_requests().clear();
  const bookmarks::BookmarkNode* node =
      bookmark_model_->AddURL(bookmark_model_->other_node(), 0,
                              base::ASCIIToUTF16("title"),
                              GURL("http://bookmarked/"));
  EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
  EXPECT_TRUE(tracker.IsUrlBookmarked(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));

  // Remove the bookmark.
  delegate_.evaluation_requests().clear();
  bookmark_model_->Remove(node);
  EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));
}

// Tests that adding and removing bookmarks triggers evaluation requests for the
// matching WebContents.
TEST_F(DeclarativeContentIsBookmarkedConditionTrackerTest, ExtensiveChanges) {
  DeclarativeContentIsBookmarkedConditionTracker tracker(profile(), &delegate_);

  // Create two tabs.
  ScopedVector<content::WebContents> tabs;
  for (int i = 0; i < 2; ++i) {
    tabs.push_back(MakeTab());
    delegate_.evaluation_requests().clear();
    tracker.TrackForWebContents(tabs.back());
    EXPECT_THAT(delegate_.evaluation_requests(),
                UnorderedElementsAre(tabs.back()));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs.back()));
  }

  // Navigate the first tab to a URL that we will bookmark.
  delegate_.evaluation_requests().clear();
  LoadURL(tabs[0], GURL("http://bookmarked/"));
  tracker.OnWebContentsNavigation(tabs[0], content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));

  {
    // Check that evaluation requests occur outside ExtensiveBookmarkChanges for
    // added nodes.
    delegate_.evaluation_requests().clear();
    bookmark_model_->BeginExtensiveChanges();
    const bookmarks::BookmarkNode* node =
        bookmark_model_->AddURL(bookmark_model_->other_node(), 0,
                                base::ASCIIToUTF16("title"),
                                GURL("http://bookmarked/"));
    EXPECT_TRUE(delegate_.evaluation_requests().empty());
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[0]));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));
    bookmark_model_->EndExtensiveChanges();
    EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
    EXPECT_TRUE(tracker.IsUrlBookmarked(tabs[0]));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));

    // Check that evaluation requests occur outside ExtensiveBookmarkChanges for
    // removed nodes.
    delegate_.evaluation_requests().clear();
    bookmark_model_->BeginExtensiveChanges();
    bookmark_model_->Remove(node);
    EXPECT_TRUE(delegate_.evaluation_requests().empty());
    EXPECT_TRUE(tracker.IsUrlBookmarked(tabs[0]));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));
    bookmark_model_->EndExtensiveChanges();
    EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[0]));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));
  }

  {
    // Check that evaluation requests occur outside ScopedGroupBookmarkActions
    // for added nodes.
    delegate_.evaluation_requests().clear();
    const bookmarks::BookmarkNode* node = nullptr;
    {
      bookmarks::ScopedGroupBookmarkActions scoped_group(bookmark_model_);
      node = bookmark_model_->AddURL(bookmark_model_->other_node(), 0,
                                     base::ASCIIToUTF16("title"),
                                     GURL("http://bookmarked/"));
      EXPECT_TRUE(delegate_.evaluation_requests().empty());
      EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[0]));
      EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));
    }
    EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
    EXPECT_TRUE(tracker.IsUrlBookmarked(tabs[0]));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));

    // Check that evaluation requests occur outside ScopedGroupBookmarkActions
    // for removed nodes.
    delegate_.evaluation_requests().clear();
    {
      bookmarks::ScopedGroupBookmarkActions scoped_group(bookmark_model_);
      bookmark_model_->Remove(node);
      EXPECT_TRUE(delegate_.evaluation_requests().empty());
      EXPECT_TRUE(tracker.IsUrlBookmarked(tabs[0]));
      EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));
    }
    EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[0]));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));
  }
}

// Tests that navigation to bookmarked and non-bookmarked URLs triggers
// evaluation requests for the relevant WebContents.
TEST_F(DeclarativeContentIsBookmarkedConditionTrackerTest, Navigation) {
  DeclarativeContentIsBookmarkedConditionTracker tracker(profile(), &delegate_);

  // Bookmark two URLs.
  delegate_.evaluation_requests().clear();
  bookmark_model_->AddURL(bookmark_model_->other_node(), 0,
                          base::ASCIIToUTF16("title"),
                          GURL("http://bookmarked1/"));
  bookmark_model_->AddURL(bookmark_model_->other_node(), 0,
                          base::ASCIIToUTF16("title"),
                          GURL("http://bookmarked2/"));

  // Create two tabs.
  ScopedVector<content::WebContents> tabs;
  for (int i = 0; i < 2; ++i) {
    tabs.push_back(MakeTab());
    delegate_.evaluation_requests().clear();
    tracker.TrackForWebContents(tabs.back());
    EXPECT_THAT(delegate_.evaluation_requests(),
                UnorderedElementsAre(tabs.back()));
    EXPECT_FALSE(tracker.IsUrlBookmarked(tabs.back()));
  }

  // Navigate the first tab to one bookmarked URL.
  delegate_.evaluation_requests().clear();
  LoadURL(tabs[0], GURL("http://bookmarked1/"));
  tracker.OnWebContentsNavigation(tabs[0], content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
  EXPECT_TRUE(tracker.IsUrlBookmarked(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));

  // Navigate the first tab to another bookmarked URL. The contents have
  // changed, so we should receive a new evaluation request even though the
  // bookmarked state hasn't.
  delegate_.evaluation_requests().clear();
  LoadURL(tabs[0], GURL("http://bookmarked2/"));
  tracker.OnWebContentsNavigation(tabs[0], content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
  EXPECT_TRUE(tracker.IsUrlBookmarked(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));

  // Navigate the first tab to a non-bookmarked URL.
  delegate_.evaluation_requests().clear();
  LoadURL(tabs[0], GURL("http://not-bookmarked1/"));
  tracker.OnWebContentsNavigation(tabs[0], content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));

  // Navigate the first tab to another non-bookmarked URL. The contents have
  // changed, so we should receive a new evaluation request even though the
  // bookmarked state hasn't.
  delegate_.evaluation_requests().clear();
  LoadURL(tabs[0], GURL("http://not-bookmarked2/"));
  tracker.OnWebContentsNavigation(tabs[0], content::LoadCommittedDetails(),
                                  content::FrameNavigateParams());
  EXPECT_THAT(delegate_.evaluation_requests(), UnorderedElementsAre(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[0]));
  EXPECT_FALSE(tracker.IsUrlBookmarked(tabs[1]));
}

}  // namespace extensions
