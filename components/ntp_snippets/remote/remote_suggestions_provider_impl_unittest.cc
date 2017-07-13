// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_delegate.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/ntp_snippets/category_rankers/mock_category_ranker.h"
#include "components/ntp_snippets/fake_content_suggestions_provider_observer.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/json_to_categories.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/variations/variations_params_manager.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using image_fetcher::ImageFetcher;
using image_fetcher::ImageFetcherDelegate;
using testing::_;
using testing::Contains;
using testing::CreateFunctor;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::InSequence;
using testing::Invoke;
using testing::IsEmpty;
using testing::Mock;
using testing::MockFunction;
using testing::NiceMock;
using testing::Not;
using testing::Property;
using testing::Return;
using testing::SaveArg;
using testing::SizeIs;
using testing::StartsWith;
using testing::StrictMock;
using testing::WithArgs;

namespace ntp_snippets {

namespace {

ACTION_P(MoveFirstArgumentPointeeTo, ptr) {
  // 0-based indexation.
  *ptr = std::move(*arg0);
}

ACTION_P(MoveSecondArgumentPointeeTo, ptr) {
  // 0-based indexation.
  *ptr = std::move(*arg1);
}

const int kMaxExcludedDismissedIds = 100;

const base::Time::Exploded kDefaultCreationTime = {2015, 11, 4, 25, 13, 46, 45};

const char kSuggestionUrl[] = "http://localhost/foobar";
const char kSuggestionTitle[] = "Title";
const char kSuggestionText[] = "Suggestion";
const char kSuggestionPublisherName[] = "Foo News";

const char kSuggestionUrl2[] = "http://foo.com/bar";

const char kTestJsonDefaultCategoryTitle[] = "Some title";

const int kUnknownRemoteCategoryId = 1234;

// Different from default values to confirm that variation param values are
// used.
const int kMaxAdditionalPrefetchedSuggestions = 7;
const base::TimeDelta kMaxAgeForAdditionalPrefetchedSuggestion =
    base::TimeDelta::FromHours(48);

base::Time GetDefaultCreationTime() {
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kDefaultCreationTime, &out_time));
  return out_time;
}

base::Time GetDefaultExpirationTime() {
  return base::Time::Now() + base::TimeDelta::FromHours(1);
}

// TODO(vitaliii): Remove this and use RemoteSuggestionBuilder instead.
std::unique_ptr<RemoteSuggestion> CreateTestRemoteSuggestion(
    const std::string& url) {
  SnippetProto snippet_proto;
  snippet_proto.add_ids(url);
  snippet_proto.set_title("title");
  snippet_proto.set_snippet("snippet");
  snippet_proto.set_salient_image_url(url + "p.jpg");
  snippet_proto.set_publish_date(GetDefaultCreationTime().ToTimeT());
  snippet_proto.set_expiry_date(GetDefaultExpirationTime().ToTimeT());
  snippet_proto.set_remote_category_id(1);
  auto* source = snippet_proto.add_sources();
  source->set_url(url);
  source->set_publisher_name("Publisher");
  source->set_amp_url(url + "amp");
  return RemoteSuggestion::CreateFromProto(snippet_proto);
}

class RemoteSuggestionBuilder {
 public:
  RemoteSuggestionBuilder() = default;

  RemoteSuggestionBuilder& AddId(const std::string& id) {
    if (!ids_) {
      ids_ = std::vector<std::string>();
    }
    ids_->push_back(id);
    return *this;
  }
  RemoteSuggestionBuilder& SetTitle(const std::string& title) {
    title_ = title;
    return *this;
  }
  RemoteSuggestionBuilder& SetSnippet(const std::string& snippet) {
    snippet_ = snippet;
    return *this;
  }
  RemoteSuggestionBuilder& SetImageUrl(const std::string& image_url) {
    salient_image_url_ = image_url;
    return *this;
  }
  RemoteSuggestionBuilder& SetPublishDate(const base::Time& publish_date) {
    publish_date_ = publish_date;
    return *this;
  }
  RemoteSuggestionBuilder& SetExpiryDate(const base::Time& expiry_date) {
    expiry_date_ = expiry_date;
    return *this;
  }
  RemoteSuggestionBuilder& SetScore(double score) {
    score_ = score;
    return *this;
  }
  RemoteSuggestionBuilder& SetIsDismissed(bool is_dismissed) {
    is_dismissed_ = is_dismissed;
    return *this;
  }
  RemoteSuggestionBuilder& SetRemoteCategoryId(int remote_category_id) {
    remote_category_id_ = remote_category_id;
    return *this;
  }
  RemoteSuggestionBuilder& SetUrl(const std::string& url) {
    url_ = url;
    return *this;
  }
  RemoteSuggestionBuilder& SetPublisher(const std::string& publisher) {
    publisher_name_ = publisher;
    return *this;
  }
  RemoteSuggestionBuilder& SetAmpUrl(const std::string& amp_url) {
    amp_url_ = amp_url;
    return *this;
  }
  RemoteSuggestionBuilder& SetFetchDate(const base::Time& fetch_date) {
    fetch_date_ = fetch_date;
    return *this;
  }

  std::unique_ptr<RemoteSuggestion> Build() const {
    SnippetProto proto;
    proto.set_title(title_.value_or("Title"));
    proto.set_snippet(snippet_.value_or("Snippet"));
    proto.set_salient_image_url(
        salient_image_url_.value_or("http://image_url.com/"));
    proto.set_publish_date(
        publish_date_.value_or(GetDefaultCreationTime()).ToInternalValue());
    proto.set_expiry_date(
        expiry_date_.value_or(GetDefaultExpirationTime()).ToInternalValue());
    proto.set_score(score_.value_or(1));
    proto.set_dismissed(is_dismissed_.value_or(false));
    proto.set_remote_category_id(remote_category_id_.value_or(1));
    auto* source = proto.add_sources();
    source->set_url(url_.value_or("http://url.com/"));
    source->set_publisher_name(publisher_name_.value_or("Publisher"));
    source->set_amp_url(amp_url_.value_or("http://amp_url.com/"));
    proto.set_fetch_date(
        fetch_date_.value_or(base::Time::Now()).ToInternalValue());
    for (const auto& id :
         ids_.value_or(std::vector<std::string>{source->url()})) {
      proto.add_ids(id);
    }
    return RemoteSuggestion::CreateFromProto(proto);
  }

 private:
  base::Optional<std::vector<std::string>> ids_;
  base::Optional<std::string> title_;
  base::Optional<std::string> snippet_;
  base::Optional<std::string> salient_image_url_;
  base::Optional<base::Time> publish_date_;
  base::Optional<base::Time> expiry_date_;
  base::Optional<double> score_;
  base::Optional<bool> is_dismissed_;
  base::Optional<int> remote_category_id_;
  base::Optional<std::string> url_;
  base::Optional<std::string> publisher_name_;
  base::Optional<std::string> amp_url_;
  base::Optional<base::Time> fetch_date_;
};

class FetchedCategoryBuilder {
 public:
  FetchedCategoryBuilder() = default;

  FetchedCategoryBuilder& SetCategory(Category category) {
    category_ = category;
    return *this;
  }
  FetchedCategoryBuilder& SetTitle(const std::string& title) {
    title_ = base::UTF8ToUTF16(title);
    return *this;
  }
  FetchedCategoryBuilder& SetCardLayout(
      ContentSuggestionsCardLayout card_layout) {
    card_layout_ = card_layout;
    return *this;
  }
  FetchedCategoryBuilder& SetAdditionalAction(
      ContentSuggestionsAdditionalAction additional_action) {
    additional_action_ = additional_action;
    return *this;
  }
  FetchedCategoryBuilder& SetShowIfEmpty(bool show_if_empty) {
    show_if_empty_ = show_if_empty;
    return *this;
  }
  FetchedCategoryBuilder& SetNoSuggestionsMessage(
      const std::string& no_suggestions_message) {
    no_suggestions_message_ = base::UTF8ToUTF16(no_suggestions_message);
    return *this;
  }
  FetchedCategoryBuilder& AddSuggestionViaBuilder(
      const RemoteSuggestionBuilder& builder) {
    if (!suggestion_builders_) {
      suggestion_builders_ = std::vector<RemoteSuggestionBuilder>();
    }
    suggestion_builders_->push_back(builder);
    return *this;
  }

  FetchedCategory Build() const {
    FetchedCategory result = FetchedCategory(
        category_.value_or(Category::FromRemoteCategory(1)),
        CategoryInfo(
            title_.value_or(base::UTF8ToUTF16("Category title")),
            card_layout_.value_or(ContentSuggestionsCardLayout::FULL_CARD),
            additional_action_.value_or(
                ContentSuggestionsAdditionalAction::FETCH),
            show_if_empty_.value_or(false),
            no_suggestions_message_.value_or(
                base::UTF8ToUTF16("No suggestions message"))));

    if (suggestion_builders_) {
      for (const auto& suggestion_builder : *suggestion_builders_)
        result.suggestions.push_back(suggestion_builder.Build());
    }
    return result;
  }

 private:
  base::Optional<Category> category_;
  base::Optional<base::string16> title_;
  base::Optional<ContentSuggestionsCardLayout> card_layout_;
  base::Optional<ContentSuggestionsAdditionalAction> additional_action_;
  base::Optional<bool> show_if_empty_;
  base::Optional<base::string16> no_suggestions_message_;
  base::Optional<std::vector<RemoteSuggestionBuilder>> suggestion_builders_;
};

using ServeImageCallback = base::Callback<void(
    const std::string&,
    base::Callback<void(const std::string&,
                        const gfx::Image&,
                        const image_fetcher::RequestMetadata&)>)>;

void ServeOneByOneImage(
    image_fetcher::ImageFetcherDelegate* notify,
    const std::string& id,
    base::Callback<void(const std::string&,
                        const gfx::Image&,
                        const image_fetcher::RequestMetadata&)> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, id, gfx::test::CreateImage(1, 1),
                            image_fetcher::RequestMetadata()));
  notify->OnImageDataFetched(id, "1-by-1-image-data");
}

gfx::Image FetchImage(RemoteSuggestionsProviderImpl* provider,
                      const ContentSuggestion::ID& suggestion_id) {
  gfx::Image result;
  base::RunLoop run_loop;
  provider->FetchSuggestionImage(
      suggestion_id, base::Bind(
                         [](base::Closure signal, gfx::Image* output,
                            const gfx::Image& loaded) {
                           *output = loaded;
                           signal.Run();
                         },
                         run_loop.QuitClosure(), &result));
  run_loop.Run();
  return result;
}

class MockImageFetcher : public ImageFetcher {
 public:
  MOCK_METHOD1(SetImageFetcherDelegate, void(ImageFetcherDelegate*));
  MOCK_METHOD1(SetDataUseServiceName, void(DataUseServiceName));
  MOCK_METHOD1(SetImageDownloadLimit,
               void(base::Optional<int64_t> max_download_bytes));
  MOCK_METHOD1(SetDesiredImageFrameSize, void(const gfx::Size&));
  MOCK_METHOD4(StartOrQueueNetworkRequest,
               void(const std::string&,
                    const GURL&,
                    const ImageFetcherCallback&,
                    const net::NetworkTrafficAnnotationTag&));
  MOCK_METHOD0(GetImageDecoder, image_fetcher::ImageDecoder*());
};

class FakeImageDecoder : public image_fetcher::ImageDecoder {
 public:
  FakeImageDecoder() {}
  ~FakeImageDecoder() override = default;
  void DecodeImage(
      const std::string& image_data,
      const gfx::Size& desired_image_frame_size,
      const image_fetcher::ImageDecodedCallback& callback) override {
    callback.Run(decoded_image_);
  }

  void SetDecodedImage(const gfx::Image& image) { decoded_image_ = image; }

 private:
  gfx::Image decoded_image_;
};

class MockScheduler : public RemoteSuggestionsScheduler {
 public:
  MOCK_METHOD1(SetProvider, void(RemoteSuggestionsProvider* provider));
  MOCK_METHOD0(OnProviderActivated, void());
  MOCK_METHOD0(OnProviderDeactivated, void());
  MOCK_METHOD0(OnSuggestionsCleared, void());
  MOCK_METHOD0(OnHistoryCleared, void());
  MOCK_METHOD0(AcquireQuotaForInteractiveFetch, bool());
  MOCK_METHOD1(OnInteractiveFetchFinished, void(Status fetch_status));
  MOCK_METHOD0(OnBrowserForegrounded, void());
  MOCK_METHOD0(OnBrowserColdStart, void());
  MOCK_METHOD0(OnSuggestionsSurfaceOpened, void());
  MOCK_METHOD0(OnPersistentSchedulerWakeUp, void());
  MOCK_METHOD0(OnBrowserUpgraded, void());
};

class MockRemoteSuggestionsFetcher : public RemoteSuggestionsFetcher {
 public:
  // GMock does not support movable-only types (SnippetsAvailableCallback is
  // OnceCallback), therefore, the call is redirected to a mock method with a
  // pointer to the callback.
  void FetchSnippets(const RequestParams& params,
                     SnippetsAvailableCallback callback) override {
    FetchSnippets(params, &callback);
  }
  MOCK_METHOD2(FetchSnippets,
               void(const RequestParams& params,
                    SnippetsAvailableCallback* callback));
  MOCK_CONST_METHOD0(GetLastStatusForDebugging, const std::string&());
  MOCK_CONST_METHOD0(GetLastJsonForDebugging, const std::string&());
  MOCK_CONST_METHOD0(GetFetchUrlForDebugging, const GURL&());
};

class MockPrefetchedPagesTracker : public PrefetchedPagesTracker {
 public:
  MOCK_CONST_METHOD0(IsInitialized, bool());

  // GMock does not support movable-only types (e.g. OnceCallback), therefore,
  // the call is redirected to a mock method with a pointer to the callback.
  void AddInitializationCompletedCallback(
      base::OnceCallback<void()> callback) override {
    AddInitializationCompletedCallback(&callback);
  }
  MOCK_METHOD1(AddInitializationCompletedCallback,
               void(base::OnceCallback<void()>* callback));

  MOCK_CONST_METHOD1(PrefetchedOfflinePageExists, bool(const GURL& url));
};

}  // namespace

class RemoteSuggestionsProviderImplTest : public ::testing::Test {
 public:
  RemoteSuggestionsProviderImplTest()
      : category_ranker_(base::MakeUnique<ConstantCategoryRanker>()),
        user_classifier_(/*pref_service=*/nullptr,
                         base::MakeUnique<base::DefaultClock>()),
        mock_suggestions_fetcher_(nullptr),
        image_fetcher_(nullptr),
        scheduler_(base::MakeUnique<NiceMock<MockScheduler>>()),
        database_(nullptr) {
    RemoteSuggestionsProviderImpl::RegisterProfilePrefs(
        utils_.pref_service()->registry());
    RequestThrottler::RegisterProfilePrefs(utils_.pref_service()->registry());

    EXPECT_TRUE(database_dir_.CreateUniqueTempDir());
  }

  ~RemoteSuggestionsProviderImplTest() override {
    // We need to run the message loop after deleting the database, because
    // ProtoDatabaseImpl deletes the actual LevelDB asynchronously on the task
    // runner. Without this, we'd get reports of memory leaks.
    base::RunLoop().RunUntilIdle();
  }

  // TODO(vitaliii): Rewrite this function to initialize a test class member
  // instead of creating a new provider.
  std::unique_ptr<RemoteSuggestionsProviderImpl> MakeSuggestionsProvider() {
    auto provider = MakeSuggestionsProviderWithoutInitialization(
        /*use_mock_prefetched_pages_tracker=*/false);
    WaitForSuggestionsProviderInitialization(provider.get());
    return provider;
  }

  std::unique_ptr<RemoteSuggestionsProviderImpl>
  MakeSuggestionsProviderWithoutInitialization(
      bool use_mock_prefetched_pages_tracker) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());

    utils_.ResetSigninManager();
    auto mock_suggestions_fetcher =
        base::MakeUnique<StrictMock<MockRemoteSuggestionsFetcher>>();
    mock_suggestions_fetcher_ = mock_suggestions_fetcher.get();

    std::unique_ptr<PrefetchedPagesTracker> prefetched_pages_tracker;
    if (use_mock_prefetched_pages_tracker) {
      prefetched_pages_tracker =
          base::MakeUnique<StrictMock<MockPrefetchedPagesTracker>>();
    }
    prefetched_pages_tracker_ = prefetched_pages_tracker.get();

    auto image_fetcher = base::MakeUnique<NiceMock<MockImageFetcher>>();

    image_fetcher_ = image_fetcher.get();
    EXPECT_CALL(*image_fetcher, SetImageFetcherDelegate(_));
    ON_CALL(*image_fetcher, GetImageDecoder())
        .WillByDefault(Return(&image_decoder_));
    EXPECT_FALSE(observer_);
    observer_ = base::MakeUnique<FakeContentSuggestionsProviderObserver>();
    auto database = base::MakeUnique<RemoteSuggestionsDatabase>(
        database_dir_.GetPath(), task_runner);
    database_ = database.get();
    return base::MakeUnique<RemoteSuggestionsProviderImpl>(
        observer_.get(), utils_.pref_service(), "fr", category_ranker_.get(),
        scheduler_.get(), std::move(mock_suggestions_fetcher),
        std::move(image_fetcher), std::move(database),
        base::MakeUnique<RemoteSuggestionsStatusService>(
            utils_.fake_signin_manager(), utils_.pref_service(), std::string()),
        std::move(prefetched_pages_tracker));
  }

  std::unique_ptr<RemoteSuggestionsProviderImpl>
  MakeSuggestionsProviderWithoutInitializationWithStrictScheduler() {
    scheduler_ = base::MakeUnique<StrictMock<MockScheduler>>();
    return MakeSuggestionsProviderWithoutInitialization(
        /*use_mock_prefetched_pages_tracker=*/false);
  }

  void WaitForSuggestionsProviderInitialization(
      RemoteSuggestionsProviderImpl* provider) {
    EXPECT_EQ(RemoteSuggestionsProviderImpl::State::NOT_INITED,
              provider->state_);

    // TODO(treib): Find a better way to wait for initialization to finish.
    base::RunLoop().RunUntilIdle();
    EXPECT_NE(RemoteSuggestionsProviderImpl::State::NOT_INITED,
              provider->state_);
  }

  void ResetSuggestionsProvider(
      std::unique_ptr<RemoteSuggestionsProviderImpl>* provider) {
    provider->reset();
    observer_.reset();
    *provider = MakeSuggestionsProvider();
  }

  void SetCategoryRanker(std::unique_ptr<CategoryRanker> category_ranker) {
    category_ranker_ = std::move(category_ranker);
  }

  ContentSuggestion::ID MakeArticleID(const std::string& id_within_category) {
    return ContentSuggestion::ID(articles_category(), id_within_category);
  }

  Category articles_category() {
    return Category::FromKnownCategory(KnownCategories::ARTICLES);
  }

  ContentSuggestion::ID MakeOtherID(const std::string& id_within_category) {
    return ContentSuggestion::ID(other_category(), id_within_category);
  }

  // TODO(tschumann): Get rid of the convenience other_category() and
  // unknown_category() helpers -- tests can just define their own.
  Category other_category() { return Category::FromRemoteCategory(2); }

  Category unknown_category() {
    return Category::FromRemoteCategory(kUnknownRemoteCategoryId);
  }

 protected:
  FakeContentSuggestionsProviderObserver& observer() { return *observer_; }
  StrictMock<MockRemoteSuggestionsFetcher>* mock_suggestions_fetcher() {
    return mock_suggestions_fetcher_;
  }
  PrefetchedPagesTracker* prefetched_pages_tracker() {
    return prefetched_pages_tracker_;
  }
  // TODO(tschumann): Make this a strict-mock. We want to avoid unneccesary
  // network requests.
  NiceMock<MockImageFetcher>* image_fetcher() { return image_fetcher_; }
  FakeImageDecoder* image_decoder() { return &image_decoder_; }
  PrefService* pref_service() { return utils_.pref_service(); }
  RemoteSuggestionsDatabase* database() { return database_; }
  MockScheduler* scheduler() { return scheduler_.get(); }

  void FetchTheseSuggestions(
      RemoteSuggestionsProviderImpl* provider,
      bool interactive_request,
      Status status,
      base::Optional<std::vector<FetchedCategory>> fetched_categories) {
    RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
    EXPECT_CALL(*mock_suggestions_fetcher(), FetchSnippets(_, _))
        .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
        .RetiresOnSaturation();
    provider->FetchSuggestions(
        interactive_request, RemoteSuggestionsProvider::FetchStatusCallback());
    std::move(snippets_callback)
        .Run(Status(StatusCode::SUCCESS, "message"),
             std::move(fetched_categories));
  }

  void FetchMoreTheseSuggestions(
      RemoteSuggestionsProviderImpl* provider,
      const Category& category,
      const std::set<std::string>& known_suggestion_ids,
      FetchDoneCallback fetch_done_callback,
      Status status,
      base::Optional<std::vector<FetchedCategory>> fetched_categories) {
    RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
    EXPECT_CALL(*mock_suggestions_fetcher(), FetchSnippets(_, _))
        .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
        .RetiresOnSaturation();
    EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    provider->Fetch(category, known_suggestion_ids, fetch_done_callback);
    std::move(snippets_callback)
        .Run(Status(StatusCode::SUCCESS, "message"),
             std::move(fetched_categories));
  }

  void SetOrderNewRemoteCategoriesBasedOnArticlesCategoryParam(bool value) {
    // params_manager supports only one
    // |SetVariationParamsWithFeatureAssociations| at a time, so we clear
    // previous settings first to make this explicit.
    params_manager_.ClearAllVariationParams();
    params_manager_.SetVariationParamsWithFeatureAssociations(
        kArticleSuggestionsFeature.name,
        {{"order_new_remote_categories_based_on_articles_category",
          value ? "true" : "false"}},
        {kArticleSuggestionsFeature.name});
  }

  void EnableKeepingPrefetchedContentSuggestions(
      int max_additional_prefetched_suggestions,
      const base::TimeDelta& max_age_for_additional_prefetched_suggestion) {
    // params_manager supports only one
    // |SetVariationParamsWithFeatureAssociations| at a time, so we clear
    // previous settings first to make this explicit.
    params_manager_.ClearAllVariationParams();
    params_manager_.SetVariationParamsWithFeatureAssociations(
        kKeepPrefetchedContentSuggestions.name,
        {
            {"max_additional_prefetched_suggestions",
             base::IntToString(max_additional_prefetched_suggestions)},
            {"max_age_for_additional_prefetched_suggestion_minutes",
             base::IntToString(
                 max_age_for_additional_prefetched_suggestion.InMinutes())},
        },
        {kKeepPrefetchedContentSuggestions.name});
  }

 private:
  variations::testing::VariationParamsManager params_manager_;
  test::RemoteSuggestionsTestUtils utils_;
  base::MessageLoop message_loop_;
  std::unique_ptr<CategoryRanker> category_ranker_;
  UserClassifier user_classifier_;
  std::unique_ptr<FakeContentSuggestionsProviderObserver> observer_;
  StrictMock<MockRemoteSuggestionsFetcher>* mock_suggestions_fetcher_;
  PrefetchedPagesTracker* prefetched_pages_tracker_;
  NiceMock<MockImageFetcher>* image_fetcher_;
  FakeImageDecoder image_decoder_;
  std::unique_ptr<MockScheduler> scheduler_;

  base::ScopedTempDir database_dir_;
  RemoteSuggestionsDatabase* database_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsProviderImplTest);
};

TEST_F(RemoteSuggestionsProviderImplTest, Full) {
  auto provider = MakeSuggestionsProvider();

  // TODO(vitaliii): Inline the vector creation in FetchTheseSuggestions.
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId(kSuggestionUrl)
                                       .SetTitle(kSuggestionTitle)
                                       .SetSnippet(kSuggestionText)
                                       .SetPublishDate(GetDefaultCreationTime())
                                       .SetPublisher(kSuggestionPublisherName))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));

  const ContentSuggestion& suggestion =
      observer().SuggestionsForCategory(articles_category()).front();

  EXPECT_EQ(MakeArticleID(kSuggestionUrl), suggestion.id());
  EXPECT_EQ(kSuggestionTitle, base::UTF16ToUTF8(suggestion.title()));
  EXPECT_EQ(kSuggestionText, base::UTF16ToUTF8(suggestion.snippet_text()));
  EXPECT_EQ(GetDefaultCreationTime(), suggestion.publish_date());
  EXPECT_EQ(kSuggestionPublisherName,
            base::UTF16ToUTF8(suggestion.publisher_name()));
}

TEST_F(RemoteSuggestionsProviderImplTest, CategoryTitle) {
  const base::string16 test_default_title =
      base::UTF8ToUTF16(kTestJsonDefaultCategoryTitle);

  // Don't send an initial response -- we want to test what happens without any
  // server status.
  auto provider = MakeSuggestionsProvider();

  // The articles category should be there by default, and have a title.
  CategoryInfo info_before = provider->GetCategoryInfo(articles_category());
  ASSERT_THAT(info_before.title(), Not(IsEmpty()));
  ASSERT_THAT(info_before.title(), Not(Eq(test_default_title)));
  EXPECT_THAT(info_before.additional_action(),
              Eq(ContentSuggestionsAdditionalAction::FETCH));
  EXPECT_THAT(info_before.show_if_empty(), Eq(true));

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .SetTitle(base::UTF16ToUTF8(test_default_title))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder())
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));

  // The response contained a title, |kTestJsonDefaultCategoryTitle|.
  // Make sure we updated the title in the CategoryInfo.
  CategoryInfo info_with_title = provider->GetCategoryInfo(articles_category());
  EXPECT_THAT(info_before.title(), Not(Eq(info_with_title.title())));
  EXPECT_THAT(test_default_title, Eq(info_with_title.title()));
  EXPECT_THAT(info_before.additional_action(),
              Eq(ContentSuggestionsAdditionalAction::FETCH));
  EXPECT_THAT(info_before.show_if_empty(), Eq(true));
}

TEST_F(RemoteSuggestionsProviderImplTest, MultipleCategories) {
  auto provider = MakeSuggestionsProvider();
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(1))
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder()
                  .AddId(base::StringPrintf("%s/%d", kSuggestionUrl, 0))
                  .SetTitle(kSuggestionTitle)
                  .SetSnippet(kSuggestionText)
                  .SetPublishDate(GetDefaultCreationTime())
                  .SetPublisher(kSuggestionPublisherName))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(2))
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder()
                  .AddId(base::StringPrintf("%s/%d", kSuggestionUrl, 1))
                  .SetTitle(kSuggestionTitle)
                  .SetSnippet(kSuggestionText)
                  .SetPublishDate(GetDefaultCreationTime())
                  .SetPublisher(kSuggestionPublisherName))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().statuses(),
              Eq(std::map<Category, CategoryStatus, Category::CompareByID>{
                  {articles_category(), CategoryStatus::AVAILABLE},
                  {other_category(), CategoryStatus::AVAILABLE},
              }));

  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
  EXPECT_THAT(provider->GetSuggestionsForTesting(other_category()), SizeIs(1));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));

  ASSERT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));

  {
    const ContentSuggestion& suggestion =
        observer().SuggestionsForCategory(articles_category()).front();
    EXPECT_EQ(MakeArticleID(std::string(kSuggestionUrl) + "/0"),
              suggestion.id());
    EXPECT_EQ(kSuggestionTitle, base::UTF16ToUTF8(suggestion.title()));
    EXPECT_EQ(kSuggestionText, base::UTF16ToUTF8(suggestion.snippet_text()));
    EXPECT_EQ(GetDefaultCreationTime(), suggestion.publish_date());
    EXPECT_EQ(kSuggestionPublisherName,
              base::UTF16ToUTF8(suggestion.publisher_name()));
  }

  {
    const ContentSuggestion& suggestion =
        observer().SuggestionsForCategory(other_category()).front();
    EXPECT_EQ(MakeOtherID(std::string(kSuggestionUrl) + "/1"), suggestion.id());
    EXPECT_EQ(kSuggestionTitle, base::UTF16ToUTF8(suggestion.title()));
    EXPECT_EQ(kSuggestionText, base::UTF16ToUTF8(suggestion.snippet_text()));
    EXPECT_EQ(GetDefaultCreationTime(), suggestion.publish_date());
    EXPECT_EQ(kSuggestionPublisherName,
              base::UTF16ToUTF8(suggestion.publisher_name()));
  }
}

TEST_F(RemoteSuggestionsProviderImplTest, ArticleCategoryInfo) {
  auto provider = MakeSuggestionsProvider();
  CategoryInfo article_info = provider->GetCategoryInfo(articles_category());
  EXPECT_THAT(article_info.additional_action(),
              Eq(ContentSuggestionsAdditionalAction::FETCH));
  EXPECT_THAT(article_info.show_if_empty(), Eq(true));
}

TEST_F(RemoteSuggestionsProviderImplTest, ExperimentalCategoryInfo) {
  auto provider = MakeSuggestionsProvider();
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(1))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("1"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(kUnknownRemoteCategoryId))
          .SetAdditionalAction(ContentSuggestionsAdditionalAction::NONE)
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("2"))
          .Build());
  // Load data with multiple categories so that a new experimental category gets
  // registered.
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  CategoryInfo info = provider->GetCategoryInfo(unknown_category());
  EXPECT_THAT(info.additional_action(),
              Eq(ContentSuggestionsAdditionalAction::NONE));
  EXPECT_THAT(info.show_if_empty(), Eq(false));
}

TEST_F(RemoteSuggestionsProviderImplTest, AddRemoteCategoriesToCategoryRanker) {
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(11))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("11"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(13))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("13"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(12))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("12"))
          .Build());
  {
    // The order of categories is determined by the order in which they are
    // added. Thus, the latter is tested here.
    InSequence s;
    EXPECT_CALL(*raw_mock_ranker,
                AppendCategoryIfNecessary(Category::FromRemoteCategory(11)));
    EXPECT_CALL(*raw_mock_ranker,
                AppendCategoryIfNecessary(Category::FromRemoteCategory(13)));
    EXPECT_CALL(*raw_mock_ranker,
                AppendCategoryIfNecessary(Category::FromRemoteCategory(12)));
  }
  auto provider = MakeSuggestionsProvider();
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       AddRemoteCategoriesToCategoryRankerRelativeToArticles) {
  SetOrderNewRemoteCategoriesBasedOnArticlesCategoryParam(true);
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(14))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("14"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(13))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("13"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(1))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("1"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(12))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("12"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(11))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("11"))
          .Build());
  {
    InSequence s;
    EXPECT_CALL(*raw_mock_ranker,
                InsertCategoryBeforeIfNecessary(
                    Category::FromRemoteCategory(14), articles_category()));
    EXPECT_CALL(*raw_mock_ranker,
                InsertCategoryBeforeIfNecessary(
                    Category::FromRemoteCategory(13), articles_category()));
    EXPECT_CALL(*raw_mock_ranker,
                InsertCategoryAfterIfNecessary(Category::FromRemoteCategory(11),
                                               articles_category()));
    EXPECT_CALL(*raw_mock_ranker,
                InsertCategoryAfterIfNecessary(Category::FromRemoteCategory(12),
                                               articles_category()));
  }
  auto provider = MakeSuggestionsProvider();
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
}

TEST_F(
    RemoteSuggestionsProviderImplTest,
    AddRemoteCategoriesToCategoryRankerRelativeToArticlesWithArticlesAbsent) {
  SetOrderNewRemoteCategoriesBasedOnArticlesCategoryParam(true);
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(11))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("11"))
          .Build());

  EXPECT_CALL(*raw_mock_ranker, InsertCategoryBeforeIfNecessary(_, _)).Times(0);
  EXPECT_CALL(*raw_mock_ranker,
              AppendCategoryIfNecessary(Category::FromRemoteCategory(11)));
  auto provider = MakeSuggestionsProvider();
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
}

TEST_F(RemoteSuggestionsProviderImplTest, PersistCategoryInfos) {
  auto provider = MakeSuggestionsProvider();
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("1"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(kUnknownRemoteCategoryId))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("2"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_EQ(observer().StatusForCategory(articles_category()),
            CategoryStatus::AVAILABLE);
  ASSERT_EQ(observer().StatusForCategory(
                Category::FromRemoteCategory(kUnknownRemoteCategoryId)),
            CategoryStatus::AVAILABLE);

  CategoryInfo info_articles_before =
      provider->GetCategoryInfo(articles_category());
  CategoryInfo info_unknown_before = provider->GetCategoryInfo(
      Category::FromRemoteCategory(kUnknownRemoteCategoryId));

  // Recreate the provider to simulate a Chrome restart.
  ResetSuggestionsProvider(&provider);

  // The categories should have been restored.
  ASSERT_NE(observer().StatusForCategory(articles_category()),
            CategoryStatus::NOT_PROVIDED);
  ASSERT_NE(observer().StatusForCategory(
                Category::FromRemoteCategory(kUnknownRemoteCategoryId)),
            CategoryStatus::NOT_PROVIDED);

  EXPECT_EQ(observer().StatusForCategory(articles_category()),
            CategoryStatus::AVAILABLE);
  EXPECT_EQ(observer().StatusForCategory(
                Category::FromRemoteCategory(kUnknownRemoteCategoryId)),
            CategoryStatus::AVAILABLE);

  CategoryInfo info_articles_after =
      provider->GetCategoryInfo(articles_category());
  CategoryInfo info_unknown_after = provider->GetCategoryInfo(
      Category::FromRemoteCategory(kUnknownRemoteCategoryId));

  EXPECT_EQ(info_articles_before.title(), info_articles_after.title());
  EXPECT_EQ(info_unknown_before.title(), info_unknown_after.title());
}

TEST_F(RemoteSuggestionsProviderImplTest, PersistRemoteCategoryOrder) {
  // We create a provider with a normal ranker to store the order.
  auto provider = MakeSuggestionsProvider();
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(11))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("11"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(13))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("13"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(12))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("12"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  // We manually recreate the provider to simulate Chrome restart and enforce a
  // mock ranker.
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));
  // Ensure that the order is not fetched.
  EXPECT_CALL(*mock_suggestions_fetcher(), FetchSnippets(_, _)).Times(0);
  {
    // The order of categories is determined by the order in which they are
    // added. Thus, the latter is tested here.
    InSequence s;
    // Article category always exists and, therefore, it is stored in prefs too.
    EXPECT_CALL(*raw_mock_ranker,
                AppendCategoryIfNecessary(articles_category()));

    EXPECT_CALL(*raw_mock_ranker,
                AppendCategoryIfNecessary(Category::FromRemoteCategory(11)));
    EXPECT_CALL(*raw_mock_ranker,
                AppendCategoryIfNecessary(Category::FromRemoteCategory(13)));
    EXPECT_CALL(*raw_mock_ranker,
                AppendCategoryIfNecessary(Category::FromRemoteCategory(12)));
  }
  ResetSuggestionsProvider(&provider);
}

TEST_F(RemoteSuggestionsProviderImplTest, PersistSuggestions) {
  auto provider = MakeSuggestionsProvider();
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(1))
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("1").SetRemoteCategoryId(1))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(2))
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("2").SetRemoteCategoryId(2))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));

  // Recreate the provider to simulate a Chrome restart.
  ResetSuggestionsProvider(&provider);

  // The suggestions in both categories should have been restored.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  EXPECT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, DontNotifyIfNotAvailable) {
  // Get some suggestions into the database.
  auto provider = MakeSuggestionsProvider();
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(1))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("1"))
          .Build());
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(Category::FromRemoteCategory(2))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("2"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));

  provider.reset();

  // Set the pref that disables remote suggestions.
  pref_service()->SetBoolean(prefs::kEnableSnippets, false);

  // Recreate the provider to simulate a Chrome start.
  ResetSuggestionsProvider(&provider);

  ASSERT_THAT(RemoteSuggestionsProviderImpl::State::DISABLED,
              Eq(provider->state_));

  // Now the observer should not have received any suggestions.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              IsEmpty());
  EXPECT_THAT(observer().SuggestionsForCategory(other_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, Clear) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("1"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));

  provider->ClearCachedSuggestions(articles_category());
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, ReplaceSuggestions) {
  auto provider = MakeSuggestionsProvider();

  std::string first("http://first");
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId(first))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              ElementsAre(Pointee(Property(&RemoteSuggestion::id, first))));

  std::string second("http://second");
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId(second))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  // The suggestions loaded last replace all that was loaded previously.
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              ElementsAre(Pointee(Property(&RemoteSuggestion::id, second))));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldResolveFetchedSuggestionThumbnail) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("id"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              ElementsAre(Pointee(Property(&RemoteSuggestion::id, "id"))));

  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  ServeImageCallback serve_one_by_one_image_callback =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke(CreateFunctor(serve_one_by_one_image_callback))));

  gfx::Image image = FetchImage(provider.get(), MakeArticleID("id"));
  ASSERT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());
}

TEST_F(RemoteSuggestionsProviderImplTest, ShouldFetchMore) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("first"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              ElementsAre(Pointee(Property(&RemoteSuggestion::id, "first"))));

  auto expect_only_second_suggestion_received =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        EXPECT_THAT(suggestions, SizeIs(1));
        EXPECT_THAT(suggestions[0].id().id_within_category(), Eq("second"));
      });
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("second"))
          .Build());
  FetchMoreTheseSuggestions(
      provider.get(), articles_category(),
      /*known_suggestion_ids=*/std::set<std::string>(),
      /*fetch_done_callback=*/expect_only_second_suggestion_received,
      Status(StatusCode::SUCCESS, "message"), std::move(fetched_categories));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldResolveFetchedMoreSuggestionThumbnail) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId("id"))
          .Build());

  auto assert_only_first_suggestion_received =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        ASSERT_THAT(suggestions, SizeIs(1));
        ASSERT_THAT(suggestions[0].id().id_within_category(), Eq("id"));
      });
  FetchMoreTheseSuggestions(
      provider.get(), articles_category(),
      /*known_suggestion_ids=*/std::set<std::string>(),
      /*fetch_done_callback=*/assert_only_first_suggestion_received,
      Status(StatusCode::SUCCESS, "message"), std::move(fetched_categories));

  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  ServeImageCallback serve_one_by_one_image_callback =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke(CreateFunctor(serve_one_by_one_image_callback))));

  gfx::Image image = FetchImage(provider.get(), MakeArticleID("id"));
  ASSERT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());
}

// Imagine that we have surfaces A and B. The user fetches more in A, this
// should not add any suggestions to B.
TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldNotChangeSuggestionsInOtherSurfacesWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);
  WaitForSuggestionsProviderInitialization(provider.get());

  // Fetch a suggestion.
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://old.com/"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              ElementsAre(Property(&ContentSuggestion::id,
                                   MakeArticleID("http://old.com/"))));

  // Now fetch more, but first prepare a response.
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://fetched-more.com/"))
          .Build());

  // The surface issuing the fetch more gets response via callback.
  auto assert_receiving_one_new_suggestion =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        ASSERT_THAT(suggestions, SizeIs(1));
        ASSERT_THAT(suggestions[0].id().id_within_category(),
                    Eq("http://fetched-more.com/"));
      });
  FetchMoreTheseSuggestions(
      provider.get(), articles_category(),
      /*known_suggestion_ids=*/{"http://old.com/"},
      /*fetch_done_callback=*/assert_receiving_one_new_suggestion,
      Status(StatusCode::SUCCESS, "message"), std::move(fetched_categories));

  // Other surfaces should remain the same.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              ElementsAre(Property(&ContentSuggestion::id,
                                   MakeArticleID("http://old.com/"))));
}

// Imagine that we have surfaces A and B. The user fetches more in A. This
// should not affect the next fetch more in B, i.e. assuming the same server
// response the same suggestions must be fetched in B if the user fetches more
// there as well.
TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldNotAffectFetchMoreInOtherSurfacesWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);
  WaitForSuggestionsProviderInitialization(provider.get());

  // Fetch more on the surface A.
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));
  fetched_categories[0].suggestions.push_back(
      CreateTestRemoteSuggestion("http://fetched-more.com/"));

  auto assert_receiving_one_new_suggestion =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        ASSERT_THAT(suggestions, SizeIs(1));
        ASSERT_THAT(suggestions[0].id().id_within_category(),
                    Eq("http://fetched-more.com/"));
      });
  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_suggestions_fetcher(), FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  provider->Fetch(articles_category(),
                  /*known_suggestion_ids=*/std::set<std::string>(),
                  assert_receiving_one_new_suggestion);
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"),
           std::move(fetched_categories));

  // Now fetch more on the surface B. The response is the same as before.
  fetched_categories.clear();
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));
  fetched_categories[0].suggestions.push_back(
      CreateTestRemoteSuggestion("http://fetched-more.com/"));

  // B should receive the same suggestion as was fetched more on A.
  auto expect_receiving_same_suggestion =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        ASSERT_THAT(suggestions, SizeIs(1));
        EXPECT_THAT(suggestions[0].id().id_within_category(),
                    Eq("http://fetched-more.com/"));
      });
  // The provider should not ask the fetcher to exclude the suggestion fetched
  // more on A.
  EXPECT_CALL(*mock_suggestions_fetcher(),
              FetchSnippets(Field(&RequestParams::excluded_ids,
                                  Not(Contains("http://fetched-more.com/"))),
                            _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  provider->Fetch(articles_category(),
                  /*known_suggestion_ids=*/std::set<std::string>(),
                  expect_receiving_same_suggestion);
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"),
           std::move(fetched_categories));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ClearHistoryShouldDeleteArchivedSuggestions) {
  auto provider = MakeSuggestionsProvider();
  // First get suggestions into the archived state which happens through
  // subsequent fetches. Then we verify the entries are gone from the 'archived'
  // state by trying to load their images (and we shouldn't even know the URLs
  // anymore).
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://id-1"))
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://id-2"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://new-id-1"))
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://new-id-2"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  // Make sure images of both batches are available. This is to sanity check our
  // assumptions for the test are right.
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .Times(2)
      .WillRepeatedly(WithArgs<0, 2>(Invoke(CreateFunctor(cb))));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  gfx::Image image = FetchImage(provider.get(), MakeArticleID("http://id-1"));
  ASSERT_FALSE(image.IsEmpty());
  ASSERT_EQ(1, image.Width());
  image = FetchImage(provider.get(), MakeArticleID("http://new-id-1"));
  ASSERT_FALSE(image.IsEmpty());
  ASSERT_EQ(1, image.Width());

  provider->ClearHistory(base::Time::UnixEpoch(), base::Time::Max(),
                         base::Callback<bool(const GURL& url)>());

  // Make sure images of both batches are gone.
  // Verify we cannot resolve the image of the new suggestions.
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  EXPECT_TRUE(
      FetchImage(provider.get(), MakeArticleID("http://id-1")).IsEmpty());
  EXPECT_TRUE(
      FetchImage(provider.get(), MakeArticleID("http://new-id-1")).IsEmpty());
}

namespace {

// Workaround for gMock's lack of support for movable types.
void SuggestionsLoaded(
    MockFunction<void(Status, const std::vector<ContentSuggestion>&)>* loaded,
    Status status,
    std::vector<ContentSuggestion> suggestions) {
  loaded->Call(status, suggestions);
}

}  // namespace

TEST_F(RemoteSuggestionsProviderImplTest, ReturnFetchRequestEmptyBeforeInit) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);
  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_suggestions_fetcher(), FetchSnippets(_, _)).Times(0);
  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                           IsEmpty()));
  provider->Fetch(articles_category(), std::set<std::string>(),
                  base::Bind(&SuggestionsLoaded, &loaded));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldForwardTemporaryErrorFromFetcher) {
  auto provider = MakeSuggestionsProvider();

  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(*mock_suggestions_fetcher(), FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback));
  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  provider->Fetch(articles_category(),
                  /*known_ids=*/std::set<std::string>(),
                  base::Bind(&SuggestionsLoaded, &loaded));

  EXPECT_CALL(loaded, Call(Field(&Status::code, StatusCode::TEMPORARY_ERROR),
                           IsEmpty()));
  ASSERT_FALSE(snippets_callback.is_null());
  std::move(snippets_callback)
      .Run(Status(StatusCode::TEMPORARY_ERROR, "Received invalid JSON"),
           base::nullopt);
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldNotAddNewSuggestionsAfterFetchError) {
  auto provider = MakeSuggestionsProvider();

  FetchTheseSuggestions(
      provider.get(), /*interactive_request=*/false,
      Status(StatusCode::TEMPORARY_ERROR, "Received invalid JSON"),
      base::nullopt);
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldNotClearOldSuggestionsAfterFetchError) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));
  fetched_categories[0].suggestions.push_back(
      CreateTestRemoteSuggestion(base::StringPrintf("http://abc.com/")));
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/false,
                        Status(StatusCode::SUCCESS, "success message"),
                        std::move(fetched_categories));

  ASSERT_THAT(
      provider->GetSuggestionsForTesting(articles_category()),
      ElementsAre(Pointee(Property(&RemoteSuggestion::id, "http://abc.com/"))));

  FetchTheseSuggestions(
      provider.get(), /*interactive_request=*/false,
      Status(StatusCode::TEMPORARY_ERROR, "Received invalid JSON"),
      base::nullopt);
  // This should not have changed the existing suggestions.
  EXPECT_THAT(
      provider->GetSuggestionsForTesting(articles_category()),
      ElementsAre(Pointee(Property(&RemoteSuggestion::id, "http://abc.com/"))));
}

TEST_F(RemoteSuggestionsProviderImplTest, Dismiss) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  const FetchedCategoryBuilder category_builder =
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://site.com"));
  fetched_categories.push_back(category_builder.Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
  // Load the image to store it in the database.
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(CreateFunctor(cb))));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  gfx::Image image =
      FetchImage(provider.get(), MakeArticleID("http://site.com"));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Dismissing a non-existent suggestion shouldn't do anything.
  provider->DismissSuggestion(MakeArticleID("http://othersite.com"));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));

  // Dismiss the suggestion.
  provider->DismissSuggestion(MakeArticleID("http://site.com"));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());

  // Verify we can still load the image of the discarded suggestion (other NTPs
  // might still reference it). This should come from the database -- no network
  // fetch necessary.
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  image = FetchImage(provider.get(), MakeArticleID("http://site.com"));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Make sure that fetching the same suggestion again does not re-add it.
  fetched_categories.clear();
  fetched_categories.push_back(category_builder.Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());

  // The suggestion should stay dismissed even after re-creating the provider.
  ResetSuggestionsProvider(&provider);
  fetched_categories.clear();
  fetched_categories.push_back(category_builder.Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());

  // The suggestion can be added again after clearing dismissed suggestions.
  provider->ClearDismissedSuggestionsForDebugging(articles_category());
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
  fetched_categories.clear();
  fetched_categories.push_back(category_builder.Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, GetDismissed) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://site.com"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  provider->DismissSuggestion(MakeArticleID("http://site.com"));

  provider->GetDismissedSuggestionsForDebugging(
      articles_category(),
      base::Bind(
          [](RemoteSuggestionsProviderImpl* provider,
             RemoteSuggestionsProviderImplTest* test,
             std::vector<ContentSuggestion> dismissed_suggestions) {
            EXPECT_EQ(1u, dismissed_suggestions.size());
            for (auto& suggestion : dismissed_suggestions) {
              EXPECT_EQ(test->MakeArticleID("http://site.com"),
                        suggestion.id());
            }
          },
          provider.get(), this));
  base::RunLoop().RunUntilIdle();

  // There should be no dismissed suggestion after clearing the list.
  provider->ClearDismissedSuggestionsForDebugging(articles_category());
  provider->GetDismissedSuggestionsForDebugging(
      articles_category(),
      base::Bind(
          [](RemoteSuggestionsProviderImpl* provider,
             RemoteSuggestionsProviderImplTest* test,
             std::vector<ContentSuggestion> dismissed_suggestions) {
            EXPECT_EQ(0u, dismissed_suggestions.size());
          },
          provider.get(), this));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsProviderImplTest, RemoveExpiredDismissedContent) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://first/")
                                       .SetExpiryDate(base::Time::Now()))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  // Load the image to store it in the database.
  // TODO(tschumann): Introduce some abstraction to nicely work with image
  // fetching expectations.
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(CreateFunctor(cb))));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  gfx::Image image = FetchImage(provider.get(), MakeArticleID("http://first/"));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Dismiss the suggestion
  provider->DismissSuggestion(
      ContentSuggestion::ID(articles_category(), "http://first/"));

  // Load a different suggestion - this will clear the expired dismissed ones.
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://second/"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  EXPECT_THAT(provider->GetDismissedSuggestionsForTesting(articles_category()),
              IsEmpty());

  // Verify the image got removed, too.
  EXPECT_TRUE(
      FetchImage(provider.get(), MakeArticleID("http://first/")).IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, ExpiredContentNotRemoved) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().SetExpiryDate(base::Time::Now()))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, TestSingleSource) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://source1.com")
                                       .SetUrl("http://source1.com")
                                       .SetPublisher("Source 1")
                                       .SetAmpUrl("http://source1.amp.com"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
  const RemoteSuggestion& suggestion =
      *provider->GetSuggestionsForTesting(articles_category()).front();
  EXPECT_EQ(suggestion.id(), "http://source1.com");
  EXPECT_EQ(suggestion.url(), GURL("http://source1.com"));
  EXPECT_EQ(suggestion.publisher_name(), std::string("Source 1"));
  EXPECT_EQ(suggestion.amp_url(), GURL("http://source1.amp.com"));
}

TEST_F(RemoteSuggestionsProviderImplTest, TestSingleSourceWithMissingData) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().SetPublisher("").SetAmpUrl(""))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, LogNumArticlesHistogram) {
  auto provider = MakeSuggestionsProvider();

  base::HistogramTester tester;

  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::TEMPORARY_ERROR, "message"),
                        base::nullopt);
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  // Fetch error shouldn't contribute to NumArticlesFetched.
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              IsEmpty());

  // Emptry categories list.
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::vector<FetchedCategory>());
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              IsEmpty());

  // Empty articles category.
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder().SetCategory(articles_category()).Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/3)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));

  // Suggestion list should be populated with size 1.
  const FetchedCategoryBuilder category_builder =
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://site.com/"));
  fetched_categories.clear();
  fetched_categories.push_back(category_builder.Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/3),
                          base::Bucket(/*min=*/1, /*count=*/1)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/1)));

  // Duplicate suggestion shouldn't increase the list size.
  fetched_categories.clear();
  fetched_categories.push_back(category_builder.Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/3),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      IsEmpty());

  // Dismissing a suggestion should decrease the list size. This will only be
  // logged after the next fetch.
  provider->DismissSuggestion(MakeArticleID("http://site.com/"));
  fetched_categories.clear();
  fetched_categories.push_back(category_builder.Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/4),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  // Dismissed suggestions shouldn't influence NumArticlesFetched.
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/3)));
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
}

TEST_F(RemoteSuggestionsProviderImplTest, DismissShouldRespectAllKnownUrls) {
  auto provider = MakeSuggestionsProvider();

  const std::vector<std::string> source_urls = {
      "http://mashable.com/2016/05/11/stolen",
      "http://www.aol.com/article/2016/05/stolen-doggie"};
  const std::vector<std::string> publishers = {"Mashable", "AOL"};
  const std::vector<std::string> amp_urls = {
      "http://mashable-amphtml.googleusercontent.com/1",
      "http://t2.gstatic.com/images?q=tbn:3"};

  // Add the suggestion from the mashable domain.
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId(source_urls[0])
                                       .AddId(source_urls[1])
                                       .SetUrl(source_urls[0])
                                       .SetAmpUrl(amp_urls[0])
                                       .SetPublisher(publishers[0]))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
  // Dismiss the suggestion via the mashable source corpus ID.
  provider->DismissSuggestion(MakeArticleID(source_urls[0]));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());

  // The same article from the AOL domain should now be detected as dismissed.
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId(source_urls[0])
                                       .AddId(source_urls[1])
                                       .SetUrl(source_urls[1])
                                       .SetAmpUrl(amp_urls[1])
                                       .SetPublisher(publishers[1]))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, ImageReturnedWithTheSameId) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId(kSuggestionUrl))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  gfx::Image image;
  MockFunction<void(const gfx::Image&)> image_fetched;
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
        .WillOnce(WithArgs<0, 2>(Invoke(CreateFunctor(cb))));
    EXPECT_CALL(image_fetched, Call(_)).WillOnce(SaveArg<0>(&image));
  }

  provider->FetchSuggestionImage(
      MakeArticleID(kSuggestionUrl),
      base::Bind(&MockFunction<void(const gfx::Image&)>::Call,
                 base::Unretained(&image_fetched)));
  base::RunLoop().RunUntilIdle();
  // Check that the image by ServeOneByOneImage is really served.
  EXPECT_EQ(1, image.Width());
}

TEST_F(RemoteSuggestionsProviderImplTest, EmptyImageReturnedForNonExistentId) {
  auto provider = MakeSuggestionsProvider();

  // Create a non-empty image so that we can test the image gets updated.
  gfx::Image image = gfx::test::CreateImage(1, 1);
  MockFunction<void(const gfx::Image&)> image_fetched;
  EXPECT_CALL(image_fetched, Call(_)).WillOnce(SaveArg<0>(&image));

  provider->FetchSuggestionImage(
      MakeArticleID("nonexistent"),
      base::Bind(&MockFunction<void(const gfx::Image&)>::Call,
                 base::Unretained(&image_fetched)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(image.IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       FetchingUnknownImageIdShouldNotHitDatabase) {
  // Testing that the provider is not accessing the database is tricky.
  // Therefore, we simply put in some data making sure that if the provider asks
  // the database, it will get a wrong answer.
  auto provider = MakeSuggestionsProvider();

  ContentSuggestion::ID unknown_id = MakeArticleID(kSuggestionUrl2);
  database()->SaveImage(unknown_id.id_within_category(), "some image blob");
  // Set up the image decoder to always return the 1x1 test image.
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));

  // Create a non-empty image so that we can test the image gets updated.
  gfx::Image image = gfx::test::CreateImage(2, 2);
  MockFunction<void(const gfx::Image&)> image_fetched;
  EXPECT_CALL(image_fetched, Call(_)).WillOnce(SaveArg<0>(&image));

  provider->FetchSuggestionImage(
      MakeArticleID(kSuggestionUrl2),
      base::Bind(&MockFunction<void(const gfx::Image&)>::Call,
                 base::Unretained(&image_fetched)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(image.IsEmpty()) << "got image with width: " << image.Width();
}

TEST_F(RemoteSuggestionsProviderImplTest, ClearHistoryRemovesAllSuggestions) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://first/"))
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://second/"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(2));

  provider->DismissSuggestion(MakeArticleID("http://first/"));
  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              Not(IsEmpty()));
  ASSERT_THAT(provider->GetDismissedSuggestionsForTesting(articles_category()),
              SizeIs(1));

  base::Time begin = base::Time::FromTimeT(123),
             end = base::Time::FromTimeT(456);
  base::Callback<bool(const GURL& url)> filter;
  provider->ClearHistory(begin, end, filter);

  // Verify that the observer received the update with the empty data as well.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              IsEmpty());
  EXPECT_THAT(provider->GetDismissedSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldKeepArticlesCategoryAvailableAfterClearHistory) {
  // If the provider marks that category as NOT_PROVIDED, then it won't be shown
  // at all in the UI and the user cannot load new data :-/.
  auto provider = MakeSuggestionsProvider();

  ASSERT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE));
  provider->ClearHistory(base::Time::UnixEpoch(), base::Time::Max(),
                         base::Callback<bool(const GURL& url)>());

  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE));
}

TEST_F(RemoteSuggestionsProviderImplTest, ShouldClearOrphanedImagesOnRestart) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId(kSuggestionUrl))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());

  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(CreateFunctor(cb))));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));

  gfx::Image image = FetchImage(provider.get(), MakeArticleID(kSuggestionUrl));
  EXPECT_EQ(1, image.Width());
  EXPECT_FALSE(image.IsEmpty());

  // Send new suggestion which don't include the suggestion referencing the
  // image.
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId(
              "http://something.com/pletely/unrelated"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  // The image should still be available until a restart happens.
  EXPECT_FALSE(
      FetchImage(provider.get(), MakeArticleID(kSuggestionUrl)).IsEmpty());
  ResetSuggestionsProvider(&provider);
  // After the restart, the image should be garbage collected.
  EXPECT_TRUE(
      FetchImage(provider.get(), MakeArticleID(kSuggestionUrl)).IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldHandleMoreThanMaxSuggestionsInResponse) {
  auto provider = MakeSuggestionsProvider();

  std::vector<FetchedCategory> fetched_categories;
  FetchedCategoryBuilder category_builder;
  category_builder.SetCategory(articles_category());
  for (int i = 0; i < provider->GetMaxSuggestionCountForTesting() + 1; ++i) {
    category_builder.AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId(
        base::StringPrintf("http://localhost/suggestion-id-%d", i)));
  }
  fetched_categories.push_back(category_builder.Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  // TODO(tschumann): We should probably trim out any additional results and
  // only serve the MaxSuggestionCount items.
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(provider->GetMaxSuggestionCountForTesting() + 1));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       StoreLastSuccessfullBackgroundFetchTime) {
  // On initialization of the RemoteSuggestionsProviderImpl a background fetch
  // is triggered since the suggestions DB is empty. Therefore the provider must
  // not be initialized until the test clock is set.
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);

  auto simple_test_clock = base::MakeUnique<base::SimpleTestClock>();
  base::SimpleTestClock* simple_test_clock_ptr = simple_test_clock.get();
  provider->SetClockForTesting(std::move(simple_test_clock));

  // Test that the preference is correctly initialized with the default value 0.
  EXPECT_EQ(
      0, pref_service()->GetInt64(prefs::kLastSuccessfulBackgroundFetchTime));

  WaitForSuggestionsProviderInitialization(provider.get());
  EXPECT_EQ(
      simple_test_clock_ptr->Now().ToInternalValue(),
      pref_service()->GetInt64(prefs::kLastSuccessfulBackgroundFetchTime));

  // Advance the time and check whether the time was updated correctly after the
  // background fetch.
  simple_test_clock_ptr->Advance(base::TimeDelta::FromHours(1));

  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_suggestions_fetcher(), FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  provider->RefetchInTheBackground(
      RemoteSuggestionsProvider::FetchStatusCallback());
  base::RunLoop().RunUntilIdle();
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"), base::nullopt);
  // TODO(jkrcal): Move together with the pref storage into the scheduler.
  EXPECT_EQ(
      simple_test_clock_ptr->Now().ToInternalValue(),
      pref_service()->GetInt64(prefs::kLastSuccessfulBackgroundFetchTime));
  // TODO(markusheintz): Add a test that simulates a browser restart once the
  // scheduler refactoring is done (crbug.com/672434).
}

TEST_F(RemoteSuggestionsProviderImplTest, CallsSchedulerWhenReady) {
  auto provider =
      MakeSuggestionsProviderWithoutInitializationWithStrictScheduler();

  // Should be called when becoming ready.
  EXPECT_CALL(*scheduler(), OnProviderActivated());
  WaitForSuggestionsProviderInitialization(provider.get());
}

TEST_F(RemoteSuggestionsProviderImplTest, CallsSchedulerOnError) {
  auto provider =
      MakeSuggestionsProviderWithoutInitializationWithStrictScheduler();

  // Should be called on error.
  EXPECT_CALL(*scheduler(), OnProviderDeactivated());
  provider->EnterState(RemoteSuggestionsProviderImpl::State::ERROR_OCCURRED);
}

TEST_F(RemoteSuggestionsProviderImplTest, CallsSchedulerWhenDisabled) {
  auto provider =
      MakeSuggestionsProviderWithoutInitializationWithStrictScheduler();

  // Should be called when becoming disabled. First deactivate and only after
  // that clear the suggestions so that they are not fetched again.
  {
    InSequence s;
    EXPECT_CALL(*scheduler(), OnProviderDeactivated());
    ASSERT_THAT(provider->ready(), Eq(false));
    EXPECT_CALL(*scheduler(), OnSuggestionsCleared());
  }
  provider->EnterState(RemoteSuggestionsProviderImpl::State::DISABLED);
}

TEST_F(RemoteSuggestionsProviderImplTest, CallsSchedulerWhenHistoryCleared) {
  auto provider =
      MakeSuggestionsProviderWithoutInitializationWithStrictScheduler();
  // Initiate the provider so that it is already READY.
  EXPECT_CALL(*scheduler(), OnProviderActivated());
  WaitForSuggestionsProviderInitialization(provider.get());

  // The scheduler should be notified of clearing the history.
  EXPECT_CALL(*scheduler(), OnHistoryCleared());
  provider->ClearHistory(GetDefaultCreationTime(), GetDefaultExpirationTime(),
                         base::Callback<bool(const GURL& url)>());
}

TEST_F(RemoteSuggestionsProviderImplTest, CallsSchedulerWhenSignedIn) {
  auto provider =
      MakeSuggestionsProviderWithoutInitializationWithStrictScheduler();
  // Initiate the provider so that it is already READY.
  EXPECT_CALL(*scheduler(), OnProviderActivated());
  WaitForSuggestionsProviderInitialization(provider.get());

  // The scheduler should be notified of clearing the history.
  EXPECT_CALL(*scheduler(), OnSuggestionsCleared());
  provider->OnStatusChanged(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN,
                            RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT);
}

TEST_F(RemoteSuggestionsProviderImplTest, CallsSchedulerWhenSignedOut) {
  auto provider =
      MakeSuggestionsProviderWithoutInitializationWithStrictScheduler();
  // Initiate the provider so that it is already READY.
  EXPECT_CALL(*scheduler(), OnProviderActivated());
  WaitForSuggestionsProviderInitialization(provider.get());

  // The scheduler should be notified of clearing the history.
  EXPECT_CALL(*scheduler(), OnSuggestionsCleared());
  provider->OnStatusChanged(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
                            RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN);
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldExcludeKnownSuggestionsWithoutTruncatingWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);
  WaitForSuggestionsProviderInitialization(provider.get());

  std::set<std::string> known_ids;
  for (int i = 0; i < 200; ++i) {
    known_ids.insert(base::IntToString(i));
  }

  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_suggestions_fetcher(),
              FetchSnippets(Field(&RequestParams::excluded_ids, known_ids), _));
  provider->Fetch(
      articles_category(), known_ids,
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldExcludeDismissedSuggestionsWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);
  WaitForSuggestionsProviderInitialization(provider.get());

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder().AddId("http://abc.com/"))
          .Build());
  ASSERT_TRUE(fetched_categories[0].suggestions[0]->is_complete());

  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  provider->DismissSuggestion(MakeArticleID("http://abc.com/"));

  std::set<std::string> expected_excluded_ids({"http://abc.com/"});
  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(
      *mock_suggestions_fetcher(),
      FetchSnippets(Field(&RequestParams::excluded_ids, expected_excluded_ids),
                    _));
  provider->Fetch(
      articles_category(), std::set<std::string>(),
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldTruncateExcludedDismissedSuggestionsWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);
  WaitForSuggestionsProviderInitialization(provider.get());

  std::vector<FetchedCategory> fetched_categories;
  FetchedCategoryBuilder category_builder;
  category_builder.SetCategory(articles_category());
  const int kSuggestionsCount = kMaxExcludedDismissedIds + 1;
  for (int i = 0; i < kSuggestionsCount; ++i) {
    category_builder.AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId(
        base::StringPrintf("http://abc.com/%d/", i)));
  }
  fetched_categories.push_back(category_builder.Build());

  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  // Dismiss them.
  for (int i = 0; i < kSuggestionsCount; ++i) {
    provider->DismissSuggestion(
        MakeArticleID(base::StringPrintf("http://abc.com/%d/", i)));
  }

  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_suggestions_fetcher(),
              FetchSnippets(Field(&RequestParams::excluded_ids,
                                  SizeIs(kMaxExcludedDismissedIds)),
                            _));
  provider->Fetch(
      articles_category(), std::set<std::string>(),
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldPreferLatestExcludedDismissedSuggestionsWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);
  WaitForSuggestionsProviderInitialization(provider.get());

  std::vector<FetchedCategory> fetched_categories;
  FetchedCategoryBuilder category_builder;
  category_builder.SetCategory(articles_category());
  const int kSuggestionsCount = kMaxExcludedDismissedIds + 1;
  for (int i = 0; i < kSuggestionsCount; ++i) {
    category_builder.AddSuggestionViaBuilder(RemoteSuggestionBuilder().AddId(
        base::StringPrintf("http://abc.com/%d/", i)));
  }
  fetched_categories.push_back(category_builder.Build());

  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  // Dismiss them in reverse order.
  std::string first_dismissed_suggestion_id;
  for (int i = kSuggestionsCount - 1; i >= 0; --i) {
    const std::string id = base::StringPrintf("http://abc.com/%d/", i);
    provider->DismissSuggestion(MakeArticleID(id));
    if (first_dismissed_suggestion_id.empty()) {
      first_dismissed_suggestion_id = id;
    }
  }

  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  // The oldest dismissed suggestion should be absent, because there are
  // |kMaxExcludedDismissedIds| newer dismissed suggestions.
  EXPECT_CALL(*mock_suggestions_fetcher(),
              FetchSnippets(Field(&RequestParams::excluded_ids,
                                  Not(Contains(first_dismissed_suggestion_id))),
                            _));
  provider->Fetch(
      articles_category(), std::set<std::string>(),
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldExcludeDismissedSuggestionsFromAllCategoriesWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);
  WaitForSuggestionsProviderInitialization(provider.get());

  // Add article suggestions.
  std::vector<FetchedCategory> fetched_categories;
  FetchedCategoryBuilder first_category_builder;
  first_category_builder.SetCategory(articles_category());
  const int kSuggestionsPerCategory = 2;
  for (int i = 0; i < kSuggestionsPerCategory; ++i) {
    first_category_builder.AddSuggestionViaBuilder(
        RemoteSuggestionBuilder().AddId(
            base::StringPrintf("http://abc.com/%d/", i)));
  }
  fetched_categories.push_back(first_category_builder.Build());
  // Add other category suggestions.
  FetchedCategoryBuilder second_category_builder;
  second_category_builder.SetCategory(other_category());
  for (int i = 0; i < kSuggestionsPerCategory; ++i) {
    second_category_builder.AddSuggestionViaBuilder(
        RemoteSuggestionBuilder().AddId(
            base::StringPrintf("http://other.com/%d/", i)));
  }
  fetched_categories.push_back(second_category_builder.Build());

  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  // Dismiss all suggestions.
  std::set<std::string> expected_excluded_ids;
  for (int i = 0; i < kSuggestionsPerCategory; ++i) {
    const std::string article_id = base::StringPrintf("http://abc.com/%d/", i);
    provider->DismissSuggestion(MakeArticleID(article_id));
    expected_excluded_ids.insert(article_id);
    const std::string other_id = base::StringPrintf("http://other.com/%d/", i);
    provider->DismissSuggestion(MakeOtherID(other_id));
    expected_excluded_ids.insert(other_id);
  }

  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  // Dismissed suggestions from all categories must be excluded (but not only
  // target category).
  EXPECT_CALL(
      *mock_suggestions_fetcher(),
      FetchSnippets(Field(&RequestParams::excluded_ids, expected_excluded_ids),
                    _));
  provider->Fetch(
      articles_category(), std::set<std::string>(),
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldPreferTargetCategoryExcludedDismissedSuggestionsWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/false);
  WaitForSuggestionsProviderInitialization(provider.get());

  // Add article suggestions.
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));

  for (int i = 0; i < kMaxExcludedDismissedIds; ++i) {
    fetched_categories[0].suggestions.push_back(CreateTestRemoteSuggestion(
        base::StringPrintf("http://abc.com/%d/", i)));
  }
  // Add other category suggestion.
  fetched_categories.push_back(FetchedCategory(
      other_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));
  fetched_categories[1].suggestions.push_back(
      CreateTestRemoteSuggestion("http://other.com/"));

  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  // Dismiss article suggestions first.
  for (int i = 0; i < kMaxExcludedDismissedIds; ++i) {
    provider->DismissSuggestion(
        MakeArticleID(base::StringPrintf("http://abc.com/%d/", i)));
  }

  // Then dismiss other category suggestion.
  provider->DismissSuggestion(MakeOtherID("http://other.com/"));

  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  // The other category dismissed suggestion should be absent, because the fetch
  // is for articles and there are |kMaxExcludedDismissedIds| dismissed
  // suggestions there.
  EXPECT_CALL(*mock_suggestions_fetcher(),
              FetchSnippets(Field(&RequestParams::excluded_ids,
                                  Not(Contains("http://other.com/"))),
                            _));
  provider->Fetch(
      articles_category(), std::set<std::string>(),
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldFetchNormallyWithoutPrefetchedPagesTracker) {
  EnableKeepingPrefetchedContentSuggestions(
      kMaxAdditionalPrefetchedSuggestions,
      kMaxAgeForAdditionalPrefetchedSuggestion);

  auto provider = MakeSuggestionsProvider();
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder())
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldKeepPrefetchedSuggestionsAfterFetchWhenEnabled) {
  EnableKeepingPrefetchedContentSuggestions(
      kMaxAdditionalPrefetchedSuggestions,
      kMaxAgeForAdditionalPrefetchedSuggestion);

  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/true);
  auto* mock_tracker = static_cast<StrictMock<MockPrefetchedPagesTracker>*>(
      prefetched_pages_tracker());
  WaitForSuggestionsProviderInitialization(provider.get());
  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://prefetched.com")
                                       .SetUrl("http://prefetched.com")
                                       .SetAmpUrl("http://amp.prefetched.com"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));

  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_tracker,
              PrefetchedOfflinePageExists(GURL("http://amp.prefetched.com")))
      .WillOnce(Return(true));
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://other.com")
                                       .SetUrl("http://other.com")
                                       .SetAmpUrl("http://amp.other.com"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  EXPECT_THAT(
      observer().SuggestionsForCategory(articles_category()),
      UnorderedElementsAre(
          Property(&ContentSuggestion::id,
                   MakeArticleID("http://prefetched.com")),
          Property(&ContentSuggestion::id, MakeArticleID("http://other.com"))));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldIgnoreNotPrefetchedSuggestionsAfterFetchWhenEnabled) {
  EnableKeepingPrefetchedContentSuggestions(
      kMaxAdditionalPrefetchedSuggestions,
      kMaxAgeForAdditionalPrefetchedSuggestion);

  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/true);
  auto* mock_tracker = static_cast<StrictMock<MockPrefetchedPagesTracker>*>(
      prefetched_pages_tracker());
  WaitForSuggestionsProviderInitialization(provider.get());
  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder()
                  .AddId("http://not_prefetched.com")
                  .SetUrl("http://not_prefetched.com")
                  .SetAmpUrl("http://amp.not_prefetched.com"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));

  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_tracker, PrefetchedOfflinePageExists(
                                 GURL("http://amp.not_prefetched.com")))
      .WillOnce(Return(false));
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://other.com")
                                       .SetUrl("http://other.com")
                                       .SetAmpUrl("http://amp.other.com"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              UnorderedElementsAre(Property(
                  &ContentSuggestion::id, MakeArticleID("http://other.com"))));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldLimitKeptPrefetchedSuggestionsAfterFetchWhenEnabled) {
  EnableKeepingPrefetchedContentSuggestions(
      kMaxAdditionalPrefetchedSuggestions,
      kMaxAgeForAdditionalPrefetchedSuggestion);

  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/true);
  auto* mock_tracker = static_cast<StrictMock<MockPrefetchedPagesTracker>*>(
      prefetched_pages_tracker());
  WaitForSuggestionsProviderInitialization(provider.get());

  const int prefetched_suggestions_count =
      2 * kMaxAdditionalPrefetchedSuggestions + 1;
  std::vector<FetchedCategory> fetched_categories;
  FetchedCategoryBuilder category_builder;
  category_builder.SetCategory(articles_category());
  for (int i = 0; i < prefetched_suggestions_count; ++i) {
    const std::string url = base::StringPrintf("http://prefetched.com/%d", i);
    category_builder.AddSuggestionViaBuilder(
        RemoteSuggestionBuilder().AddId(url).SetUrl(url).SetAmpUrl(
            base::StringPrintf("http://amp.prefetched.com/%d", i)));
  }
  fetched_categories.push_back(category_builder.Build());

  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(prefetched_suggestions_count));

  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  for (int i = 0; i < prefetched_suggestions_count; ++i) {
    EXPECT_CALL(*mock_tracker,
                PrefetchedOfflinePageExists(GURL(
                    base::StringPrintf("http://amp.prefetched.com/%d", i))))
        .WillOnce(Return(true));
  }
  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder()
                  .AddId("http://not_prefetched.com")
                  .SetUrl("http://not_prefetched.com")
                  .SetAmpUrl("http://amp.not_prefetched.com"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(kMaxAdditionalPrefetchedSuggestions + 1));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldMixInPrefetchedSuggestionsByScoreAfterFetchWhenEnabled) {
  EnableKeepingPrefetchedContentSuggestions(
      kMaxAdditionalPrefetchedSuggestions,
      kMaxAgeForAdditionalPrefetchedSuggestion);

  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/true);
  auto* mock_tracker = static_cast<StrictMock<MockPrefetchedPagesTracker>*>(
      prefetched_pages_tracker());
  WaitForSuggestionsProviderInitialization(provider.get());

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://prefetched.com/1")
                                       .SetUrl("http://prefetched.com/1")
                                       .SetAmpUrl("http://amp.prefetched.com/1")
                                       .SetScore(1))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://prefetched.com/3")
                                       .SetUrl("http://prefetched.com/3")
                                       .SetAmpUrl("http://amp.prefetched.com/3")
                                       .SetScore(3))
          .Build());
  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(2));

  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://new.com/2")
                                       .SetUrl("http://new.com/2")
                                       .SetAmpUrl("http://amp.new.com/2")
                                       .SetScore(2))
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://new.com/4")
                                       .SetUrl("http://new.com/4")
                                       .SetAmpUrl("http://amp.new.com/4")
                                       .SetScore(4))
          .Build());

  EXPECT_CALL(*mock_tracker,
              PrefetchedOfflinePageExists(GURL("http://amp.prefetched.com/1")))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker,
              PrefetchedOfflinePageExists(GURL("http://amp.prefetched.com/3")))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  EXPECT_THAT(
      observer().SuggestionsForCategory(articles_category()),
      ElementsAre(
          Property(&ContentSuggestion::id, MakeArticleID("http://new.com/4")),
          Property(&ContentSuggestion::id,
                   MakeArticleID("http://prefetched.com/3")),
          Property(&ContentSuggestion::id, MakeArticleID("http://new.com/2")),
          Property(&ContentSuggestion::id,
                   MakeArticleID("http://prefetched.com/1"))));
}

TEST_F(
    RemoteSuggestionsProviderImplTest,
    ShouldKeepMostRecentlyFetchedPrefetchedSuggestionsFirstAfterFetchWhenEnabled) {
  EnableKeepingPrefetchedContentSuggestions(
      kMaxAdditionalPrefetchedSuggestions,
      kMaxAgeForAdditionalPrefetchedSuggestion);

  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/true);
  auto* mock_tracker = static_cast<StrictMock<MockPrefetchedPagesTracker>*>(
      prefetched_pages_tracker());
  WaitForSuggestionsProviderInitialization(provider.get());

  std::vector<FetchedCategory> fetched_categories;
  const int prefetched_suggestions_count =
      2 * kMaxAdditionalPrefetchedSuggestions + 1;
  for (int i = 0; i < prefetched_suggestions_count; ++i) {
    const std::string url = base::StringPrintf("http://prefetched.com/%d", i);
    fetched_categories.push_back(
        FetchedCategoryBuilder()
            .SetCategory(articles_category())
            .AddSuggestionViaBuilder(
                RemoteSuggestionBuilder().AddId(url).SetUrl(url).SetAmpUrl(
                    base::StringPrintf("http://amp.prefetched.com/%d", i)))
            .Build());
    EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
    if (i != 0) {
      EXPECT_CALL(*mock_tracker,
                  PrefetchedOfflinePageExists(GURL(base::StringPrintf(
                      "http://amp.prefetched.com/%d", i - 1))))
          .WillRepeatedly(Return(true));
    }

    FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                          Status(StatusCode::SUCCESS, "message"),
                          std::move(fetched_categories));
  }

  const std::vector<ContentSuggestion>& actual_suggestions =
      observer().SuggestionsForCategory(articles_category());

  ASSERT_THAT(actual_suggestions,
              SizeIs(kMaxAdditionalPrefetchedSuggestions + 1));

  int matched = 0;
  for (int i = prefetched_suggestions_count - 1; i >= 0; --i) {
    EXPECT_THAT(actual_suggestions,
                Contains(Property(&ContentSuggestion::id,
                                  MakeArticleID(base::StringPrintf(
                                      "http://prefetched.com/%d", i)))));
    ++matched;
    if (matched == kMaxAdditionalPrefetchedSuggestions + 1) {
      break;
    }
  }
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldNotKeepStalePrefetchedSuggestionsAfterFetchWhenEnabled) {
  EnableKeepingPrefetchedContentSuggestions(
      kMaxAdditionalPrefetchedSuggestions,
      kMaxAgeForAdditionalPrefetchedSuggestion);

  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/true);
  auto* mock_tracker = static_cast<StrictMock<MockPrefetchedPagesTracker>*>(
      prefetched_pages_tracker());

  auto wrapped_provider_clock = base::MakeUnique<base::SimpleTestClock>();
  base::SimpleTestClock* provider_clock = wrapped_provider_clock.get();
  provider->SetClockForTesting(std::move(wrapped_provider_clock));

  provider_clock->SetNow(GetDefaultCreationTime() +
                         base::TimeDelta::FromHours(10));

  WaitForSuggestionsProviderInitialization(provider.get());
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder()
                  .AddId("http://prefetched.com")
                  .SetUrl("http://prefetched.com")
                  .SetAmpUrl("http://amp.prefetched.com")
                  .SetFetchDate(provider_clock->Now())
                  .SetPublishDate(GetDefaultCreationTime()))
          .Build());
  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));

  provider_clock->Advance(kMaxAgeForAdditionalPrefetchedSuggestion -
                          base::TimeDelta::FromSeconds(1));

  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder()
                  .AddId("http://other.com")
                  .SetUrl("http://other.com")
                  .SetAmpUrl("http://amp.other.com")
                  .SetFetchDate(provider_clock->Now())
                  .SetPublishDate(GetDefaultCreationTime()))
          .Build());
  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_tracker,
              PrefetchedOfflinePageExists(GURL("http://amp.prefetched.com")))
      .WillOnce(Return(true));
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(2));

  provider_clock->Advance(base::TimeDelta::FromSeconds(2));

  fetched_categories.clear();
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(
              RemoteSuggestionBuilder()
                  .AddId("http://other.com")
                  .SetUrl("http://other.com")
                  .SetAmpUrl("http://amp.other.com")
                  .SetFetchDate(provider_clock->Now())
                  .SetPublishDate(GetDefaultCreationTime()))
          .Build());
  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_tracker,
              PrefetchedOfflinePageExists(GURL("http://amp.prefetched.com")))
      .WillOnce(Return(true));
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              ElementsAre(Property(&ContentSuggestion::id,
                                   MakeArticleID("http://other.com"))));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldWaitForPrefetchedPagesTrackerInitialization) {
  EnableKeepingPrefetchedContentSuggestions(
      kMaxAdditionalPrefetchedSuggestions,
      kMaxAgeForAdditionalPrefetchedSuggestion);

  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_prefetched_pages_tracker=*/true);
  auto* mock_tracker = static_cast<StrictMock<MockPrefetchedPagesTracker>*>(
      prefetched_pages_tracker());
  WaitForSuggestionsProviderInitialization(provider.get());

  base::OnceCallback<void()> initialization_completed_callback;
  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_tracker, AddInitializationCompletedCallback(_))
      .WillOnce(MoveFirstArgumentPointeeTo(&initialization_completed_callback));
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(
      FetchedCategoryBuilder()
          .SetCategory(articles_category())
          .AddSuggestionViaBuilder(RemoteSuggestionBuilder()
                                       .AddId("http://prefetched.com")
                                       .SetUrl("http://prefetched.com")
                                       .SetAmpUrl("http://amp.prefetched.com"))
          .Build());
  FetchTheseSuggestions(provider.get(), /*interactive_request=*/true,
                        Status(StatusCode::SUCCESS, "message"),
                        std::move(fetched_categories));
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(0));

  EXPECT_CALL(*mock_tracker, IsInitialized()).WillRepeatedly(Return(true));
  std::move(initialization_completed_callback).Run();
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
}

}  // namespace ntp_snippets
