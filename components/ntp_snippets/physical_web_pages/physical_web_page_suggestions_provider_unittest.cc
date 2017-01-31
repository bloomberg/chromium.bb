// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "components/ntp_snippets/offline_pages/offline_pages_test_utils.h"
#include "components/ntp_snippets/status.h"
#include "components/physical_web/data_source/fake_physical_web_data_source.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ntp_snippets::test::CaptureDismissedSuggestions;
using physical_web::CreateDummyPhysicalWebPages;
using physical_web::FakePhysicalWebDataSource;
using testing::_;
using testing::AnyNumber;
using testing::AtMost;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Mock;
using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace ntp_snippets {

namespace {

MATCHER_P(HasUrl, url, "") {
  *result_listener << "expected URL: " << url
                   << ", but has URL: " << arg.url().spec();
  return url == arg.url().spec();
}

void CompareFetchMoreResult(
    const base::Closure& done_closure,
    Status expected_status,
    const std::vector<std::string>& expected_suggestion_urls,
    Status actual_status,
    std::vector<ContentSuggestion> actual_suggestions) {
  EXPECT_EQ(expected_status.code, actual_status.code);
  std::vector<std::string> actual_suggestion_urls;
  for (const ContentSuggestion& suggestion : actual_suggestions) {
    actual_suggestion_urls.push_back(suggestion.url().spec());
  }
  EXPECT_THAT(actual_suggestion_urls,
              ElementsAreArray(expected_suggestion_urls));
  done_closure.Run();
}

}  // namespace

class PhysicalWebPageSuggestionsProviderTest : public testing::Test {
 public:
  PhysicalWebPageSuggestionsProviderTest()
      : pref_service_(base::MakeUnique<TestingPrefServiceSimple>()) {
    PhysicalWebPageSuggestionsProvider::RegisterProfilePrefs(
        pref_service_->registry());
  }

  void IgnoreOnCategoryStatusChangedToAvailable() {
    EXPECT_CALL(observer_, OnCategoryStatusChanged(_, provided_category(),
                                                   CategoryStatus::AVAILABLE))
        .Times(AnyNumber());
    EXPECT_CALL(observer_,
                OnCategoryStatusChanged(_, provided_category(),
                                        CategoryStatus::AVAILABLE_LOADING))
        .Times(AnyNumber());
  }

  void IgnoreOnSuggestionInvalidated() {
    EXPECT_CALL(observer_, OnSuggestionInvalidated(_, _)).Times(AnyNumber());
  }

  void IgnoreOnNewSuggestions() {
    EXPECT_CALL(observer_, OnNewSuggestions(_, provided_category(), _))
        .Times(AnyNumber());
  }

  PhysicalWebPageSuggestionsProvider* CreateProvider() {
    DCHECK(!provider_);
    provider_ = base::MakeUnique<PhysicalWebPageSuggestionsProvider>(
        &observer_, &physical_web_data_source_, pref_service_.get());
    return provider_.get();
  }

  void DestroyProvider() { provider_.reset(); }

  Category provided_category() {
    return Category::FromKnownCategory(KnownCategories::PHYSICAL_WEB_PAGES);
  }

  ContentSuggestion::ID GetDummySuggestionId(int id) {
    return ContentSuggestion::ID(
        provided_category(),
        "https://resolved_url.com/" + base::IntToString(id));
  }

  void FireUrlFound(const std::string& url) {
    physical_web_data_source_.NotifyOnFound(GURL(url));
  }

  void FireUrlLost(const std::string& url) {
    physical_web_data_source_.NotifyOnLost(GURL(url));
  }

  void FireUrlDistanceChanged(const GURL& url, double new_distance) {
    physical_web_data_source_.NotifyOnDistanceChanged(url, new_distance);
  }

  ContentSuggestionsProvider* provider() {
    DCHECK(provider_);
    return provider_.get();
  }

  FakePhysicalWebDataSource* physical_web_data_source() {
    return &physical_web_data_source_;
  }
  MockContentSuggestionsProviderObserver* observer() { return &observer_; }
  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }

 private:
  FakePhysicalWebDataSource physical_web_data_source_;
  StrictMock<MockContentSuggestionsProviderObserver> observer_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  // Added in order to test provider's |Fetch| method.
  base::MessageLoop message_loop_;
  // Last so that the dependencies are deleted after the provider.
  std::unique_ptr<PhysicalWebPageSuggestionsProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebPageSuggestionsProviderTest);
};

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldSubmitSuggestionsOnStartup) {
  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({1, 2, 3}));
  EXPECT_CALL(*observer(), OnCategoryStatusChanged(_, provided_category(),
                                                   CategoryStatus::AVAILABLE));
  EXPECT_CALL(*observer(),
              OnNewSuggestions(
                  _, provided_category(),
                  UnorderedElementsAre(HasUrl("https://resolved_url.com/1"),
                                       HasUrl("https://resolved_url.com/2"),
                                       HasUrl("https://resolved_url.com/3"))));
  CreateProvider();
}

TEST_F(PhysicalWebPageSuggestionsProviderTest, ShouldSortByDistance) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  // |CreateDummyPhysicalWebPages| builds pages with distances 1, 2 and 3
  // respectively.
  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({3, 2, 1}));
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, provided_category(),
                       ElementsAre(HasUrl("https://resolved_url.com/3"),
                                   HasUrl("https://resolved_url.com/2"),
                                   HasUrl("https://resolved_url.com/1"))));
  CreateProvider();
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldConsiderPagesWithoutDistanceEstimateFurthest) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  // |CreateDummyPhysicalWebPages| builds pages with distances 1, 2 and 3
  // respectively.
  std::unique_ptr<physical_web::MetadataList> pages =
      CreateDummyPhysicalWebPages({3, 2, 1});
  // Set the second page distance estimate to unknown.
  (*pages)[1].distance_estimate = -1.0;
  physical_web_data_source()->SetMetadataList(std::move(pages));
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, provided_category(),
                       ElementsAre(HasUrl("https://resolved_url.com/3"),
                                   HasUrl("https://resolved_url.com/1"),
                                   HasUrl("https://resolved_url.com/2"))));
  CreateProvider();
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldNotShowSuggestionsWithSameGroupId) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  // |CreateDummyPhysicalWebPages| builds pages with distances 1, 2
  // respectively.
  std::unique_ptr<physical_web::MetadataList> pages =
      CreateDummyPhysicalWebPages({2, 1});
  for (auto& page : *pages) {
    page.group_id = "some_group_id";
  }

  physical_web_data_source()->SetMetadataList(std::move(pages));
  // The closest page should be reported.
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, provided_category(),
                       ElementsAre(HasUrl("https://resolved_url.com/2"))));
  CreateProvider();
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldShowSuggestionsWithEmptyGroupId) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  std::unique_ptr<physical_web::MetadataList> pages =
      CreateDummyPhysicalWebPages({1, 2});
  for (auto& page : *pages) {
    page.group_id = "";
  }

  physical_web_data_source()->SetMetadataList(std::move(pages));
  EXPECT_CALL(*observer(),
              OnNewSuggestions(
                  _, provided_category(),
                  UnorderedElementsAre(HasUrl("https://resolved_url.com/1"),
                                       HasUrl("https://resolved_url.com/2"))));
  CreateProvider();
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       FetchMoreShouldFilterAndReturnSortedSuggestions) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  std::vector<int> ids;
  const int kNumSuggestionsInSection = 10;
  const int kNumSuggestionsInSource = 3 * kNumSuggestionsInSection;
  for (int i = 1; i <= kNumSuggestionsInSource; ++i) {
    ids.push_back(i);
  }
  // |CreateDummyPhysicalWebPages| builds pages with distances 1, 2, 3, ... ,
  // so we know the order of suggestions in the provider.
  physical_web_data_source()->SetMetadataList(CreateDummyPhysicalWebPages(ids));
  EXPECT_CALL(*observer(), OnNewSuggestions(_, provided_category(), _));
  CreateProvider();

  const std::string dummy_resolved_url = "https://resolved_url.com/";
  std::set<std::string> known_ids;
  for (int i = 1; i <= kNumSuggestionsInSection; ++i) {
    known_ids.insert(dummy_resolved_url + base::IntToString(i));
  }
  known_ids.insert(dummy_resolved_url + base::IntToString(12));
  known_ids.insert(dummy_resolved_url + base::IntToString(17));
  std::vector<std::string> expected_suggestion_urls;
  for (int i = 1; i <= kNumSuggestionsInSource; ++i) {
    if (expected_suggestion_urls.size() == kNumSuggestionsInSection) {
      break;
    }
    std::string url = dummy_resolved_url + base::IntToString(i);
    if (!known_ids.count(url)) {
      expected_suggestion_urls.push_back(url);
    }
  }

  // Added to wait for |Fetch| callback to be called.
  base::RunLoop run_loop;
  provider()->Fetch(provided_category(), known_ids,
                    base::Bind(CompareFetchMoreResult, run_loop.QuitClosure(),
                               Status::Success(), expected_suggestion_urls));
  // Wait for the callback to be called.
  run_loop.Run();
}

TEST_F(PhysicalWebPageSuggestionsProviderTest, ShouldDismiss) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  physical_web_data_source()->SetMetadataList(CreateDummyPhysicalWebPages({1}));
  EXPECT_CALL(*observer(), OnNewSuggestions(_, provided_category(), _))
      .Times(AtMost(1));
  CreateProvider();

  provider()->DismissSuggestion(GetDummySuggestionId(1));

  std::vector<ContentSuggestion> dismissed_suggestions;
  provider()->GetDismissedSuggestionsForDebugging(
      provided_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(dismissed_suggestions,
              ElementsAre(HasUrl("https://resolved_url.com/1")));
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldNotShowDismissedSuggestions) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  physical_web_data_source()->SetMetadataList(CreateDummyPhysicalWebPages({1}));
  EXPECT_CALL(*observer(), OnNewSuggestions(_, provided_category(), _))
      .Times(AtMost(1));
  CreateProvider();
  Mock::VerifyAndClearExpectations(observer());

  provider()->DismissSuggestion(GetDummySuggestionId(1));

  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({1, 2}));
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, provided_category(),
                       ElementsAre(HasUrl("https://resolved_url.com/2"))));
  FireUrlFound("https://resolved_url.com/2");
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldPruneDismissedSuggestionsWhenFetching) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  IgnoreOnNewSuggestions();
  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({1, 2}));
  CreateProvider();

  provider()->DismissSuggestion(GetDummySuggestionId(1));
  provider()->DismissSuggestion(GetDummySuggestionId(2));

  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({2, 3}));
  FireUrlFound("https://resolved_url.com/3");

  // The first page needs to be silently added back to the source, because
  // |GetDismissedSuggestionsForDebugging| uses the data source to return
  // suggestions and dismissed suggestions, which are not present there, cannot
  // be returned.
  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({1, 2, 3}));
  std::vector<ContentSuggestion> dismissed_suggestions;
  provider()->GetDismissedSuggestionsForDebugging(
      provided_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(dismissed_suggestions,
              ElementsAre(HasUrl("https://resolved_url.com/2")));
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldPruneDismissedSuggestionsWhenUrlLost) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  IgnoreOnNewSuggestions();
  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({1, 2}));
  CreateProvider();

  provider()->DismissSuggestion(GetDummySuggestionId(1));
  provider()->DismissSuggestion(GetDummySuggestionId(2));

  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({2}));
  FireUrlLost("https://resolved_url.com/1");

  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({1, 2}));
  std::vector<ContentSuggestion> dismissed_suggestions;
  provider()->GetDismissedSuggestionsForDebugging(
      provided_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(dismissed_suggestions,
              ElementsAre(HasUrl("https://resolved_url.com/2")));
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldStoreDismissedSuggestions) {
  IgnoreOnCategoryStatusChangedToAvailable();
  IgnoreOnSuggestionInvalidated();
  IgnoreOnNewSuggestions();
  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({1, 2}));
  CreateProvider();
  provider()->DismissSuggestion(GetDummySuggestionId(1));
  DestroyProvider();

  CreateProvider();
  std::vector<ContentSuggestion> dismissed_suggestions;
  provider()->GetDismissedSuggestionsForDebugging(
      provided_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(dismissed_suggestions,
              ElementsAre(HasUrl("https://resolved_url.com/1")));
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldInvalidateSuggestionWhenItsOnlyBeaconIsLost) {
  IgnoreOnCategoryStatusChangedToAvailable();
  physical_web_data_source()->SetMetadataList(
      CreateDummyPhysicalWebPages({1, 2}));

  EXPECT_CALL(*observer(), OnNewSuggestions(_, provided_category(), _));
  CreateProvider();

  EXPECT_CALL(*observer(), OnSuggestionInvalidated(_, GetDummySuggestionId(1)));
  FireUrlLost("https://scanned_url.com/1");
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldNotInvalidateSuggestionWhenBeaconWithDifferentScannedURLRemains) {
  IgnoreOnCategoryStatusChangedToAvailable();
  // Make 2 beacons point to the same URL, while having different |scanned_url|.
  std::unique_ptr<physical_web::MetadataList> pages =
      CreateDummyPhysicalWebPages({1, 2});
  (*pages)[1].resolved_url = (*pages)[0].resolved_url;
  physical_web_data_source()->SetMetadataList(std::move(pages));

  EXPECT_CALL(*observer(), OnNewSuggestions(_, provided_category(), _));
  CreateProvider();

  // The first beacons is lost, but the second one still points to the same
  // |resolved_url|, so the suggestion must not be invalidated.
  EXPECT_CALL(*observer(), OnSuggestionInvalidated(_, _)).Times(0);
  FireUrlLost("https://scanned_url.com/1");
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldNotInvalidateSuggestionWhenBeaconWithSameScannedURLRemains) {
  IgnoreOnCategoryStatusChangedToAvailable();
  // Make 2 beacons point to the same URL, while having the same |scanned_url|.
  std::unique_ptr<physical_web::MetadataList> pages =
      CreateDummyPhysicalWebPages({1, 2});
  (*pages)[1].scanned_url = (*pages)[0].scanned_url;
  (*pages)[1].resolved_url = (*pages)[0].resolved_url;
  physical_web_data_source()->SetMetadataList(std::move(pages));

  EXPECT_CALL(*observer(), OnNewSuggestions(_, provided_category(), _));
  CreateProvider();

  // The first beacons is lost, but the second one still points to the same
  // |resolved_url|, so the suggestion must not be invalidated.
  EXPECT_CALL(*observer(), OnSuggestionInvalidated(_, _)).Times(0);
  FireUrlLost("https://scanned_url.com/1");
}

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldInvalidateSuggestionWhenAllBeaconsLost) {
  IgnoreOnCategoryStatusChangedToAvailable();
  // Make 3 beacons point to the same URL. Two of them have the same
  // |scanned_url|.
  std::unique_ptr<physical_web::MetadataList> pages =
      CreateDummyPhysicalWebPages({1, 2, 3});
  (*pages)[1].scanned_url = (*pages)[0].scanned_url;
  (*pages)[1].resolved_url = (*pages)[0].resolved_url;
  (*pages)[2].resolved_url = (*pages)[0].resolved_url;
  physical_web_data_source()->SetMetadataList(std::move(pages));

  EXPECT_CALL(*observer(), OnNewSuggestions(_, provided_category(), _));
  CreateProvider();

  FireUrlLost("https://scanned_url.com/1");
  FireUrlLost("https://scanned_url.com/1");
  EXPECT_CALL(*observer(), OnSuggestionInvalidated(_, GetDummySuggestionId(1)));
  FireUrlLost("https://scanned_url.com/3");
}

}  // namespace ntp_snippets
