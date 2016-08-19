// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_service.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using testing::ByRef;
using testing::Const;
using testing::ElementsAre;
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::IsNull;
using testing::Mock;
using testing::NotNull;
using testing::Property;
using testing::_;

namespace ntp_snippets {

namespace {

// Returns a suggestion instance for testing.
ContentSuggestion CreateSuggestion(int number) {
  return ContentSuggestion(
      base::IntToString(number),
      GURL("http://testsuggestion/" + base::IntToString(number)));
}

std::vector<ContentSuggestion> CreateSuggestions(
    const std::vector<int>& numbers) {
  std::vector<ContentSuggestion> result;
  for (int number : numbers) {
    result.emplace_back(CreateSuggestion(number));
  }
  return result;
}

class MockProvider : public ContentSuggestionsProvider {
 public:
  MockProvider(Observer* observer,
               CategoryFactory* category_factory,
               const std::vector<Category>& provided_categories)
      : ContentSuggestionsProvider(observer, category_factory) {
    SetProvidedCategories(provided_categories);
  }

  void SetProvidedCategories(const std::vector<Category>& provided_categories) {
    statuses_.clear();
    provided_categories_ = provided_categories;
    for (Category category : provided_categories) {
      statuses_[category.id()] = CategoryStatus::AVAILABLE;
    }
  }

  std::vector<Category> GetProvidedCategories() override {
    return provided_categories_;
  }

  CategoryStatus GetCategoryStatus(Category category) {
    return statuses_[category.id()];
  }

  CategoryInfo GetCategoryInfo(Category category) {
    return CategoryInfo(base::ASCIIToUTF16("Section title"),
                        ContentSuggestionsCardLayout::FULL_CARD, true, true);
  }

  void FireSuggestionsChanged(Category category,
                              const std::vector<int>& numbers) {
    observer()->OnNewSuggestions(this, category, CreateSuggestions(numbers));
  }

  void FireCategoryStatusChanged(Category category, CategoryStatus new_status) {
    statuses_[category.id()] = new_status;
    observer()->OnCategoryStatusChanged(this, category, new_status);
  }

  void FireSuggestionInvalidated(Category category,
                                 const std::string& suggestion_id) {
    observer()->OnSuggestionInvalidated(this, category, suggestion_id);
  }

  MOCK_METHOD1(ClearCachedSuggestionsForDebugging, void(Category category));
  MOCK_METHOD1(GetDismissedSuggestionsForDebugging,
               std::vector<ContentSuggestion>(Category category));
  MOCK_METHOD1(ClearDismissedSuggestionsForDebugging, void(Category category));
  MOCK_METHOD1(DismissSuggestion, void(const std::string& suggestion_id));
  MOCK_METHOD2(FetchSuggestionImage,
               void(const std::string& suggestion_id,
                    const ImageFetchedCallback& callback));

 private:
  std::vector<Category> provided_categories_;
  std::map<int, CategoryStatus> statuses_;
};

class MockServiceObserver : public ContentSuggestionsService::Observer {
 public:
  MOCK_METHOD1(OnNewSuggestions, void(Category category));
  MOCK_METHOD2(OnCategoryStatusChanged,
               void(Category changed_category, CategoryStatus new_status));
  MOCK_METHOD2(OnSuggestionInvalidated,
               void(Category category, const std::string& suggestion_id));
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
  void ExpectThatSuggestionsAre(Category category, std::vector<int> numbers) {
    std::vector<Category> categories = service()->GetCategories();
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

  const std::map<Category, ContentSuggestionsProvider*, Category::CompareByID>&
  providers() {
    return service()->providers_by_category_;
  }

  CategoryFactory* category_factory() { return service()->category_factory(); }

  Category FromKnownCategory(KnownCategories known_category) {
    return service()->category_factory()->FromKnownCategory(known_category);
  }

  MockProvider* RegisterProvider(Category provided_category) {
    return RegisterProvider(std::vector<Category>({provided_category}));
  }

  MockProvider* RegisterProvider(
      const std::vector<Category>& provided_categories) {
    std::unique_ptr<MockProvider> provider = base::MakeUnique<MockProvider>(
        service(), category_factory(), provided_categories);
    MockProvider* result = provider.get();
    service()->RegisterProvider(std::move(provider));
    return result;
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

TEST_F(ContentSuggestionsServiceTest, ShouldRegisterProviders) {
  EXPECT_THAT(service()->state(),
              Eq(ContentSuggestionsService::State::ENABLED));
  Category articles_category = FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      FromKnownCategory(KnownCategories::BOOKMARKS);
  ASSERT_THAT(providers(), IsEmpty());
  EXPECT_THAT(service()->GetCategories(), IsEmpty());
  EXPECT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(service()->GetCategoryStatus(offline_pages_category),
              Eq(CategoryStatus::NOT_PROVIDED));

  MockProvider* provider1 = RegisterProvider(articles_category);
  EXPECT_THAT(providers().count(offline_pages_category), Eq(0ul));
  EXPECT_THAT(providers().at(articles_category), Eq(provider1));
  EXPECT_THAT(providers().size(), Eq(1ul));
  EXPECT_THAT(service()->GetCategories(), ElementsAre(articles_category));
  EXPECT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(service()->GetCategoryStatus(offline_pages_category),
              Eq(CategoryStatus::NOT_PROVIDED));

  MockProvider* provider2 = RegisterProvider(offline_pages_category);
  EXPECT_THAT(providers().at(articles_category), Eq(provider1));
  EXPECT_THAT(providers().at(offline_pages_category), Eq(provider2));
  EXPECT_THAT(providers().size(), Eq(2ul));
  EXPECT_THAT(service()->GetCategories(),
              ElementsAre(offline_pages_category, articles_category));
  EXPECT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(service()->GetCategoryStatus(offline_pages_category),
              Eq(CategoryStatus::AVAILABLE));
}

TEST_F(ContentSuggestionsServiceDisabledTest, ShouldDoNothingWhenDisabled) {
  Category articles_category = FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      FromKnownCategory(KnownCategories::BOOKMARKS);
  EXPECT_THAT(service()->state(),
              Eq(ContentSuggestionsService::State::DISABLED));
  EXPECT_THAT(providers(), IsEmpty());
  EXPECT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED));
  EXPECT_THAT(service()->GetCategoryStatus(offline_pages_category),
              Eq(CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED));
  EXPECT_THAT(service()->GetCategories(), IsEmpty());
  EXPECT_THAT(service()->GetSuggestionsForCategory(articles_category),
              IsEmpty());
}

TEST_F(ContentSuggestionsServiceTest, ShouldRedirectFetchSuggestionImage) {
  Category articles_category = FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      FromKnownCategory(KnownCategories::BOOKMARKS);
  MockProvider* provider1 = RegisterProvider(articles_category);
  MockProvider* provider2 = RegisterProvider(offline_pages_category);

  provider1->FireSuggestionsChanged(articles_category, {1});
  std::string suggestion_id = CreateSuggestion(1).id();

  EXPECT_CALL(*provider1, FetchSuggestionImage(suggestion_id, _));
  EXPECT_CALL(*provider2, FetchSuggestionImage(_, _)).Times(0);
  service()->FetchSuggestionImage(
      suggestion_id, base::Bind(&ContentSuggestionsServiceTest::OnImageFetched,
                                base::Unretained(this)));
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldCallbackEmptyImageForUnavailableProvider) {
  // Setup the current thread's MessageLoop.
  base::MessageLoop message_loop;

  base::RunLoop run_loop;
  std::string suggestion_id = "TestID";
  EXPECT_CALL(*this, OnImageFetched(suggestion_id,
                                    Property(&gfx::Image::IsEmpty, Eq(true))))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service()->FetchSuggestionImage(
      suggestion_id, base::Bind(&ContentSuggestionsServiceTest::OnImageFetched,
                                base::Unretained(this)));
  run_loop.Run();
}

TEST_F(ContentSuggestionsServiceTest, ShouldRedirectDismissSuggestion) {
  Category articles_category = FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      FromKnownCategory(KnownCategories::BOOKMARKS);
  MockProvider* provider1 = RegisterProvider(articles_category);
  MockProvider* provider2 = RegisterProvider(offline_pages_category);

  provider2->FireSuggestionsChanged(offline_pages_category, {11});
  std::string suggestion_id = CreateSuggestion(11).id();

  EXPECT_CALL(*provider1, DismissSuggestion(_)).Times(0);
  EXPECT_CALL(*provider2, DismissSuggestion(suggestion_id));
  service()->DismissSuggestion(suggestion_id);
}

TEST_F(ContentSuggestionsServiceTest, ShouldRedirectSuggestionInvalidated) {
  Category articles_category = FromKnownCategory(KnownCategories::ARTICLES);

  MockProvider* provider = RegisterProvider(articles_category);
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  provider->FireSuggestionsChanged(articles_category, {11, 12, 13});
  ExpectThatSuggestionsAre(articles_category, {11, 12, 13});

  std::string suggestion_id = CreateSuggestion(12).id();
  EXPECT_CALL(observer,
              OnSuggestionInvalidated(articles_category, suggestion_id));
  provider->FireSuggestionInvalidated(articles_category, suggestion_id);
  ExpectThatSuggestionsAre(articles_category, {11, 13});
  Mock::VerifyAndClearExpectations(&observer);

  // Unknown IDs must be forwarded (though no change happens to the service's
  // internal data structures) because previously opened UIs, which can still
  // show the invalidated suggestion, must be notified.
  std::string unknown_id = CreateSuggestion(1234).id();
  EXPECT_CALL(observer, OnSuggestionInvalidated(articles_category, unknown_id));
  provider->FireSuggestionInvalidated(articles_category, unknown_id);
  ExpectThatSuggestionsAre(articles_category, {11, 13});
  Mock::VerifyAndClearExpectations(&observer);

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest, ShouldForwardSuggestions) {
  Category articles_category = FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      FromKnownCategory(KnownCategories::BOOKMARKS);

  // Create and register providers
  MockProvider* provider1 = RegisterProvider(articles_category);
  MockProvider* provider2 = RegisterProvider(offline_pages_category);
  EXPECT_THAT(providers().at(articles_category), Eq(provider1));
  EXPECT_THAT(providers().at(offline_pages_category), Eq(provider2));

  // Create and register observer
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // Send suggestions 1 and 2
  EXPECT_CALL(observer, OnNewSuggestions(articles_category));
  provider1->FireSuggestionsChanged(articles_category, {1, 2});
  ExpectThatSuggestionsAre(articles_category, {1, 2});
  Mock::VerifyAndClearExpectations(&observer);

  // Send them again, make sure they're not reported twice
  EXPECT_CALL(observer, OnNewSuggestions(articles_category));
  provider1->FireSuggestionsChanged(articles_category, {1, 2});
  ExpectThatSuggestionsAre(articles_category, {1, 2});
  ExpectThatSuggestionsAre(offline_pages_category, std::vector<int>());
  Mock::VerifyAndClearExpectations(&observer);

  // Send suggestions 13 and 14
  EXPECT_CALL(observer, OnNewSuggestions(offline_pages_category));
  provider2->FireSuggestionsChanged(offline_pages_category, {13, 14});
  ExpectThatSuggestionsAre(articles_category, {1, 2});
  ExpectThatSuggestionsAre(offline_pages_category, {13, 14});
  Mock::VerifyAndClearExpectations(&observer);

  // Send suggestion 1 only
  EXPECT_CALL(observer, OnNewSuggestions(articles_category));
  provider1->FireSuggestionsChanged(articles_category, {1});
  ExpectThatSuggestionsAre(articles_category, {1});
  ExpectThatSuggestionsAre(offline_pages_category, {13, 14});
  Mock::VerifyAndClearExpectations(&observer);

  // provider2 reports BOOKMARKS as unavailable
  EXPECT_CALL(observer, OnCategoryStatusChanged(
                            offline_pages_category,
                            CategoryStatus::CATEGORY_EXPLICITLY_DISABLED));
  provider2->FireCategoryStatusChanged(
      offline_pages_category, CategoryStatus::CATEGORY_EXPLICITLY_DISABLED);
  EXPECT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(service()->GetCategoryStatus(offline_pages_category),
              Eq(CategoryStatus::CATEGORY_EXPLICITLY_DISABLED));
  ExpectThatSuggestionsAre(articles_category, {1});
  ExpectThatSuggestionsAre(offline_pages_category, std::vector<int>());
  Mock::VerifyAndClearExpectations(&observer);

  // Shutdown the service
  EXPECT_CALL(observer, ContentSuggestionsServiceShutdown());
  service()->Shutdown();
  service()->RemoveObserver(&observer);
  // The service will receive two Shutdown() calls.
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldNotReturnCategoryInfoForNonexistentCategory) {
  Category category = FromKnownCategory(KnownCategories::BOOKMARKS);
  base::Optional<CategoryInfo> result = service()->GetCategoryInfo(category);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContentSuggestionsServiceTest, ShouldReturnCategoryInfo) {
  Category category = FromKnownCategory(KnownCategories::BOOKMARKS);
  MockProvider* provider = RegisterProvider(category);
  base::Optional<CategoryInfo> result = service()->GetCategoryInfo(category);
  ASSERT_TRUE(result.has_value());
  CategoryInfo expected = provider->GetCategoryInfo(category);
  const CategoryInfo& actual = result.value();
  EXPECT_THAT(expected.title(), Eq(actual.title()));
  EXPECT_THAT(expected.card_layout(), Eq(actual.card_layout()));
  EXPECT_THAT(expected.has_more_button(), Eq(actual.has_more_button()));
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldRegisterNewCategoryOnNewSuggestions) {
  Category category = FromKnownCategory(KnownCategories::BOOKMARKS);
  MockProvider* provider = RegisterProvider(category);
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // Provider starts providing |new_category| without calling
  // |OnCategoryStatusChanged|. This is supported for now until further
  // reconsideration.
  Category new_category = FromKnownCategory(KnownCategories::ARTICLES);
  provider->SetProvidedCategories(
      std::vector<Category>({category, new_category}));

  EXPECT_CALL(observer, OnNewSuggestions(new_category));
  EXPECT_CALL(observer,
              OnCategoryStatusChanged(new_category, CategoryStatus::AVAILABLE));
  provider->FireSuggestionsChanged(new_category, {1, 2});

  ExpectThatSuggestionsAre(new_category, {1, 2});
  EXPECT_THAT(providers().at(category), Eq(provider));
  EXPECT_THAT(service()->GetCategoryStatus(category),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(providers().at(new_category), Eq(provider));
  EXPECT_THAT(service()->GetCategoryStatus(new_category),
              Eq(CategoryStatus::AVAILABLE));

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldRegisterNewCategoryOnCategoryStatusChanged) {
  Category category = FromKnownCategory(KnownCategories::BOOKMARKS);
  MockProvider* provider = RegisterProvider(category);
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // Provider starts providing |new_category| and calls
  // |OnCategoryStatusChanged|, but the category is not yet available.
  Category new_category = FromKnownCategory(KnownCategories::ARTICLES);
  provider->SetProvidedCategories(
      std::vector<Category>({category, new_category}));
  EXPECT_CALL(observer, OnCategoryStatusChanged(new_category,
                                                CategoryStatus::INITIALIZING));
  provider->FireCategoryStatusChanged(new_category,
                                      CategoryStatus::INITIALIZING);

  EXPECT_THAT(providers().at(new_category), Eq(provider));
  ExpectThatSuggestionsAre(new_category, std::vector<int>());
  EXPECT_THAT(service()->GetCategoryStatus(new_category),
              Eq(CategoryStatus::INITIALIZING));
  EXPECT_THAT(service()->GetCategories(),
              Eq(std::vector<Category>({category, new_category})));

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest, ShouldRemoveCategoryWhenNotProvided) {
  Category category = FromKnownCategory(KnownCategories::BOOKMARKS);
  MockProvider* provider = RegisterProvider(category);
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  provider->FireSuggestionsChanged(category, {1, 2});
  ExpectThatSuggestionsAre(category, {1, 2});

  EXPECT_CALL(observer,
              OnCategoryStatusChanged(category, CategoryStatus::NOT_PROVIDED));
  provider->FireCategoryStatusChanged(category, CategoryStatus::NOT_PROVIDED);

  EXPECT_THAT(service()->GetCategoryStatus(category),
              Eq(CategoryStatus::NOT_PROVIDED));
  EXPECT_TRUE(service()->GetCategories().empty());
  ExpectThatSuggestionsAre(category, std::vector<int>());

  service()->RemoveObserver(&observer);
}

}  // namespace ntp_snippets
