// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "components/physical_web/data_source/fake_physical_web_data_source.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::ListValue;
using physical_web::CreateDummyPhysicalWebPages;
using physical_web::FakePhysicalWebDataSource;
using testing::_;
using testing::AnyNumber;
using testing::ElementsAre;
using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace ntp_snippets {

namespace {

MATCHER_P(HasUrl, url, "") {
  *result_listener << "expected URL: " << url
                   << "has URL: " << arg.url().spec();
  return arg.url().spec() == url;
}

}  // namespace

class PhysicalWebPageSuggestionsProviderTest : public testing::Test {
 public:
  PhysicalWebPageSuggestionsProviderTest()
      : pref_service_(base::MakeUnique<TestingPrefServiceSimple>()) {}

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

  PhysicalWebPageSuggestionsProvider* CreateProvider() {
    DCHECK(!provider_);
    provider_ = base::MakeUnique<PhysicalWebPageSuggestionsProvider>(
        &observer_, &category_factory_, &physical_web_data_source_);
    return provider_.get();
  }

  void DestroyProvider() { provider_.reset(); }

  Category provided_category() {
    return category_factory_.FromKnownCategory(
        KnownCategories::PHYSICAL_WEB_PAGES);
  }

  void FireUrlFound(const std::string& url) {
    physical_web_data_source_.NotifyOnFound(url);
  }

  void FireUrlLost(const std::string& url) {
    physical_web_data_source_.NotifyOnLost(url);
  }

  void FireUrlDistanceChanged(const std::string& url, double new_distance) {
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
  CategoryFactory category_factory_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  // Last so that the dependencies are deleted after the provider.
  std::unique_ptr<PhysicalWebPageSuggestionsProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebPageSuggestionsProviderTest);
};

TEST_F(PhysicalWebPageSuggestionsProviderTest,
       ShouldSubmitSuggestionsOnStartup) {
  physical_web_data_source()->SetMetadata(
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
  physical_web_data_source()->SetMetadata(
      CreateDummyPhysicalWebPages({3, 2, 1}));
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, provided_category(),
                       ElementsAre(HasUrl("https://resolved_url.com/3"),
                                   HasUrl("https://resolved_url.com/2"),
                                   HasUrl("https://resolved_url.com/1"))));
  CreateProvider();
}

}  // namespace ntp_snippets
