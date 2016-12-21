// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "components/ntp_snippets/offline_pages/offline_pages_test_utils.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ntp_snippets::test::CaptureDismissedSuggestions;
using ntp_snippets::test::FakeOfflinePageModel;
using offline_pages::ClientId;
using offline_pages::MultipleOfflinePageItemCallback;
using offline_pages::OfflinePageItem;
using offline_pages::StubOfflinePageModel;
using testing::_;
using testing::IsEmpty;
using testing::Mock;
using testing::Property;
using testing::SizeIs;

namespace ntp_snippets {

namespace {

OfflinePageItem CreateDummyRecentTab(int id) {
  return test::CreateDummyOfflinePageItem(id, offline_pages::kLastNNamespace);
}

std::vector<OfflinePageItem> CreateDummyRecentTabs(
    const std::vector<int>& ids) {
  std::vector<OfflinePageItem> result;
  for (int id : ids) {
    result.push_back(CreateDummyRecentTab(id));
  }
  return result;
}

OfflinePageItem CreateDummyRecentTab(int id, base::Time time) {
  OfflinePageItem item = CreateDummyRecentTab(id);
  item.creation_time = time;
  item.last_access_time = time;
  return item;
}

}  // namespace

class RecentTabSuggestionsProviderTest : public testing::Test {
 public:
  RecentTabSuggestionsProviderTest()
      : pref_service_(new TestingPrefServiceSimple()) {
    RecentTabSuggestionsProvider::RegisterProfilePrefs(
        pref_service()->registry());

    provider_.reset(
        new RecentTabSuggestionsProvider(&observer_, &model_, pref_service()));
  }

  Category recent_tabs_category() {
    return Category::FromKnownCategory(KnownCategories::RECENT_TABS);
  }

  ContentSuggestion::ID GetDummySuggestionId(int id) {
    return ContentSuggestion::ID(recent_tabs_category(), base::IntToString(id));
  }

  void AddOfflinePageToModel(const OfflinePageItem& item) {
    model_.mutable_items()->push_back(item);
    provider_->OfflinePageAdded(&model_, item);
  }

  void FireOfflinePageDeleted(const OfflinePageItem& item) {
    auto iter = std::remove(model_.mutable_items()->begin(),
                            model_.mutable_items()->end(), item);
    auto end = model_.mutable_items()->end();
    model_.mutable_items()->erase(iter, end);

    provider_->OfflinePageDeleted(item.offline_id, item.client_id);
  }

  std::set<std::string> ReadDismissedIDsFromPrefs() {
    return provider_->ReadDismissedIDsFromPrefs();
  }

  RecentTabSuggestionsProvider* provider() { return provider_.get(); }
  FakeOfflinePageModel* model() { return &model_; }
  MockContentSuggestionsProviderObserver* observer() { return &observer_; }
  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }

 private:
  FakeOfflinePageModel model_;
  MockContentSuggestionsProviderObserver observer_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  // Last so that the dependencies are deleted after the provider.
  std::unique_ptr<RecentTabSuggestionsProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabSuggestionsProviderTest);
};

TEST_F(RecentTabSuggestionsProviderTest, ShouldConvertToSuggestions) {
  auto recent_tabs_list = CreateDummyRecentTabs({1, 2});
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(2);
  for (OfflinePageItem& recent_tab : recent_tabs_list)
    AddOfflinePageToModel(recent_tab);

  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, recent_tabs_category(),
                       UnorderedElementsAre(
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/1")),
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/2")),
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/3")))));
  AddOfflinePageToModel(CreateDummyRecentTab(3));
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldSortByCreationTime) {
  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::TimeDelta::FromDays(1);
  base::Time tomorrow = now + base::TimeDelta::FromDays(1);

  std::vector<OfflinePageItem> offline_pages = {
      CreateDummyRecentTab(1, now), CreateDummyRecentTab(2, yesterday),
      CreateDummyRecentTab(3, tomorrow)};

  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, recent_tabs_category(),
                       ElementsAre(Property(&ContentSuggestion::url,
                                            GURL("http://dummy.com/1")))));
  AddOfflinePageToModel(CreateDummyRecentTab(1, now));

  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          ElementsAre(
              Property(&ContentSuggestion::url, GURL("http://dummy.com/1")),
              Property(&ContentSuggestion::url, GURL("http://dummy.com/2")))));
  AddOfflinePageToModel(CreateDummyRecentTab(2, yesterday));

  offline_pages[1].last_access_time =
      offline_pages[0].last_access_time + base::TimeDelta::FromHours(1);

  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          ElementsAre(Property(&ContentSuggestion::url,
                               GURL("http://dummy.com/3")),
                      Property(&ContentSuggestion::url,
                               GURL("http://dummy.com/1")),
                      Property(&ContentSuggestion::url,
                               GURL("http://dummy.com/2")))));
  AddOfflinePageToModel(CreateDummyRecentTab(3, tomorrow));
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldDeliverCorrectCategoryInfo) {
  EXPECT_FALSE(
      provider()->GetCategoryInfo(recent_tabs_category()).has_more_action());
  EXPECT_FALSE(
      provider()->GetCategoryInfo(recent_tabs_category()).has_reload_action());
  EXPECT_FALSE(provider()
                   ->GetCategoryInfo(recent_tabs_category())
                   .has_view_all_action());
}

// TODO(vitaliii): Break this test into multiple tests. Currently if it fails,
// it takes long time to find which part of it actually fails.
TEST_F(RecentTabSuggestionsProviderTest, ShouldDismiss) {
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(3);
  auto recent_tabs_list = CreateDummyRecentTabs({1, 2, 3});
  for (OfflinePageItem& recent_tab : recent_tabs_list) {
    AddOfflinePageToModel(recent_tab);
  }

  // Dismiss 2 and 3.
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(0);
  provider()->DismissSuggestion(GetDummySuggestionId(2));
  provider()->DismissSuggestion(GetDummySuggestionId(3));
  Mock::VerifyAndClearExpectations(observer());

  // They should disappear from the reported suggestions.
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          UnorderedElementsAre(
              Property(&ContentSuggestion::url, GURL("http://dummy.com/1")),
              Property(&ContentSuggestion::url, GURL("http://dummy.com/4")))));

  AddOfflinePageToModel(CreateDummyRecentTab(4));
  Mock::VerifyAndClearExpectations(observer());

  // And appear in the dismissed suggestions.
  std::vector<ContentSuggestion> dismissed_suggestions;
  provider()->GetDismissedSuggestionsForDebugging(
      recent_tabs_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(
      dismissed_suggestions,
      UnorderedElementsAre(Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/2")),
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/3"))));

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
              OnNewSuggestions(_, recent_tabs_category(), SizeIs(5)));
  AddOfflinePageToModel(CreateDummyRecentTab(5));
  Mock::VerifyAndClearExpectations(observer());
}

TEST_F(RecentTabSuggestionsProviderTest,
       ShouldInvalidateWhenOfflinePageDeleted) {
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(3);
  std::vector<OfflinePageItem> offline_pages = CreateDummyRecentTabs({1, 2, 3});
  for (OfflinePageItem& recent_tab : offline_pages)
    AddOfflinePageToModel(recent_tab);

  // Invalidation of suggestion 2 should be forwarded.
  EXPECT_CALL(*observer(), OnSuggestionInvalidated(_, GetDummySuggestionId(2)));
  FireOfflinePageDeleted(offline_pages[1]);
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldClearDismissedOnInvalidate) {
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(3);
  std::vector<OfflinePageItem> offline_pages = CreateDummyRecentTabs({1, 2, 3});
  for (OfflinePageItem& recent_tab : offline_pages)
    AddOfflinePageToModel(recent_tab);
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), IsEmpty());

  provider()->DismissSuggestion(GetDummySuggestionId(2));
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), SizeIs(1));

  FireOfflinePageDeleted(offline_pages[1]);
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), IsEmpty());
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldClearDismissedOnFetch) {
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(3);
  std::vector<OfflinePageItem> offline_pages = CreateDummyRecentTabs({1, 2, 3});
  for (OfflinePageItem& recent_tab : offline_pages)
    AddOfflinePageToModel(recent_tab);

  provider()->DismissSuggestion(GetDummySuggestionId(2));
  provider()->DismissSuggestion(GetDummySuggestionId(3));
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), SizeIs(2));

  FireOfflinePageDeleted(offline_pages[0]);
  FireOfflinePageDeleted(offline_pages[2]);
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), SizeIs(1));

  FireOfflinePageDeleted(offline_pages[1]);
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), IsEmpty());
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldNotShowSameUrlMutlipleTimes) {
  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::TimeDelta::FromDays(1);
  base::Time tomorrow = now + base::TimeDelta::FromDays(1);
  std::vector<OfflinePageItem> offline_pages = {
      CreateDummyRecentTab(1, yesterday), CreateDummyRecentTab(2, now),
      CreateDummyRecentTab(3, tomorrow)};

  // We leave IDs different, but make the URLs the same.
  offline_pages[2].url = offline_pages[0].url;

  AddOfflinePageToModel(offline_pages[0]);
  AddOfflinePageToModel(offline_pages[1]);
  Mock::VerifyAndClearExpectations(observer());
  EXPECT_CALL(*observer(),
              OnNewSuggestions(
                  _, recent_tabs_category(),
                  UnorderedElementsAre(
                      Property(&ContentSuggestion::publish_date, now),
                      Property(&ContentSuggestion::publish_date, tomorrow))));

  AddOfflinePageToModel(offline_pages[2]);
}

TEST_F(RecentTabSuggestionsProviderTest,
       ShouldNotFetchIfAddedOfflinePageIsNotRecentTab) {
  // The provider is not notified about the first recent tab yet.
  model()->mutable_items()->push_back(CreateDummyRecentTab(1));
  // It should not fetch when not a recent tab is added, thus, it should not
  // report the first recent tab (which it is not aware about).
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(0);
  AddOfflinePageToModel(ntp_snippets::test::CreateDummyOfflinePageItem(
      2, offline_pages::kDefaultNamespace));
}

TEST_F(RecentTabSuggestionsProviderTest,
       ShouldFetchIfAddedOfflinePageIsRecentTab) {
  // The provider is not notified about the first recent tab yet.
  model()->mutable_items()->push_back(CreateDummyRecentTab(1));
  // However, it must return the first recent tab (i.e. manually fetch it) even
  // when notified about a different recent tab.
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          UnorderedElementsAre(
              Property(&ContentSuggestion::url, GURL("http://dummy.com/1")),
              Property(&ContentSuggestion::url, GURL("http://dummy.com/2")))));
  AddOfflinePageToModel(CreateDummyRecentTab(2));
}

}  // namespace ntp_snippets
