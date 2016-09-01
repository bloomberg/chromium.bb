// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/offline_page_suggestions_provider.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/stub_offline_page_model.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using offline_pages::ClientId;
using offline_pages::MultipleOfflinePageItemCallback;
using offline_pages::OfflinePageItem;
using offline_pages::StubOfflinePageModel;
using testing::AllOf;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::Property;
using testing::ElementsAre;
using testing::Mock;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;
using testing::SizeIs;
using testing::WhenSortedBy;
using testing::_;

namespace ntp_snippets {

namespace {

struct OrderByMostRecentlyVisited {
  bool operator()(const OfflinePageItem* left,
                  const OfflinePageItem* right) const {
    return left->last_access_time > right->last_access_time;
  }
};

OfflinePageItem CreateDummyItem(std::string name_space, int id) {
  std::string strid = base::IntToString(id);
  return OfflinePageItem(
      GURL("http://dummy.com/" + strid), id,
      ClientId(name_space, base::GenerateGUID()),
      base::FilePath::FromUTF8Unsafe("some/folder/test" + strid + ".mhtml"), 0,
      base::Time::Now());
}

OfflinePageItem CreateDummyRecentTab(int id) {
  return CreateDummyItem(offline_pages::kLastNNamespace, id);
}

OfflinePageItem CreateDummyRecentTab(int id, base::Time time) {
  OfflinePageItem item = CreateDummyRecentTab(id);
  item.last_access_time = time;
  return item;
}

OfflinePageItem CreateDummyDownload(int id) {
  return CreateDummyItem(offline_pages::kAsyncNamespace, id);
}

void CaptureDismissedSuggestions(
    std::vector<ContentSuggestion>* captured_suggestions,
    std::vector<ContentSuggestion> dismissed_suggestions) {
  std::move(dismissed_suggestions.begin(), dismissed_suggestions.end(),
            std::back_inserter(*captured_suggestions));
}

}  // namespace

class FakeOfflinePageModel : public StubOfflinePageModel {
 public:
  FakeOfflinePageModel() {}

  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override {
    callback.Run(items_);
  }

  const std::vector<OfflinePageItem>& items() { return items_; }
  std::vector<OfflinePageItem>* mutable_items() { return &items_; }

 private:
  std::vector<OfflinePageItem> items_;
};

class OfflinePageSuggestionsProviderTest : public testing::Test {
 public:
  OfflinePageSuggestionsProviderTest()
      : pref_service_(new TestingPrefServiceSimple()) {
    OfflinePageSuggestionsProvider::RegisterProfilePrefs(
        pref_service()->registry());
    CreateProvider(true, true, true);
  }

  void RecreateProvider(bool recent_tabs_enabled,
                        bool downloads_enabled,
                        bool download_manager_ui_enabled) {
    provider_.reset();
    CreateProvider(recent_tabs_enabled, downloads_enabled,
                   download_manager_ui_enabled);
  }

  void CreateProvider(bool recent_tabs_enabled,
                      bool downloads_enabled,
                      bool download_manager_ui_enabled) {
    DCHECK(!provider_);

    provider_.reset(new OfflinePageSuggestionsProvider(
        recent_tabs_enabled, downloads_enabled, download_manager_ui_enabled,
        &observer_, &category_factory_, &model_, pref_service()));
  }

  Category recent_tabs_category() {
    return category_factory_.FromKnownCategory(KnownCategories::RECENT_TABS);
  }

  Category downloads_category() {
    return category_factory_.FromKnownCategory(KnownCategories::DOWNLOADS);
  }

  std::string GetDummySuggestionId(Category category, int id) {
    return provider_->MakeUniqueID(category, base::IntToString(id));
  }

  ContentSuggestion CreateDummySuggestion(Category category, int id) {
    std::string strid = base::IntToString(id);
    ContentSuggestion result(
        GetDummySuggestionId(category, id),
        GURL("file:///some/folder/test" + strid + ".mhtml"));
    result.set_title(base::UTF8ToUTF16("http://dummy.com/" + strid));
    return result;
  }

  void FireOfflinePageModelChanged() {
    provider_->OfflinePageModelChanged(model());
  }

  void FireOfflinePageDeleted(const OfflinePageItem& item) {
    provider_->OfflinePageDeleted(item.offline_id, item.client_id);
  }

  std::set<std::string> ReadDismissedIDsFromPrefs(Category category) {
    return provider_->ReadDismissedIDsFromPrefs(category);
  }

  ContentSuggestionsProvider* provider() { return provider_.get(); }
  FakeOfflinePageModel* model() { return &model_; }
  MockContentSuggestionsProviderObserver* observer() { return &observer_; }
  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }

 private:
  FakeOfflinePageModel model_;
  MockContentSuggestionsProviderObserver observer_;
  CategoryFactory category_factory_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  // Last so that the dependencies are deleted after the provider.
  std::unique_ptr<OfflinePageSuggestionsProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageSuggestionsProviderTest);
};

TEST_F(OfflinePageSuggestionsProviderTest, ShouldSplitAndConvertToSuggestions) {
  model()->mutable_items()->push_back(CreateDummyRecentTab(1));
  model()->mutable_items()->push_back(CreateDummyRecentTab(2));
  model()->mutable_items()->push_back(CreateDummyRecentTab(3));
  model()->mutable_items()->push_back(CreateDummyDownload(101));

  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, recent_tabs_category(),
                       UnorderedElementsAre(
                           Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test1.mhtml")),
                           Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test2.mhtml")),
                           Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test3.mhtml")))));

  EXPECT_CALL(*observer(),
              OnNewSuggestions(
                  _, downloads_category(),
                  UnorderedElementsAre(AllOf(
                      Property(&ContentSuggestion::url,
                               GURL("file:///some/folder/test101.mhtml")),
                      Property(&ContentSuggestion::title,
                               base::UTF8ToUTF16("http://dummy.com/101"))))));

  FireOfflinePageModelChanged();
}

TEST_F(OfflinePageSuggestionsProviderTest, ShouldIgnoreDisabledCategories) {
  model()->mutable_items()->push_back(CreateDummyRecentTab(1));
  model()->mutable_items()->push_back(CreateDummyRecentTab(2));
  model()->mutable_items()->push_back(CreateDummyRecentTab(3));
  model()->mutable_items()->push_back(CreateDummyDownload(101));

  // Disable recent tabs, enable downloads.
  EXPECT_CALL(*observer(), OnNewSuggestions(_, recent_tabs_category(), _))
      .Times(0);
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, downloads_category(),
                       UnorderedElementsAre(Property(
                           &ContentSuggestion::url,
                           GURL("file:///some/folder/test101.mhtml")))));
  RecreateProvider(false, true, true);
  Mock::VerifyAndClearExpectations(observer());

  // Enable recent tabs, disable downloads.
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, recent_tabs_category(),
                       UnorderedElementsAre(
                           Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test1.mhtml")),
                           Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test2.mhtml")),
                           Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test3.mhtml")))));
  EXPECT_CALL(*observer(), OnNewSuggestions(_, downloads_category(), _))
      .Times(0);
  RecreateProvider(true, false, true);
  Mock::VerifyAndClearExpectations(observer());
}

TEST_F(OfflinePageSuggestionsProviderTest, ShouldSortByMostRecentlyVisited) {
  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::TimeDelta::FromDays(1);
  base::Time tomorrow = now + base::TimeDelta::FromDays(1);
  model()->mutable_items()->push_back(CreateDummyRecentTab(1, now));
  model()->mutable_items()->push_back(CreateDummyRecentTab(2, yesterday));
  model()->mutable_items()->push_back(CreateDummyRecentTab(3, tomorrow));

  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          ElementsAre(Property(&ContentSuggestion::url,
                               GURL("file:///some/folder/test3.mhtml")),
                      Property(&ContentSuggestion::url,
                               GURL("file:///some/folder/test1.mhtml")),
                      Property(&ContentSuggestion::url,
                               GURL("file:///some/folder/test2.mhtml")))));
  EXPECT_CALL(*observer(), OnNewSuggestions(_, downloads_category(), _));
  FireOfflinePageModelChanged();
}

TEST_F(OfflinePageSuggestionsProviderTest, ShouldDeliverCorrectCategoryInfo) {
  EXPECT_FALSE(
      provider()->GetCategoryInfo(recent_tabs_category()).has_more_button());
  EXPECT_TRUE(
      provider()->GetCategoryInfo(downloads_category()).has_more_button());
  RecreateProvider(true, true, false);
  EXPECT_FALSE(
      provider()->GetCategoryInfo(recent_tabs_category()).has_more_button());
  EXPECT_FALSE(
      provider()->GetCategoryInfo(downloads_category()).has_more_button());
}

TEST_F(OfflinePageSuggestionsProviderTest, ShouldDismiss) {
  model()->mutable_items()->push_back(CreateDummyRecentTab(1));
  model()->mutable_items()->push_back(CreateDummyRecentTab(2));
  model()->mutable_items()->push_back(CreateDummyRecentTab(3));
  model()->mutable_items()->push_back(CreateDummyRecentTab(4));
  FireOfflinePageModelChanged();

  // Dismiss 2 and 3.
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(0);
  provider()->DismissSuggestion(
      GetDummySuggestionId(recent_tabs_category(), 2));
  provider()->DismissSuggestion(
      GetDummySuggestionId(recent_tabs_category(), 3));
  Mock::VerifyAndClearExpectations(observer());

  // They should disappear from the reported suggestions.
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, recent_tabs_category(),
                       UnorderedElementsAre(
                           Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test1.mhtml")),
                           Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test4.mhtml")))));
  EXPECT_CALL(*observer(),
              OnNewSuggestions(_, downloads_category(), IsEmpty()));
  FireOfflinePageModelChanged();
  Mock::VerifyAndClearExpectations(observer());

  // And appear in the dismissed suggestions for the right category.
  std::vector<ContentSuggestion> dismissed_suggestions;
  provider()->GetDismissedSuggestionsForDebugging(
      recent_tabs_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(
      dismissed_suggestions,
      UnorderedElementsAre(Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test2.mhtml")),
                           Property(&ContentSuggestion::url,
                                    GURL("file:///some/folder/test3.mhtml"))));

  // The other category should have no dismissed suggestions.
  dismissed_suggestions.clear();
  provider()->GetDismissedSuggestionsForDebugging(
      downloads_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(dismissed_suggestions, IsEmpty());

  // Clear dismissed suggestions.
  provider()->ClearDismissedSuggestionsForDebugging(recent_tabs_category());

  // They should be gone from the dismissed suggestions.
  dismissed_suggestions.clear();
  provider()->GetDismissedSuggestionsForDebugging(
      recent_tabs_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(dismissed_suggestions, IsEmpty());

  // And appear in the reported suggestions for the category again.
  EXPECT_CALL(*observer(),
              OnNewSuggestions(_, recent_tabs_category(), SizeIs(4)));
  EXPECT_CALL(*observer(),
              OnNewSuggestions(_, downloads_category(), IsEmpty()));
  FireOfflinePageModelChanged();
  Mock::VerifyAndClearExpectations(observer());
}

TEST_F(OfflinePageSuggestionsProviderTest,
       ShouldInvalidateWhenOfflinePageDeleted) {
  model()->mutable_items()->push_back(CreateDummyRecentTab(1));
  model()->mutable_items()->push_back(CreateDummyRecentTab(2));
  model()->mutable_items()->push_back(CreateDummyRecentTab(3));
  FireOfflinePageModelChanged();

  // Invalidation of suggestion 2 should be forwarded.
  EXPECT_CALL(
      *observer(),
      OnSuggestionInvalidated(_, recent_tabs_category(),
                              GetDummySuggestionId(recent_tabs_category(), 2)));
  FireOfflinePageDeleted(model()->items().at(1));
}

TEST_F(OfflinePageSuggestionsProviderTest, ShouldClearDismissedOnInvalidate) {
  model()->mutable_items()->push_back(CreateDummyRecentTab(1));
  model()->mutable_items()->push_back(CreateDummyRecentTab(2));
  model()->mutable_items()->push_back(CreateDummyRecentTab(3));
  FireOfflinePageModelChanged();
  EXPECT_THAT(ReadDismissedIDsFromPrefs(recent_tabs_category()), IsEmpty());
  EXPECT_THAT(ReadDismissedIDsFromPrefs(downloads_category()), IsEmpty());

  provider()->DismissSuggestion(
      GetDummySuggestionId(recent_tabs_category(), 2));
  EXPECT_THAT(ReadDismissedIDsFromPrefs(recent_tabs_category()), SizeIs(1));
  EXPECT_THAT(ReadDismissedIDsFromPrefs(downloads_category()), IsEmpty());

  FireOfflinePageDeleted(model()->items().at(1));
  EXPECT_THAT(ReadDismissedIDsFromPrefs(recent_tabs_category()), IsEmpty());
  EXPECT_THAT(ReadDismissedIDsFromPrefs(downloads_category()), IsEmpty());
}

TEST_F(OfflinePageSuggestionsProviderTest, ShouldClearDismissedOnFetch) {
  model()->mutable_items()->push_back(CreateDummyDownload(1));
  model()->mutable_items()->push_back(CreateDummyDownload(2));
  model()->mutable_items()->push_back(CreateDummyDownload(3));
  FireOfflinePageModelChanged();

  provider()->DismissSuggestion(GetDummySuggestionId(downloads_category(), 2));
  provider()->DismissSuggestion(GetDummySuggestionId(downloads_category(), 3));
  EXPECT_THAT(ReadDismissedIDsFromPrefs(recent_tabs_category()), IsEmpty());
  EXPECT_THAT(ReadDismissedIDsFromPrefs(downloads_category()), SizeIs(2));

  model()->mutable_items()->clear();
  model()->mutable_items()->push_back(CreateDummyDownload(2));
  FireOfflinePageModelChanged();
  EXPECT_THAT(ReadDismissedIDsFromPrefs(recent_tabs_category()), IsEmpty());
  EXPECT_THAT(ReadDismissedIDsFromPrefs(downloads_category()), SizeIs(1));

  model()->mutable_items()->clear();
  FireOfflinePageModelChanged();
  EXPECT_THAT(ReadDismissedIDsFromPrefs(recent_tabs_category()), IsEmpty());
  EXPECT_THAT(ReadDismissedIDsFromPrefs(downloads_category()), IsEmpty());
}

}  // namespace ntp_snippets
