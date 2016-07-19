// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_service.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_category_status.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using testing::Eq;
using testing::IsNull;
using testing::NotNull;
using testing::IsEmpty;
using testing::ElementsAre;
using testing::Property;
using testing::Const;
using testing::Mock;
using testing::ByRef;
using testing::_;

namespace ntp_snippets {

namespace {

// Returns a suggestion instance for testing.
ContentSuggestion CreateSuggestion(int number) {
  return ContentSuggestion(
      base::IntToString(number),
      GURL("http://testsuggestion/" + base::IntToString(number)));
}

std::vector<ContentSuggestion> CreateSuggestions(std::vector<int> numbers) {
  std::vector<ContentSuggestion> result;
  for (int number : numbers) {
    result.emplace_back(CreateSuggestion(number));
  }
  return result;
}

class MockProvider : public ContentSuggestionsProvider {
 public:
  MockProvider(ContentSuggestionsCategory provided_category)
      : MockProvider(
            std::vector<ContentSuggestionsCategory>({provided_category})){};

  MockProvider(std::vector<ContentSuggestionsCategory> provided_categories)
      : ContentSuggestionsProvider(provided_categories), observer_(nullptr) {
    for (ContentSuggestionsCategory category : provided_categories) {
      statuses_[category] = ContentSuggestionsCategoryStatus::AVAILABLE;
    }
  }

  Observer* observer() { return observer_; }

  void SetObserver(Observer* observer) override { observer_ = observer; }

  ContentSuggestionsCategoryStatus GetCategoryStatus(
      ContentSuggestionsCategory category) {
    return statuses_[category];
  }

  void FireSuggestionsChanged(ContentSuggestionsCategory category,
                              std::vector<int> numbers) {
    observer_->OnNewSuggestions(category, CreateSuggestions(numbers));
  }

  void FireCategoryStatusChanged(ContentSuggestionsCategory category,
                                 ContentSuggestionsCategoryStatus new_status) {
    statuses_[category] = new_status;
    observer_->OnCategoryStatusChanged(category, new_status);
  }

  void FireShutdown() {
    observer_->OnProviderShutdown(this);
    observer_ = nullptr;
  }

  MOCK_METHOD0(ClearCachedSuggestionsForDebugging, void());
  MOCK_METHOD0(ClearDiscardedSuggestionsForDebugging, void());
  MOCK_METHOD1(DiscardSuggestion, void(const std::string& suggestion_id));
  MOCK_METHOD2(FetchSuggestionImage,
               void(const std::string& suggestion_id,
                    const ImageFetchedCallback& callback));

 private:
  Observer* observer_;
  std::map<ContentSuggestionsCategory, ContentSuggestionsCategoryStatus>
      statuses_;
};

class MockServiceObserver : public ContentSuggestionsService::Observer {
 public:
  MOCK_METHOD0(OnNewSuggestions, void());
  MOCK_METHOD2(OnCategoryStatusChanged,
               void(ContentSuggestionsCategory changed_category,
                    ContentSuggestionsCategoryStatus new_status));
  MOCK_METHOD0(ContentSuggestionsServiceShutdown, void());
  ~MockServiceObserver() override {}
};

}  // namespace

class ContentSuggestionsServiceTest : public testing::Test {
 public:
  ContentSuggestionsServiceTest() {}

  void SetUp() override {
    CreateContentSuggestionsService(ContentSuggestionsService::State::ENABLED);
  }

  void TearDown() override {
    service_->Shutdown();
    service_.reset();
  }

  // Verifies that exactly the suggestions with the given |numbers| are
  // returned by the service for the given |category|.
  void ExpectThatSuggestionsAre(ContentSuggestionsCategory category,
                                std::vector<int> numbers) {
    std::vector<ContentSuggestionsCategory> categories =
        service()->GetCategories();
    auto position = std::find(categories.begin(), categories.end(), category);
    if (!numbers.empty()) {
      EXPECT_NE(categories.end(), position);
    }

    for (const auto& suggestion :
         service()->GetSuggestionsForCategory(category)) {
      int id;
      ASSERT_TRUE(base::StringToInt(suggestion.id(), &id));
      auto position = std::find(numbers.begin(), numbers.end(), id);
      if (position == numbers.end()) {
        ADD_FAILURE() << "Unexpected suggestion with ID " << id;
      } else {
        numbers.erase(position);
      }
    }
    for (int number : numbers) {
      ADD_FAILURE() << "Suggestion number " << number
                    << " not present, though expected";
    }
  }

  const std::map<ContentSuggestionsCategory, ContentSuggestionsProvider*>&
  providers() {
    return service()->providers_;
  }

  MOCK_METHOD2(OnImageFetched,
               void(const std::string& suggestion_id, const gfx::Image&));

 protected:
  void CreateContentSuggestionsService(
      ContentSuggestionsService::State enabled) {
    ASSERT_FALSE(service_);
    service_.reset(new ContentSuggestionsService(enabled));
  }

  ContentSuggestionsService* service() { return service_.get(); }

 private:
  std::unique_ptr<ContentSuggestionsService> service_;

  DISALLOW_COPY_AND_ASSIGN(ContentSuggestionsServiceTest);
};

class ContentSuggestionsServiceDisabledTest
    : public ContentSuggestionsServiceTest {
 public:
  void SetUp() override {
    CreateContentSuggestionsService(ContentSuggestionsService::State::DISABLED);
  }
};

TEST_F(ContentSuggestionsServiceTest, ShouldRegisterProvidersAndShutdown) {
  EXPECT_THAT(service()->state(),
              Eq(ContentSuggestionsService::State::ENABLED));
  MockProvider provider1(ContentSuggestionsCategory::ARTICLES);
  MockProvider provider2(ContentSuggestionsCategory::OFFLINE_PAGES);
  ASSERT_THAT(provider1.observer(), IsNull());
  ASSERT_THAT(provider2.observer(), IsNull());
  ASSERT_THAT(providers(), IsEmpty());
  EXPECT_THAT(service()->GetCategories(), IsEmpty());
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::ARTICLES),
      Eq(ContentSuggestionsCategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::OFFLINE_PAGES),
      Eq(ContentSuggestionsCategoryStatus::NOT_PROVIDED));

  service()->RegisterProvider(&provider1);
  EXPECT_THAT(provider1.observer(), NotNull());
  EXPECT_THAT(providers().count(ContentSuggestionsCategory::OFFLINE_PAGES),
              Eq(0ul));
  EXPECT_THAT(providers().at(ContentSuggestionsCategory::ARTICLES),
              Eq(&provider1));
  EXPECT_THAT(providers().size(), Eq(1ul));
  EXPECT_THAT(service()->GetCategories(),
              ElementsAre(ContentSuggestionsCategory::ARTICLES));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::ARTICLES),
      Eq(ContentSuggestionsCategoryStatus::AVAILABLE));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::OFFLINE_PAGES),
      Eq(ContentSuggestionsCategoryStatus::NOT_PROVIDED));

  service()->RegisterProvider(&provider2);
  EXPECT_THAT(provider1.observer(), NotNull());
  EXPECT_THAT(provider2.observer(), NotNull());
  EXPECT_THAT(providers().at(ContentSuggestionsCategory::ARTICLES),
              Eq(&provider1));
  EXPECT_THAT(providers().at(ContentSuggestionsCategory::OFFLINE_PAGES),
              Eq(&provider2));
  EXPECT_THAT(providers().size(), Eq(2ul));
  EXPECT_THAT(service()->GetCategories(),
              ElementsAre(ContentSuggestionsCategory::ARTICLES,
                          ContentSuggestionsCategory::OFFLINE_PAGES));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::ARTICLES),
      Eq(ContentSuggestionsCategoryStatus::AVAILABLE));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::OFFLINE_PAGES),
      Eq(ContentSuggestionsCategoryStatus::AVAILABLE));

  provider1.FireShutdown();
  EXPECT_THAT(providers().count(ContentSuggestionsCategory::ARTICLES), Eq(0ul));
  EXPECT_THAT(providers().at(ContentSuggestionsCategory::OFFLINE_PAGES),
              Eq(&provider2));
  EXPECT_THAT(providers().size(), Eq(1ul));
  EXPECT_THAT(service()->GetCategories(),
              ElementsAre(ContentSuggestionsCategory::OFFLINE_PAGES));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::ARTICLES),
      ContentSuggestionsCategoryStatus::NOT_PROVIDED);
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::OFFLINE_PAGES),
      ContentSuggestionsCategoryStatus::AVAILABLE);

  provider2.FireShutdown();
  EXPECT_THAT(providers(), IsEmpty());
  EXPECT_THAT(service()->GetCategories(), IsEmpty());
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::ARTICLES),
      Eq(ContentSuggestionsCategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::OFFLINE_PAGES),
      Eq(ContentSuggestionsCategoryStatus::NOT_PROVIDED));
}

TEST_F(ContentSuggestionsServiceDisabledTest, ShouldDoNothingWhenDisabled) {
  EXPECT_THAT(service()->state(),
              Eq(ContentSuggestionsService::State::DISABLED));
  MockProvider provider1(ContentSuggestionsCategory::ARTICLES);
  service()->RegisterProvider(&provider1);
  EXPECT_THAT(providers(), IsEmpty());
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::ARTICLES),
      Eq(ContentSuggestionsCategoryStatus::
             ALL_SUGGESTIONS_EXPLICITLY_DISABLED));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::OFFLINE_PAGES),
      Eq(ContentSuggestionsCategoryStatus::
             ALL_SUGGESTIONS_EXPLICITLY_DISABLED));
  EXPECT_THAT(service()->GetCategories(), IsEmpty());
  EXPECT_THAT(service()->GetSuggestionsForCategory(
                  ContentSuggestionsCategory::ARTICLES),
              IsEmpty());
}

TEST_F(ContentSuggestionsServiceTest, ShouldRedirectFetchSuggestionImage) {
  MockProvider provider1(ContentSuggestionsCategory::ARTICLES);
  MockProvider provider2(ContentSuggestionsCategory::OFFLINE_PAGES);
  service()->RegisterProvider(&provider1);
  service()->RegisterProvider(&provider2);

  provider1.FireSuggestionsChanged(ContentSuggestionsCategory::ARTICLES, {1});
  std::string suggestion_id = CreateSuggestion(1).id();

  EXPECT_CALL(provider1, FetchSuggestionImage(suggestion_id, _)).Times(1);
  EXPECT_CALL(provider2, FetchSuggestionImage(_, _)).Times(0);
  service()->FetchSuggestionImage(
      suggestion_id, base::Bind(&ContentSuggestionsServiceTest::OnImageFetched,
                                base::Unretained(this)));
  provider1.FireShutdown();
  provider2.FireShutdown();
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldCallbackEmptyImageForUnavailableProvider) {
  std::string suggestion_id = "TestID";
  EXPECT_CALL(*this, OnImageFetched(suggestion_id,
                                    Property(&gfx::Image::IsEmpty, Eq(true))));
  service()->FetchSuggestionImage(
      suggestion_id, base::Bind(&ContentSuggestionsServiceTest::OnImageFetched,
                                base::Unretained(this)));
}

TEST_F(ContentSuggestionsServiceTest, ShouldRedirectDiscardSuggestion) {
  MockProvider provider1(ContentSuggestionsCategory::ARTICLES);
  MockProvider provider2(ContentSuggestionsCategory::OFFLINE_PAGES);
  service()->RegisterProvider(&provider1);
  service()->RegisterProvider(&provider2);

  provider2.FireSuggestionsChanged(ContentSuggestionsCategory::OFFLINE_PAGES,
                                   {11});
  std::string suggestion_id = CreateSuggestion(11).id();

  EXPECT_CALL(provider1, DiscardSuggestion(_)).Times(0);
  EXPECT_CALL(provider2, DiscardSuggestion(suggestion_id)).Times(1);
  service()->DiscardSuggestion(suggestion_id);
  provider1.FireShutdown();
  provider2.FireShutdown();
}

TEST_F(ContentSuggestionsServiceTest, ShouldForwardSuggestions) {
  // Create and register providers
  MockProvider provider1(ContentSuggestionsCategory::ARTICLES);
  MockProvider provider2(ContentSuggestionsCategory::OFFLINE_PAGES);
  service()->RegisterProvider(&provider1);
  service()->RegisterProvider(&provider2);
  EXPECT_THAT(providers().at(ContentSuggestionsCategory::ARTICLES),
              Eq(&provider1));
  EXPECT_THAT(providers().at(ContentSuggestionsCategory::OFFLINE_PAGES),
              Eq(&provider2));

  // Create and register observer
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // Send suggestions 1 and 2
  EXPECT_CALL(observer, OnNewSuggestions()).Times(1);
  provider1.FireSuggestionsChanged(ContentSuggestionsCategory::ARTICLES,
                                   {1, 2});
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::ARTICLES, {1, 2});
  Mock::VerifyAndClearExpectations(&observer);

  // Send them again, make sure they're not reported twice
  EXPECT_CALL(observer, OnNewSuggestions()).Times(1);
  provider1.FireSuggestionsChanged(ContentSuggestionsCategory::ARTICLES,
                                   {1, 2});
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::ARTICLES, {1, 2});
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::OFFLINE_PAGES,
                           std::vector<int>());
  Mock::VerifyAndClearExpectations(&observer);

  // Send suggestions 13 and 14
  EXPECT_CALL(observer, OnNewSuggestions()).Times(1);
  provider2.FireSuggestionsChanged(ContentSuggestionsCategory::OFFLINE_PAGES,
                                   {13, 14});
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::ARTICLES, {1, 2});
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::OFFLINE_PAGES, {13, 14});
  Mock::VerifyAndClearExpectations(&observer);

  // Send suggestion 1 only
  EXPECT_CALL(observer, OnNewSuggestions()).Times(1);
  provider1.FireSuggestionsChanged(ContentSuggestionsCategory::ARTICLES, {1});
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::ARTICLES, {1});
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::OFFLINE_PAGES, {13, 14});
  Mock::VerifyAndClearExpectations(&observer);

  // provider2 reports OFFLINE_PAGEs as unavailable
  EXPECT_CALL(
      observer,
      OnCategoryStatusChanged(
          ContentSuggestionsCategory::OFFLINE_PAGES,
          ContentSuggestionsCategoryStatus::CATEGORY_EXPLICITLY_DISABLED))
      .Times(1);
  provider2.FireCategoryStatusChanged(
      ContentSuggestionsCategory::OFFLINE_PAGES,
      ContentSuggestionsCategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::ARTICLES),
      Eq(ContentSuggestionsCategoryStatus::AVAILABLE));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::OFFLINE_PAGES),
      Eq(ContentSuggestionsCategoryStatus::CATEGORY_EXPLICITLY_DISABLED));
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::ARTICLES, {1});
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::OFFLINE_PAGES,
                           std::vector<int>());
  Mock::VerifyAndClearExpectations(&observer);

  // Let provider1 shut down
  EXPECT_CALL(observer, OnCategoryStatusChanged(
                            ContentSuggestionsCategory::ARTICLES,
                            ContentSuggestionsCategoryStatus::NOT_PROVIDED))
      .Times(1);
  provider1.FireShutdown();
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::ARTICLES),
      Eq(ContentSuggestionsCategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::OFFLINE_PAGES),
      Eq(ContentSuggestionsCategoryStatus::CATEGORY_EXPLICITLY_DISABLED));
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::ARTICLES,
                           std::vector<int>());
  ExpectThatSuggestionsAre(ContentSuggestionsCategory::OFFLINE_PAGES,
                           std::vector<int>());
  Mock::VerifyAndClearExpectations(&observer);

  // Let provider2 shut down
  provider2.FireShutdown();
  EXPECT_TRUE(providers().empty());
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::ARTICLES),
      Eq(ContentSuggestionsCategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(
      service()->GetCategoryStatus(ContentSuggestionsCategory::OFFLINE_PAGES),
      Eq(ContentSuggestionsCategoryStatus::NOT_PROVIDED));

  // Shutdown the service
  EXPECT_CALL(observer, ContentSuggestionsServiceShutdown());
  service()->Shutdown();
  service()->RemoveObserver(&observer);
  // The service will receive two Shutdown() calls.
}

}  // namespace ntp_snippets
