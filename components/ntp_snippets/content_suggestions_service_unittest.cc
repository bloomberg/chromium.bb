// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/time/default_clock.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/ntp_snippets/category_rankers/fake_category_ranker.h"
#include "components/ntp_snippets/category_rankers/mock_category_ranker.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/logger.h"
#include "components/ntp_snippets/mock_content_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::Mock;
using testing::Property;
using testing::Return;
using testing::SizeIs;
using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace ntp_snippets {

namespace {

class MockServiceObserver : public ContentSuggestionsService::Observer {
 public:
  MockServiceObserver() = default;
  ~MockServiceObserver() override = default;

  MOCK_METHOD1(OnNewSuggestions, void(Category category));
  MOCK_METHOD2(OnCategoryStatusChanged,
               void(Category changed_category, CategoryStatus new_status));
  MOCK_METHOD1(OnSuggestionInvalidated,
               void(const ContentSuggestion::ID& suggestion_id));
  MOCK_METHOD0(OnFullRefreshRequired, void());
  MOCK_METHOD0(ContentSuggestionsServiceShutdown, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockServiceObserver);
};

}  // namespace

class ContentSuggestionsServiceTest : public testing::Test {
 public:
  ContentSuggestionsServiceTest()
      : pref_service_(base::MakeUnique<TestingPrefServiceSimple>()),
        category_ranker_(base::MakeUnique<ConstantCategoryRanker>()) {}

  void SetUp() override {
    RegisterPrefs();
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
      std::string id_within_category = suggestion.id().id_within_category();
      int id;
      ASSERT_TRUE(base::StringToInt(id_within_category, &id));
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

  const std::map<Category, ContentSuggestionsProvider*, Category::CompareByID>&
  dismissed_providers() {
    return service()->dismissed_providers_by_category_;
  }

  MockContentSuggestionsProvider* MakeRegisteredMockProvider(
      Category provided_category) {
    return MakeRegisteredMockProvider(
        std::vector<Category>({provided_category}));
  }

  MockContentSuggestionsProvider* MakeRegisteredMockProvider(
      const std::vector<Category>& provided_categories) {
    auto provider =
        base::MakeUnique<testing::StrictMock<MockContentSuggestionsProvider>>(
            service(), provided_categories);
    MockContentSuggestionsProvider* result = provider.get();
    service()->RegisterProvider(std::move(provider));
    return result;
  }

  void SetCategoryRanker(std::unique_ptr<CategoryRanker> category_ranker) {
    category_ranker_ = std::move(category_ranker);
  }

  MOCK_METHOD1(OnImageFetched, void(const gfx::Image&));

 protected:
  void RegisterPrefs() {
    ContentSuggestionsService::RegisterProfilePrefs(pref_service_->registry());
    RemoteSuggestionsProviderImpl::RegisterProfilePrefs(
        pref_service_->registry());
    UserClassifier::RegisterProfilePrefs(pref_service_->registry());
  }

  void CreateContentSuggestionsService(
      ContentSuggestionsService::State enabled) {
    ASSERT_FALSE(service_);

    // TODO(jkrcal): Replace by a mock.
    auto user_classifier = base::MakeUnique<UserClassifier>(
        pref_service_.get(), base::MakeUnique<base::DefaultClock>());

    service_ = base::MakeUnique<ContentSuggestionsService>(
        enabled, /*signin_manager=*/nullptr, /*history_service=*/nullptr,
        /*large_icon_service=*/nullptr, pref_service_.get(),
        std::move(category_ranker_), std::move(user_classifier),
        /*scheduler=*/nullptr, /*debug_logger=*/base::MakeUnique<Logger>());
  }

  void ResetService() {
    service_->Shutdown();
    service_.reset();
    CreateContentSuggestionsService(ContentSuggestionsService::State::ENABLED);
  }

  ContentSuggestionsService* service() { return service_.get(); }

  // Returns a suggestion instance for testing.
  ContentSuggestion CreateSuggestion(Category category, int number) {
    return ContentSuggestion(
        category, base::IntToString(number),
        GURL("http://testsuggestion/" + base::IntToString(number)));
  }

  std::vector<ContentSuggestion> CreateSuggestions(
      Category category,
      const std::vector<int>& numbers) {
    std::vector<ContentSuggestion> result;
    for (int number : numbers) {
      result.push_back(CreateSuggestion(category, number));
    }
    return result;
  }

 private:
  std::unique_ptr<ContentSuggestionsService> service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<CategoryRanker> category_ranker_;

  DISALLOW_COPY_AND_ASSIGN(ContentSuggestionsServiceTest);
};

class ContentSuggestionsServiceDisabledTest
    : public ContentSuggestionsServiceTest {
 public:
  void SetUp() override {
    RegisterPrefs();
    CreateContentSuggestionsService(ContentSuggestionsService::State::DISABLED);
  }
};

TEST_F(ContentSuggestionsServiceTest, ShouldRegisterProviders) {
  EXPECT_THAT(service()->state(),
              Eq(ContentSuggestionsService::State::ENABLED));
  Category articles_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  ASSERT_THAT(providers(), IsEmpty());
  EXPECT_THAT(service()->GetCategories(), IsEmpty());
  EXPECT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(service()->GetCategoryStatus(offline_pages_category),
              Eq(CategoryStatus::NOT_PROVIDED));

  MockContentSuggestionsProvider* provider1 =
      MakeRegisteredMockProvider(articles_category);
  provider1->FireCategoryStatusChangedWithCurrentStatus(articles_category);
  EXPECT_THAT(providers().count(offline_pages_category), Eq(0ul));
  ASSERT_THAT(providers().count(articles_category), Eq(1ul));
  EXPECT_THAT(providers().at(articles_category), Eq(provider1));
  EXPECT_THAT(providers().size(), Eq(1ul));
  EXPECT_THAT(service()->GetCategories(),
              UnorderedElementsAre(articles_category));
  EXPECT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(service()->GetCategoryStatus(offline_pages_category),
              Eq(CategoryStatus::NOT_PROVIDED));

  MockContentSuggestionsProvider* provider2 =
      MakeRegisteredMockProvider(offline_pages_category);
  provider2->FireCategoryStatusChangedWithCurrentStatus(offline_pages_category);
  ASSERT_THAT(providers().count(offline_pages_category), Eq(1ul));
  EXPECT_THAT(providers().at(articles_category), Eq(provider1));
  ASSERT_THAT(providers().count(articles_category), Eq(1ul));
  EXPECT_THAT(providers().at(offline_pages_category), Eq(provider2));
  EXPECT_THAT(providers().size(), Eq(2ul));
  EXPECT_THAT(service()->GetCategories(),
              UnorderedElementsAre(offline_pages_category, articles_category));
  EXPECT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(service()->GetCategoryStatus(offline_pages_category),
              Eq(CategoryStatus::AVAILABLE));
}

TEST_F(ContentSuggestionsServiceDisabledTest, ShouldDoNothingWhenDisabled) {
  Category articles_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      Category::FromKnownCategory(KnownCategories::DOWNLOADS);
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
  Category articles_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* provider1 =
      MakeRegisteredMockProvider(articles_category);
  MockContentSuggestionsProvider* provider2 =
      MakeRegisteredMockProvider(offline_pages_category);

  provider1->FireSuggestionsChanged(articles_category,
                                    CreateSuggestions(articles_category, {1}));
  ContentSuggestion::ID suggestion_id(articles_category, "1");

  EXPECT_CALL(*provider1, FetchSuggestionImageMock(suggestion_id, _));
  EXPECT_CALL(*provider2, FetchSuggestionImageMock(_, _)).Times(0);
  service()->FetchSuggestionImage(
      suggestion_id, base::Bind(&ContentSuggestionsServiceTest::OnImageFetched,
                                base::Unretained(this)));
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldCallbackEmptyImageForUnavailableProvider) {
  // Setup the current thread's MessageLoop.
  base::MessageLoop message_loop;

  base::RunLoop run_loop;
  // Assuming there will never be a category with the id below.
  ContentSuggestion::ID suggestion_id(Category::FromIDValue(21563), "TestID");
  EXPECT_CALL(*this, OnImageFetched(Property(&gfx::Image::IsEmpty, Eq(true))))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service()->FetchSuggestionImage(
      suggestion_id, base::Bind(&ContentSuggestionsServiceTest::OnImageFetched,
                                base::Unretained(this)));
  run_loop.Run();
}

TEST_F(ContentSuggestionsServiceTest, ShouldRedirectDismissSuggestion) {
  Category articles_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* provider1 =
      MakeRegisteredMockProvider(articles_category);
  MockContentSuggestionsProvider* provider2 =
      MakeRegisteredMockProvider(offline_pages_category);

  provider2->FireSuggestionsChanged(
      offline_pages_category, CreateSuggestions(offline_pages_category, {11}));
  ContentSuggestion::ID suggestion_id(offline_pages_category, "11");

  EXPECT_CALL(*provider1, DismissSuggestion(_)).Times(0);
  EXPECT_CALL(*provider2, DismissSuggestion(suggestion_id));
  service()->DismissSuggestion(suggestion_id);
}

TEST_F(ContentSuggestionsServiceTest, ShouldRedirectSuggestionInvalidated) {
  Category articles_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);

  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(articles_category);
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  provider->FireSuggestionsChanged(
      articles_category, CreateSuggestions(articles_category, {11, 12, 13}));
  ExpectThatSuggestionsAre(articles_category, {11, 12, 13});

  ContentSuggestion::ID suggestion_id(articles_category, "12");
  EXPECT_CALL(observer, OnSuggestionInvalidated(suggestion_id));
  provider->FireSuggestionInvalidated(suggestion_id);
  ExpectThatSuggestionsAre(articles_category, {11, 13});
  Mock::VerifyAndClearExpectations(&observer);

  // Unknown IDs must be forwarded (though no change happens to the service's
  // internal data structures) because previously opened UIs, which can still
  // show the invalidated suggestion, must be notified.
  ContentSuggestion::ID unknown_id(articles_category, "1234");
  EXPECT_CALL(observer, OnSuggestionInvalidated(unknown_id));
  provider->FireSuggestionInvalidated(unknown_id);
  ExpectThatSuggestionsAre(articles_category, {11, 13});
  Mock::VerifyAndClearExpectations(&observer);

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest, ShouldForwardSuggestions) {
  Category articles_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  Category offline_pages_category =
      Category::FromKnownCategory(KnownCategories::DOWNLOADS);

  // Create and register providers
  MockContentSuggestionsProvider* provider1 =
      MakeRegisteredMockProvider(articles_category);
  provider1->FireCategoryStatusChangedWithCurrentStatus(articles_category);
  MockContentSuggestionsProvider* provider2 =
      MakeRegisteredMockProvider(offline_pages_category);
  provider2->FireCategoryStatusChangedWithCurrentStatus(offline_pages_category);
  ASSERT_THAT(providers().count(articles_category), Eq(1ul));
  EXPECT_THAT(providers().at(articles_category), Eq(provider1));
  ASSERT_THAT(providers().count(offline_pages_category), Eq(1ul));
  EXPECT_THAT(providers().at(offline_pages_category), Eq(provider2));

  // Create and register observer
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // Send suggestions 1 and 2
  EXPECT_CALL(observer, OnNewSuggestions(articles_category));
  provider1->FireSuggestionsChanged(
      articles_category, CreateSuggestions(articles_category, {1, 2}));
  ExpectThatSuggestionsAre(articles_category, {1, 2});
  Mock::VerifyAndClearExpectations(&observer);

  // Send them again, make sure they're not reported twice
  EXPECT_CALL(observer, OnNewSuggestions(articles_category));
  provider1->FireSuggestionsChanged(
      articles_category, CreateSuggestions(articles_category, {1, 2}));
  ExpectThatSuggestionsAre(articles_category, {1, 2});
  ExpectThatSuggestionsAre(offline_pages_category, std::vector<int>());
  Mock::VerifyAndClearExpectations(&observer);

  // Send suggestions 13 and 14
  EXPECT_CALL(observer, OnNewSuggestions(offline_pages_category));
  provider2->FireSuggestionsChanged(
      offline_pages_category, CreateSuggestions(articles_category, {13, 14}));
  ExpectThatSuggestionsAre(articles_category, {1, 2});
  ExpectThatSuggestionsAre(offline_pages_category, {13, 14});
  Mock::VerifyAndClearExpectations(&observer);

  // Send suggestion 1 only
  EXPECT_CALL(observer, OnNewSuggestions(articles_category));
  provider1->FireSuggestionsChanged(articles_category,
                                    CreateSuggestions(articles_category, {1}));
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
  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  base::Optional<CategoryInfo> result = service()->GetCategoryInfo(category);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContentSuggestionsServiceTest, ShouldReturnCategoryInfo) {
  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  provider->FireCategoryStatusChangedWithCurrentStatus(category);
  base::Optional<CategoryInfo> result = service()->GetCategoryInfo(category);
  ASSERT_TRUE(result.has_value());
  CategoryInfo expected = provider->GetCategoryInfo(category);
  const CategoryInfo& actual = result.value();
  EXPECT_THAT(expected.title(), Eq(actual.title()));
  EXPECT_THAT(expected.card_layout(), Eq(actual.card_layout()));
  EXPECT_THAT(expected.additional_action(), Eq(actual.additional_action()));
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldRegisterNewCategoryOnNewSuggestions) {
  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  provider->FireCategoryStatusChangedWithCurrentStatus(category);
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // Provider starts providing |new_category| without calling
  // |OnCategoryStatusChanged|. This is supported for now until further
  // reconsideration.
  Category new_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  provider->SetProvidedCategories(
      std::vector<Category>({category, new_category}));

  EXPECT_CALL(observer, OnNewSuggestions(new_category));
  EXPECT_CALL(observer,
              OnCategoryStatusChanged(new_category, CategoryStatus::AVAILABLE));
  provider->FireSuggestionsChanged(new_category,
                                   CreateSuggestions(new_category, {1, 2}));

  ExpectThatSuggestionsAre(new_category, {1, 2});
  ASSERT_THAT(providers().count(category), Eq(1ul));
  EXPECT_THAT(providers().at(category), Eq(provider));
  EXPECT_THAT(service()->GetCategoryStatus(category),
              Eq(CategoryStatus::AVAILABLE));
  ASSERT_THAT(providers().count(new_category), Eq(1ul));
  EXPECT_THAT(providers().at(new_category), Eq(provider));
  EXPECT_THAT(service()->GetCategoryStatus(new_category),
              Eq(CategoryStatus::AVAILABLE));

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldRegisterNewCategoryOnCategoryStatusChanged) {
  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  provider->FireCategoryStatusChangedWithCurrentStatus(category);
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // Provider starts providing |new_category| and calls
  // |OnCategoryStatusChanged|, but the category is not yet available.
  Category new_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  provider->SetProvidedCategories(
      std::vector<Category>({category, new_category}));
  EXPECT_CALL(observer, OnCategoryStatusChanged(new_category,
                                                CategoryStatus::INITIALIZING));
  provider->FireCategoryStatusChanged(new_category,
                                      CategoryStatus::INITIALIZING);

  ASSERT_THAT(providers().count(new_category), Eq(1ul));
  EXPECT_THAT(providers().at(new_category), Eq(provider));
  ExpectThatSuggestionsAre(new_category, std::vector<int>());
  EXPECT_THAT(service()->GetCategoryStatus(new_category),
              Eq(CategoryStatus::INITIALIZING));
  EXPECT_THAT(service()->GetCategories(),
              UnorderedElementsAre(category, new_category));

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest, ShouldRemoveCategoryWhenNotProvided) {
  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  MockServiceObserver observer;
  service()->AddObserver(&observer);

  provider->FireSuggestionsChanged(category,
                                   CreateSuggestions(category, {1, 2}));
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

TEST_F(ContentSuggestionsServiceTest, ShouldForwardClearHistoryToProviders) {
  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  base::Time begin = base::Time::FromTimeT(123);
  base::Time end = base::Time::FromTimeT(456);
  EXPECT_CALL(*provider, ClearHistory(begin, end, _));
  base::Callback<bool(const GURL& url)> filter;
  service()->ClearHistory(begin, end, filter);
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldForwardClearHistoryToCategoryRanker) {
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));

  // The service is recreated to pick up the new ranker.
  ResetService();

  base::Time begin = base::Time::FromTimeT(123),
             end = base::Time::FromTimeT(456);
  EXPECT_CALL(*raw_mock_ranker, ClearHistory(begin, end));
  base::Callback<bool(const GURL& url)> filter;
  service()->ClearHistory(begin, end, filter);
}

TEST_F(ContentSuggestionsServiceTest, ShouldForwardDeleteAllHistoryURLs) {
  StrictMock<MockServiceObserver> observer;
  service()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFullRefreshRequired());

  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  EXPECT_CALL(*provider, ClearHistory(base::Time(), base::Time::Max(), _));
  static_cast<history::HistoryServiceObserver*>(service())->OnURLsDeleted(
      /*history_service=*/nullptr, /*all_history=*/true, /*expired=*/false,
      history::URLRows(), /*favicon_urls=*/std::set<GURL>());

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest, ShouldIgnoreExpiredURLDeletions) {
  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MakeRegisteredMockProvider(category);
  // Create an observer to make sure OnFullRefresh() does not get triggered.
  StrictMock<MockServiceObserver> observer;
  service()->AddObserver(&observer);

  static_cast<history::HistoryServiceObserver*>(service())->OnURLsDeleted(
      /*history_service=*/nullptr, /*all_history=*/true,
      /*expired=*/true, history::URLRows(),
      /*favicon_urls=*/std::set<GURL>());
  static_cast<history::HistoryServiceObserver*>(service())->OnURLsDeleted(
      /*history_service=*/nullptr, /*all_history=*/false,
      /*expired=*/true,
      history::URLRows({history::URLRow(GURL("http://google.com")),
                        history::URLRow(GURL("http://maps.google.com"))}),
      /*favicon_urls=*/std::set<GURL>());

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest, ShouldIgnoreEmptyURLDeletions) {
  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MakeRegisteredMockProvider(category);
  static_cast<history::HistoryServiceObserver*>(service())->OnURLsDeleted(
      /*history_service=*/nullptr, /*all_history=*/false,
      /*expired=*/false, history::URLRows(),
      /*favicon_urls=*/std::set<GURL>());
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldNotClearHistoryOnSingleURLDeletion) {
  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MakeRegisteredMockProvider(category);
  // Single URLs should not trigger
  static_cast<history::HistoryServiceObserver*>(service())->OnURLsDeleted(
      /*history_service=*/nullptr, /*all_history=*/false,
      /*expired=*/false,
      history::URLRows({history::URLRow(GURL("http://google.com"))}),
      /*favicon_urls=*/std::set<GURL>());
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldClearHistoryForMultipleURLDeletion) {
  StrictMock<MockServiceObserver> observer;
  service()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFullRefreshRequired());

  Category category = Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  EXPECT_CALL(*provider, ClearHistory(base::Time(), base::Time::Max(), _));

  static_cast<history::HistoryServiceObserver*>(service())->OnURLsDeleted(
      /*history_service=*/nullptr, /*all_history=*/false,
      /*expired=*/false,
      history::URLRows({history::URLRow(GURL("http://google.com")),
                        history::URLRow(GURL("http://youtube.com"))}),
      /*favicon_urls=*/std::set<GURL>());

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest, ShouldForwardFetch) {
  Category category = Category::FromKnownCategory(KnownCategories::ARTICLES);
  std::set<std::string> known_suggestions;
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  provider->FireCategoryStatusChangedWithCurrentStatus(category);
  EXPECT_CALL(*provider, FetchMock(category, known_suggestions, _));
  service()->Fetch(category, known_suggestions, FetchDoneCallback());
}

TEST_F(ContentSuggestionsServiceTest, DismissAndRestoreCategory) {
  // Register a category with one suggestion.
  Category category = Category::FromKnownCategory(KnownCategories::ARTICLES);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  provider->FireCategoryStatusChangedWithCurrentStatus(category);
  provider->FireSuggestionsChanged(category, CreateSuggestions(category, {42}));

  EXPECT_THAT(service()->GetCategories(), UnorderedElementsAre(category));
  EXPECT_THAT(service()->GetCategoryStatus(category),
              Eq(CategoryStatus::AVAILABLE));
  ExpectThatSuggestionsAre(category, {42});
  EXPECT_THAT(providers().count(category), Eq(1ul));
  EXPECT_THAT(dismissed_providers(), IsEmpty());

  // Dismissing the category clears the suggestions for it.
  service()->DismissCategory(category);

  EXPECT_THAT(service()->GetCategories(), IsEmpty());
  EXPECT_THAT(service()->GetCategoryStatus(category),
              Eq(CategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(service()->GetSuggestionsForCategory(category), IsEmpty());
  EXPECT_THAT(providers(), IsEmpty());
  EXPECT_THAT(dismissed_providers().count(category), Eq(1ul));

  // Restoring the dismissed category makes it available again but it is still
  // empty.
  service()->RestoreDismissedCategories();

  EXPECT_THAT(service()->GetCategories(), UnorderedElementsAre(category));
  EXPECT_THAT(service()->GetCategoryStatus(category),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(service()->GetSuggestionsForCategory(category), IsEmpty());
  EXPECT_THAT(providers().count(category), Eq(1ul));
  EXPECT_THAT(dismissed_providers(), IsEmpty());
}

TEST_F(ContentSuggestionsServiceTest, ShouldRestoreDismissedCategories) {
  // Create and register provider.
  Category category1 = Category::FromIDValue(1);
  Category category2 = Category::FromIDValue(2);

  // Setup and verify initial state.
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider({category1, category2});
  provider->FireCategoryStatusChangedWithCurrentStatus(category1);
  provider->FireCategoryStatusChangedWithCurrentStatus(category2);

  ASSERT_THAT(service()->GetCategoryStatus(category1),
              Eq(CategoryStatus::AVAILABLE));
  ASSERT_THAT(service()->GetCategoryStatus(category2),
              Eq(CategoryStatus::AVAILABLE));

  // Dismiss all the categories. None should be provided now.
  service()->DismissCategory(category1);
  service()->DismissCategory(category2);

  ASSERT_THAT(service()->GetCategoryStatus(category1),
              Eq(CategoryStatus::NOT_PROVIDED));
  ASSERT_THAT(service()->GetCategoryStatus(category2),
              Eq(CategoryStatus::NOT_PROVIDED));

  // Receiving a status change notification should not change anything.
  provider->FireCategoryStatusChanged(category1, CategoryStatus::AVAILABLE);

  EXPECT_THAT(service()->GetCategoryStatus(category1),
              Eq(CategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(service()->GetCategoryStatus(category2),
              Eq(CategoryStatus::NOT_PROVIDED));

  // Receiving a notification without suggestions should not change anything.
  provider->FireSuggestionsChanged(category1, std::vector<ContentSuggestion>());

  EXPECT_THAT(service()->GetCategoryStatus(category1),
              Eq(CategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(service()->GetCategoryStatus(category2),
              Eq(CategoryStatus::NOT_PROVIDED));

  // Receiving suggestions should make the notified category available.
  provider->FireSuggestionsChanged(category1,
                                   CreateSuggestions(category1, {1, 2}));

  EXPECT_THAT(service()->GetCategoryStatus(category1),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(service()->GetCategoryStatus(category2),
              Eq(CategoryStatus::NOT_PROVIDED));
}

TEST_F(ContentSuggestionsServiceTest, ShouldRestoreDismissalsFromPrefs) {
  // Register a category with one suggestion.
  Category category = Category::FromKnownCategory(KnownCategories::ARTICLES);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  provider->FireCategoryStatusChangedWithCurrentStatus(category);

  // For a regular initialisation, the category is not dismissed.
  ASSERT_FALSE(service()->IsCategoryDismissed(category));

  // Dismiss the category.
  service()->DismissCategory(category);
  ASSERT_TRUE(service()->IsCategoryDismissed(category));

  // Simulate a Chrome restart. The category should still be dismissed.
  ResetService();
  EXPECT_TRUE(service()->IsCategoryDismissed(category));

  // Ensure that the provider registered at initialisation is used after
  // restoration.
  provider = MakeRegisteredMockProvider(category);
  provider->FireCategoryStatusChangedWithCurrentStatus(category);
  EXPECT_TRUE(service()->IsCategoryDismissed(category));

  service()->RestoreDismissedCategories();
  EXPECT_FALSE(service()->IsCategoryDismissed(category));
  EXPECT_THAT(providers().find(category)->second, Eq(provider));
}

TEST_F(ContentSuggestionsServiceTest, ShouldReturnCategoriesInOrderToDisplay) {
  const Category first_category = Category::FromRemoteCategory(1);
  const Category second_category = Category::FromRemoteCategory(2);

  auto fake_ranker = base::MakeUnique<FakeCategoryRanker>();
  FakeCategoryRanker* raw_fake_ranker = fake_ranker.get();
  SetCategoryRanker(std::move(fake_ranker));

  raw_fake_ranker->SetOrder({first_category, second_category});

  // The service is recreated to pick up the new ranker.
  ResetService();

  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider({first_category, second_category});
  provider->FireCategoryStatusChangedWithCurrentStatus(first_category);
  provider->FireCategoryStatusChangedWithCurrentStatus(second_category);

  EXPECT_THAT(service()->GetCategories(),
              ElementsAre(first_category, second_category));

  // The order to display (in the ranker) changes.
  raw_fake_ranker->SetOrder({second_category, first_category});

  // Categories order should reflect the new order.
  EXPECT_THAT(service()->GetCategories(),
              ElementsAre(second_category, first_category));
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldForwardDismissedCategoryToCategoryRanker) {
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));

  // The service is recreated to pick up the new ranker.
  ResetService();

  Category category = Category::FromKnownCategory(KnownCategories::BOOKMARKS);
  MockContentSuggestionsProvider* provider =
      MakeRegisteredMockProvider(category);
  provider->FireCategoryStatusChangedWithCurrentStatus(category);

  EXPECT_CALL(*raw_mock_ranker, OnCategoryDismissed(category));
  service()->DismissCategory(category);
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldDestroyBookmarksProviderWhenChromeHomeIsEnabled) {
  // Create and register the provider.
  Category bookmarks_category =
      Category::FromKnownCategory(KnownCategories::BOOKMARKS);
  MockContentSuggestionsProvider* bookmarks_provider =
      MakeRegisteredMockProvider(bookmarks_category);
  bookmarks_provider->FireSuggestionsChanged(
      bookmarks_category, CreateSuggestions(bookmarks_category, {1}));
  bookmarks_provider->FireCategoryStatusChangedWithCurrentStatus(
      bookmarks_category);
  ASSERT_THAT(service()->GetCategories(),
              UnorderedElementsAre(bookmarks_category));
  ASSERT_THAT(service()->GetCategoryStatus(bookmarks_category),
              Eq(CategoryStatus::AVAILABLE));
  ASSERT_THAT(service()->GetSuggestionsForCategory(bookmarks_category),
              SizeIs(1));

  // Set destructor callback to expect the destruction.
  base::MockCallback<MockContentSuggestionsProvider::DestructorCallback>
      mock_destructor_callback;
  bookmarks_provider->SetDestructorCallback(mock_destructor_callback.Get());

  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // The provider must be destructed and the category status must change.
  EXPECT_CALL(mock_destructor_callback, Run());
  EXPECT_CALL(observer, OnCategoryStatusChanged(bookmarks_category,
                                                CategoryStatus::NOT_PROVIDED));
  service()->OnChromeHomeStatusChanged(/*is_chrome_home_enabled=*/true);

  EXPECT_THAT(service()->GetCategories(), IsEmpty());
  EXPECT_THAT(service()->GetCategoryStatus(bookmarks_category),
              Eq(CategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(service()->GetSuggestionsForCategory(bookmarks_category),
              IsEmpty());

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldDestroyDownloadsProviderWhenChromeHomeIsEnabled) {
  // Create and register the provider.
  Category downloads_category =
      Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  MockContentSuggestionsProvider* downloads_provider =
      MakeRegisteredMockProvider(downloads_category);
  downloads_provider->FireSuggestionsChanged(
      downloads_category, CreateSuggestions(downloads_category, {1}));
  downloads_provider->FireCategoryStatusChangedWithCurrentStatus(
      downloads_category);
  ASSERT_THAT(service()->GetCategories(),
              UnorderedElementsAre(downloads_category));
  ASSERT_THAT(service()->GetCategoryStatus(downloads_category),
              Eq(CategoryStatus::AVAILABLE));
  ASSERT_THAT(service()->GetSuggestionsForCategory(downloads_category),
              SizeIs(1));

  // Set destructor callback to expect the destruction.
  base::MockCallback<MockContentSuggestionsProvider::DestructorCallback>
      mock_destructor_callback;
  downloads_provider->SetDestructorCallback(mock_destructor_callback.Get());

  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // The provider must be destructed and the category status must change.
  EXPECT_CALL(mock_destructor_callback, Run());
  EXPECT_CALL(observer, OnCategoryStatusChanged(downloads_category,
                                                CategoryStatus::NOT_PROVIDED));
  service()->OnChromeHomeStatusChanged(/*is_chrome_home_enabled=*/true);

  EXPECT_THAT(service()->GetCategories(), IsEmpty());
  EXPECT_THAT(service()->GetCategoryStatus(downloads_category),
              Eq(CategoryStatus::NOT_PROVIDED));
  EXPECT_THAT(service()->GetSuggestionsForCategory(downloads_category),
              IsEmpty());

  service()->RemoveObserver(&observer);
}

TEST_F(ContentSuggestionsServiceTest,
       ShouldNotDestroyArticlesProviderWhenChromeHomeIsEnabled) {
  // Create and register the provider.
  Category articles_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  MockContentSuggestionsProvider* articles_provider =
      MakeRegisteredMockProvider(articles_category);
  articles_provider->FireSuggestionsChanged(
      articles_category, CreateSuggestions(articles_category, {1}));
  articles_provider->FireCategoryStatusChangedWithCurrentStatus(
      articles_category);
  ASSERT_THAT(service()->GetCategories(),
              UnorderedElementsAre(articles_category));
  ASSERT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::AVAILABLE));
  ASSERT_THAT(service()->GetSuggestionsForCategory(articles_category),
              SizeIs(1));

  // Set destructor callback to ensure no destruction.
  base::MockCallback<MockContentSuggestionsProvider::DestructorCallback>
      mock_destructor_callback;
  articles_provider->SetDestructorCallback(mock_destructor_callback.Get());

  MockServiceObserver observer;
  service()->AddObserver(&observer);

  // The provider must not be destructed and the category status must not
  // change, because Articles are enabled in Chrome Home.
  EXPECT_CALL(mock_destructor_callback, Run()).Times(0);
  EXPECT_CALL(observer, OnCategoryStatusChanged(articles_category, _)).Times(0);
  service()->OnChromeHomeStatusChanged(/*is_chrome_home_enabled=*/true);

  EXPECT_THAT(service()->GetCategories(),
              UnorderedElementsAre(articles_category));
  EXPECT_THAT(service()->GetCategoryStatus(articles_category),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(service()->GetSuggestionsForCategory(articles_category),
              SizeIs(1));

  articles_provider->SetDestructorCallback(
      MockContentSuggestionsProvider::DestructorCallback());
  service()->RemoveObserver(&observer);
}

}  // namespace ntp_snippets
