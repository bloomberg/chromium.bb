// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "components/ntp_snippets/offline_pages/offline_pages_test_utils.h"
#include "components/offline_pages/core/background/request_coordinator_stub_taco.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/downloads/download_ui_adapter.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/recent_tabs/recent_tabs_ui_adapter_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ntp_snippets::test::CaptureDismissedSuggestions;
using ntp_snippets::test::FakeOfflinePageModel;
using offline_pages::ClientId;
using offline_pages::MultipleOfflinePageItemCallback;
using offline_pages::OfflinePageItem;
using testing::_;
using testing::IsEmpty;
using testing::Mock;
using testing::Property;
using testing::SizeIs;

namespace ntp_snippets {

namespace {

OfflinePageItem CreateDummyRecentTab(int offline_id) {
  // This is used to assign unique tab IDs to pages.  Since offline IDs are
  // typically small integers like 1, 2, 3 etc, we start at 1001 to ensure that
  // they are different, and can catch bugs where offline page ID is used in
  // place of tab ID and vice versa.
  std::string tab_id = base::IntToString(offline_id + 1000);
  ClientId client_id(offline_pages::kLastNNamespace, tab_id);
  return test::CreateDummyOfflinePageItem(offline_id, client_id);
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

class RecentTabSuggestionsProviderTestNoLoad : public testing::Test {
 public:
  RecentTabSuggestionsProviderTestNoLoad()
      : task_runner_(new base::TestSimpleTaskRunner()),
        task_runner_handle_(task_runner_),
        pref_service_(new TestingPrefServiceSimple()) {
    RecentTabSuggestionsProvider::RegisterProfilePrefs(
        pref_service()->registry());

    taco_ = base::MakeUnique<offline_pages::RequestCoordinatorStubTaco>();
    taco_->CreateRequestCoordinator();

    ui_adapter_ = offline_pages::RecentTabsUIAdapterDelegate::
        GetOrCreateRecentTabsUIAdapter(&model_, taco_->request_coordinator());
    delegate_ =
        offline_pages::RecentTabsUIAdapterDelegate::FromDownloadUIAdapter(
            ui_adapter_);
    provider_ = base::MakeUnique<RecentTabSuggestionsProvider>(
        &observer_, ui_adapter_, pref_service());
  }

  Category recent_tabs_category() {
    return Category::FromKnownCategory(KnownCategories::RECENT_TABS);
  }

  ContentSuggestion::ID GetDummySuggestionId(int id) {
    return ContentSuggestion::ID(recent_tabs_category(), base::IntToString(id));
  }

  void AddTabAndOfflinePageToModel(const OfflinePageItem& item) {
    AddTab(offline_pages::RecentTabsUIAdapterDelegate::TabIdFromClientId(
        item.client_id));
    AddOfflinePageToModel(item);
  }

  void AddTab(int tab_id) { delegate_->RegisterTab(tab_id); }

  void RemoveTab(int tab_id) { delegate_->UnregisterTab(tab_id); }

  void AddOfflinePageToModel(const OfflinePageItem& item) {
    ui_adapter_->OfflinePageAdded(&model_, item);
  }

  void FireOfflinePageDeleted(const OfflinePageItem& item) {
    int tab_id = offline_pages::RecentTabsUIAdapterDelegate::TabIdFromClientId(
        item.client_id);
    RemoveTab(tab_id);
    ui_adapter_->OfflinePageDeleted(item.offline_id, item.client_id);
  }

  std::set<std::string> ReadDismissedIDsFromPrefs() {
    return provider_->ReadDismissedIDsFromPrefs();
  }

  RecentTabSuggestionsProvider* provider() { return provider_.get(); }
  MockContentSuggestionsProviderObserver* observer() { return &observer_; }
  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }
  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  FakeOfflinePageModel model_;
  offline_pages::DownloadUIAdapter* ui_adapter_;
  offline_pages::RecentTabsUIAdapterDelegate* delegate_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<offline_pages::RequestCoordinatorStubTaco> taco_;
  MockContentSuggestionsProviderObserver observer_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  // Last so that the dependencies are deleted after the provider.
  std::unique_ptr<RecentTabSuggestionsProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabSuggestionsProviderTestNoLoad);
};

// Test that always loads the model before the start of the test.
class RecentTabSuggestionsProviderTest
    : public RecentTabSuggestionsProviderTestNoLoad {
 public:
  RecentTabSuggestionsProviderTest() = default;

  void SetUp() override {
    // The UI adapter always fires asynchronously upon loading, so we want to
    // run past that moment before each test.  Expect a call to hide warnings.
    EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(1);
    task_runner()->RunUntilIdle();
    Mock::VerifyAndClearExpectations(observer());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RecentTabSuggestionsProviderTest);
};

TEST_F(RecentTabSuggestionsProviderTest, ShouldConvertToSuggestions) {
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(2);
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

  auto recent_tabs_list = CreateDummyRecentTabs({1, 2, 3});
  for (OfflinePageItem& recent_tab : recent_tabs_list) {
    AddTabAndOfflinePageToModel(recent_tab);
  }
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
  AddTabAndOfflinePageToModel(CreateDummyRecentTab(1, now));

  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          ElementsAre(
              Property(&ContentSuggestion::url, GURL("http://dummy.com/1")),
              Property(&ContentSuggestion::url, GURL("http://dummy.com/2")))));
  AddTabAndOfflinePageToModel(CreateDummyRecentTab(2, yesterday));

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
  AddTabAndOfflinePageToModel(CreateDummyRecentTab(3, tomorrow));
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldDeliverCorrectCategoryInfo) {
  EXPECT_FALSE(
      provider()->GetCategoryInfo(recent_tabs_category()).has_fetch_action());
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
    AddTabAndOfflinePageToModel(recent_tab);
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

  AddTabAndOfflinePageToModel(CreateDummyRecentTab(4));
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
  AddTabAndOfflinePageToModel(CreateDummyRecentTab(5));
  Mock::VerifyAndClearExpectations(observer());
}

TEST_F(RecentTabSuggestionsProviderTest,
       ShouldInvalidateWhenOfflinePageDeleted) {
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(3);
  std::vector<OfflinePageItem> offline_pages = CreateDummyRecentTabs({1, 2, 3});
  for (OfflinePageItem& recent_tab : offline_pages)
    AddTabAndOfflinePageToModel(recent_tab);

  // Invalidation of suggestion 2 should be forwarded.
  EXPECT_CALL(*observer(), OnSuggestionInvalidated(_, GetDummySuggestionId(2)));
  FireOfflinePageDeleted(offline_pages[1]);
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldClearDismissedOnInvalidate) {
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(3);
  std::vector<OfflinePageItem> offline_pages = CreateDummyRecentTabs({1, 2, 3});
  for (OfflinePageItem& recent_tab : offline_pages)
    AddTabAndOfflinePageToModel(recent_tab);
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
    AddTabAndOfflinePageToModel(recent_tab);

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

  AddTabAndOfflinePageToModel(offline_pages[0]);
  AddTabAndOfflinePageToModel(offline_pages[1]);
  Mock::VerifyAndClearExpectations(observer());
  EXPECT_CALL(*observer(),
              OnNewSuggestions(
                  _, recent_tabs_category(),
                  UnorderedElementsAre(
                      Property(&ContentSuggestion::publish_date, now),
                      Property(&ContentSuggestion::publish_date, tomorrow))));

  AddTabAndOfflinePageToModel(offline_pages[2]);
}

TEST_F(RecentTabSuggestionsProviderTest,
       ShouldNotFetchIfAddedOfflinePageIsNotRecentTab) {
  // It should not fetch when not a recent tab is added, thus, it should not
  // report the first recent tab (which it is not aware about).
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(0);
  AddOfflinePageToModel(ntp_snippets::test::CreateDummyOfflinePageItem(
      2, offline_pages::kDefaultNamespace));
}

TEST_F(RecentTabSuggestionsProviderTest,
       ShouldInvalidateSuggestionWhenTabGone) {
  OfflinePageItem first_tab = CreateDummyRecentTab(1);
  AddTabAndOfflinePageToModel(first_tab);
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnSuggestionInvalidated(_, GetDummySuggestionId(1)))
      .Times(1);
  RemoveTab(offline_pages::RecentTabsUIAdapterDelegate::TabIdFromClientId(
      first_tab.client_id));
  // Removing an unknown tab should not cause extra invalidations.
  RemoveTab(42);
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldNotShowPagesWithoutTab) {
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(0);
  // The provider is not notified about the first recent tab yet (no tab).
  OfflinePageItem first_tab = CreateDummyRecentTab(1);
  AddOfflinePageToModel(first_tab);

  Mock::VerifyAndClearExpectations(observer());
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(1);
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          UnorderedElementsAre(
              Property(&ContentSuggestion::url, GURL("http://dummy.com/1")),
              Property(&ContentSuggestion::url, GURL("http://dummy.com/2")))));

  AddTab(offline_pages::RecentTabsUIAdapterDelegate::TabIdFromClientId(
      first_tab.client_id));
  OfflinePageItem second_tab = CreateDummyRecentTab(2);
  AddTabAndOfflinePageToModel(second_tab);

  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(0);
  // |RemoveTab| by itself doesn't cause OnNewSuggestions to be called.
  RemoveTab(offline_pages::RecentTabsUIAdapterDelegate::TabIdFromClientId(
      second_tab.client_id));
  Mock::VerifyAndClearExpectations(observer());

  // But when we get another tab, OnNewSuggestions will be called.
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          UnorderedElementsAre(
              Property(&ContentSuggestion::url, GURL("http://dummy.com/1")),
              Property(&ContentSuggestion::url, GURL("http://dummy.com/3")))));

  AddTabAndOfflinePageToModel(CreateDummyRecentTab(3));
}

// The following test uses a different fixture that does not automatically pump
// the event loop in SetUp, which means that until |RunUntilIdle| is called, the
// UI adapter will not be loaded (and should not fire any events).

TEST_F(RecentTabSuggestionsProviderTestNoLoad, ShouldFetchOnLoad) {
  // Tabs are added to the model before the UI adapter is loaded, so there
  // should only be a single |OnNewSuggestions| call, at load time.
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          UnorderedElementsAre(
              Property(&ContentSuggestion::url, GURL("http://dummy.com/1")),
              Property(&ContentSuggestion::url, GURL("http://dummy.com/2")))));

  AddTabAndOfflinePageToModel(CreateDummyRecentTab(1));
  AddTabAndOfflinePageToModel(CreateDummyRecentTab(2));
  // The provider is not notified about the recent tabs yet.
  task_runner()->RunUntilIdle();
  // However, it must return both tabs when the model is loaded.
}

}  // namespace ntp_snippets
