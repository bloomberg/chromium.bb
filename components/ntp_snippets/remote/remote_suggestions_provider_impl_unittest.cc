// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"

#include <memory>
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
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
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
using testing::Return;
using testing::SaveArg;
using testing::SizeIs;
using testing::StartsWith;
using testing::StrictMock;
using testing::WithArgs;

namespace ntp_snippets {

namespace {

ACTION_P(MoveSecondArgumentPointeeTo, ptr) {
  // 0-based indexation.
  *ptr = std::move(*arg1);
}

MATCHER_P(IdEq, value, "") {
  return arg->id() == value;
}

MATCHER_P(IdWithinCategoryEq, expected_id, "") {
  return arg.id().id_within_category() == expected_id;
}

MATCHER_P(HasCode, code, "") {
  return arg.code == code;
}

const int kMaxExcludedDismissedIds = 100;

const base::Time::Exploded kDefaultCreationTime = {2015, 11, 4, 25, 13, 46, 45};
const char kTestContentSuggestionsServerEndpoint[] =
    "https://alpha-chromecontentsuggestions-pa.sandbox.googleapis.com/v1/"
    "suggestions/fetch";
const char kAPIKey[] = "fakeAPIkey";
const char kTestContentSuggestionsServerWithAPIKey[] =
    "https://alpha-chromecontentsuggestions-pa.sandbox.googleapis.com/v1/"
    "suggestions/fetch?key=fakeAPIkey";

const char kSuggestionUrl[] = "http://localhost/foobar";
const char kSuggestionTitle[] = "Title";
const char kSuggestionText[] = "Suggestion";
const char kSuggestionSalientImage[] = "http://localhost/salient_image";
const char kSuggestionPublisherName[] = "Foo News";
const char kSuggestionAmpUrl[] = "http://localhost/amp";

const char kSuggestionUrl2[] = "http://foo.com/bar";

const char kTestJsonDefaultCategoryTitle[] = "Some title";

const int kUnknownRemoteCategoryId = 1234;

base::Time GetDefaultCreationTime() {
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kDefaultCreationTime, &out_time));
  return out_time;
}

base::Time GetDefaultExpirationTime() {
  return base::Time::Now() + base::TimeDelta::FromHours(1);
}

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

std::string GetCategoryJson(const std::vector<std::string>& suggestions,
                            int remote_category_id,
                            const std::string& category_title) {
  return base::StringPrintf(
      "  {\n"
      "    \"id\": %d,\n"
      "    \"localizedTitle\": \"%s\",\n"
      "    \"suggestions\": [%s]\n"
      "  }\n",
      remote_category_id, category_title.c_str(),
      base::JoinString(suggestions, ", ").c_str());
}

class MultiCategoryJsonBuilder {
 public:
  MultiCategoryJsonBuilder() {}

  MultiCategoryJsonBuilder& AddCategoryWithCustomTitle(
      const std::vector<std::string>& suggestions,
      int remote_category_id,
      const std::string& category_title) {
    category_json_.push_back(
        GetCategoryJson(suggestions, remote_category_id, category_title));
    return *this;
  }

  MultiCategoryJsonBuilder& AddCategory(
      const std::vector<std::string>& suggestions,
      int remote_category_id) {
    return AddCategoryWithCustomTitle(
        suggestions, remote_category_id,
        "Title" + base::IntToString(remote_category_id));
  }

  std::string Build() {
    return base::StringPrintf(
        "{\n"
        "  \"categories\": [\n"
        "%s\n"
        "  ]\n"
        "}\n",
        base::JoinString(category_json_, "  ,\n").c_str());
  }

 private:
  std::vector<std::string> category_json_;
};

// TODO(vitaliii): Remove these convenience functions as they do not provide
// that much value and add additional redirections obscuring the code.
std::string GetTestJson(const std::vector<std::string>& suggestions,
                        const std::string& category_title) {
  return MultiCategoryJsonBuilder()
      .AddCategoryWithCustomTitle(suggestions, /*remote_category_id=*/1,
                                  category_title)
      .Build();
}

std::string GetTestJson(const std::vector<std::string>& suggestions) {
  return GetTestJson(suggestions, kTestJsonDefaultCategoryTitle);
}

std::string FormatTime(const base::Time& t) {
  base::Time::Exploded x;
  t.UTCExplode(&x);
  return base::StringPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ", x.year, x.month,
                            x.day_of_month, x.hour, x.minute, x.second);
}

std::string GetSuggestionWithUrlAndTimesAndSource(
    const std::vector<std::string>& ids,
    const std::string& url,
    const base::Time& creation_time,
    const base::Time& expiry_time,
    const std::string& publisher,
    const std::string& amp_url) {
  const std::string ids_string = base::JoinString(ids, "\",\n      \"");
  return base::StringPrintf(
      "{\n"
      "      \"ids\": [\n"
      "        \"%s\"\n"
      "      ],\n"
      "      \"title\": \"%s\",\n"
      "      \"snippet\": \"%s\",\n"
      "      \"fullPageUrl\": \"%s\",\n"
      "      \"creationTime\": \"%s\",\n"
      "      \"expirationTime\": \"%s\",\n"
      "      \"attribution\": \"%s\",\n"
      "      \"imageUrl\": \"%s\",\n"
      "      \"ampUrl\": \"%s\"\n"
      "    }",
      ids_string.c_str(), kSuggestionTitle, kSuggestionText, url.c_str(),
      FormatTime(creation_time).c_str(), FormatTime(expiry_time).c_str(),
      publisher.c_str(), kSuggestionSalientImage, amp_url.c_str());
}

std::string GetSuggestionWithSources(const std::string& source_url,
                                     const std::string& publisher,
                                     const std::string& amp_url) {
  return GetSuggestionWithUrlAndTimesAndSource(
      {kSuggestionUrl}, source_url, GetDefaultCreationTime(),
      GetDefaultExpirationTime(), publisher, amp_url);
}

std::string GetSuggestionWithUrlAndTimes(
    const std::string& url,
    const base::Time& content_creation_time,
    const base::Time& expiry_time) {
  return GetSuggestionWithUrlAndTimesAndSource(
      {url}, url, content_creation_time, expiry_time, kSuggestionPublisherName,
      kSuggestionAmpUrl);
}

std::string GetSuggestionWithTimes(const base::Time& content_creation_time,
                                   const base::Time& expiry_time) {
  return GetSuggestionWithUrlAndTimes(kSuggestionUrl, content_creation_time,
                                      expiry_time);
}

std::string GetSuggestionWithUrl(const std::string& url) {
  return GetSuggestionWithUrlAndTimes(url, GetDefaultCreationTime(),
                                      GetDefaultExpirationTime());
}

std::string GetSuggestion() {
  return GetSuggestionWithUrlAndTimes(kSuggestionUrl, GetDefaultCreationTime(),
                                      GetDefaultExpirationTime());
}

std::string GetSuggestionN(int n) {
  return GetSuggestionWithUrlAndTimes(
      base::StringPrintf("%s/%d", kSuggestionUrl, n), GetDefaultCreationTime(),
      GetDefaultExpirationTime());
}

std::string GetExpiredSuggestion() {
  return GetSuggestionWithTimes(GetDefaultCreationTime(), base::Time::Now());
}

std::string GetInvalidSuggestion() {
  std::string json_str = GetSuggestion();
  // Make the json invalid by removing the final closing brace.
  return json_str.substr(0, json_str.size() - 1);
}

std::string GetIncompleteSuggestion() {
  std::string json_str = GetSuggestion();
  // Rename the "url" entry. The result is syntactically valid json that will
  // fail to parse as suggestions.
  size_t pos = json_str.find("\"fullPageUrl\"");
  if (pos == std::string::npos) {
    NOTREACHED();
    return std::string();
  }
  json_str[pos + 1] = 'x';
  return json_str;
}

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

void ParseJson(const std::string& json,
               const SuccessCallback& success_callback,
               const ErrorCallback& error_callback) {
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value) {
    success_callback.Run(std::move(value));
  } else {
    error_callback.Run(json_reader.GetErrorMessage());
  }
}

// Factory for FakeURLFetcher objects that always generate errors.
class FailingFakeURLFetcherFactory : public net::URLFetcherFactory {
 public:
  std::unique_ptr<net::URLFetcher> CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d,
      net::NetworkTrafficAnnotationTag traffic_annotation) override {
    return base::MakeUnique<net::FakeURLFetcher>(
        url, d, /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
        net::URLRequestStatus::FAILED);
  }
};

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
  MOCK_METHOD0(OnNTPOpened, void());
  MOCK_METHOD0(OnPersistentSchedulerWakeUp, void());
  MOCK_METHOD0(RescheduleFetching, void());
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

}  // namespace

class RemoteSuggestionsProviderImplTest : public ::testing::Test {
 public:
  RemoteSuggestionsProviderImplTest()
      : params_manager_(ntp_snippets::kArticleSuggestionsFeature.name,
                        {{"content_suggestions_backend",
                          kTestContentSuggestionsServerEndpoint}},
                        {ntp_snippets::kArticleSuggestionsFeature.name}),
        fake_url_fetcher_factory_(
            /*default_factory=*/&failing_url_fetcher_factory_),
        test_url_(kTestContentSuggestionsServerWithAPIKey),
        category_ranker_(base::MakeUnique<ConstantCategoryRanker>()),
        user_classifier_(/*pref_service=*/nullptr,
                         base::MakeUnique<base::DefaultClock>()),
        suggestions_fetcher_(nullptr),
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
  std::unique_ptr<RemoteSuggestionsProviderImpl> MakeSuggestionsProvider(
      bool set_empty_response = true) {
    auto provider = MakeSuggestionsProviderWithoutInitialization(
        /*use_mock_suggestions_fetcher=*/false);
    WaitForSuggestionsProviderInitialization(provider.get(),
                                             set_empty_response);
    return provider;
  }

  // TODO(vitaliii): Rewrite tests and always use mock suggestions fetcher.
  std::unique_ptr<RemoteSuggestionsProviderImpl>
  MakeSuggestionsProviderWithoutInitialization(
      bool use_mock_suggestions_fetcher) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    scoped_refptr<net::TestURLRequestContextGetter> request_context_getter =
        new net::TestURLRequestContextGetter(task_runner.get());

    utils_.ResetSigninManager();
    std::unique_ptr<RemoteSuggestionsFetcher> suggestions_fetcher;
    if (use_mock_suggestions_fetcher) {
      suggestions_fetcher =
          base::MakeUnique<StrictMock<MockRemoteSuggestionsFetcher>>();
    } else {
      suggestions_fetcher = base::MakeUnique<RemoteSuggestionsFetcherImpl>(
          utils_.fake_signin_manager(), /*token_service=*/nullptr,
          std::move(request_context_getter), utils_.pref_service(), nullptr,
          base::Bind(&ParseJson),
          GetFetchEndpoint(version_info::Channel::STABLE), kAPIKey,
          &user_classifier_);
    }
    suggestions_fetcher_ = suggestions_fetcher.get();

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
        scheduler_.get(), std::move(suggestions_fetcher),
        std::move(image_fetcher), std::move(database),
        base::MakeUnique<RemoteSuggestionsStatusService>(
            utils_.fake_signin_manager(), utils_.pref_service(),
            std::string()));
  }

  std::unique_ptr<RemoteSuggestionsProviderImpl>
  MakeSuggestionsProviderWithoutInitializationWithStrictScheduler() {
    scheduler_ = base::MakeUnique<StrictMock<MockScheduler>>();
    return MakeSuggestionsProviderWithoutInitialization(
        /*use_mock_suggestions_fetcher=*/false);
  }

  void WaitForSuggestionsProviderInitialization(
      RemoteSuggestionsProviderImpl* provider,
      bool set_empty_response) {
    EXPECT_EQ(RemoteSuggestionsProviderImpl::State::NOT_INITED,
              provider->state_);

    // Add an initial fetch response, as the provider tries to fetch when there
    // is nothing in the DB.
    if (set_empty_response) {
      SetUpFetchResponse(GetTestJson(std::vector<std::string>()));
    }

    // TODO(treib): Find a better way to wait for initialization to finish.
    base::RunLoop().RunUntilIdle();
    EXPECT_NE(RemoteSuggestionsProviderImpl::State::NOT_INITED,
              provider->state_);
  }

  void ResetSuggestionsProvider(
      std::unique_ptr<RemoteSuggestionsProviderImpl>* provider,
      bool set_empty_response) {
    provider->reset();
    observer_.reset();
    *provider = MakeSuggestionsProvider(set_empty_response);
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
  const GURL& test_url() { return test_url_; }
  FakeContentSuggestionsProviderObserver& observer() { return *observer_; }
  RemoteSuggestionsFetcher* suggestions_fetcher() {
    return suggestions_fetcher_;
  }
  // TODO(tschumann): Make this a strict-mock. We want to avoid unneccesary
  // network requests.
  NiceMock<MockImageFetcher>* image_fetcher() { return image_fetcher_; }
  FakeImageDecoder* image_decoder() { return &image_decoder_; }
  PrefService* pref_service() { return utils_.pref_service(); }
  RemoteSuggestionsDatabase* database() { return database_; }
  MockScheduler* scheduler() { return scheduler_.get(); }

  // Provide the json to be returned by the fake fetcher.
  void SetUpFetchResponse(const std::string& json) {
    fake_url_fetcher_factory_.SetFakeResponse(test_url_, json, net::HTTP_OK,
                                              net::URLRequestStatus::SUCCESS);
  }

  // Have the fake fetcher fail due to a HTTP error like a 404.
  void SetUpHttpError() {
    fake_url_fetcher_factory_.SetFakeResponse(test_url_, /*json=*/std::string(),
                                              net::HTTP_NOT_FOUND,
                                              net::URLRequestStatus::SUCCESS);
  }

  void FetchSuggestions(
      RemoteSuggestionsProviderImpl* provider,
      bool interactive_request,
      const RemoteSuggestionsProvider::FetchStatusCallback& callback) {
    provider->FetchSuggestions(interactive_request, callback);
  }

  void LoadFromJSONString(RemoteSuggestionsProviderImpl* provider,
                          const std::string& json) {
    SetUpFetchResponse(json);
    provider->FetchSuggestions(
        /*interactive_request=*/true,
        RemoteSuggestionsProvider::FetchStatusCallback());
    base::RunLoop().RunUntilIdle();
  }

  void LoadMoreFromJSONString(RemoteSuggestionsProviderImpl* provider,
                              const Category& category,
                              const std::string& json,
                              const std::set<std::string>& known_ids,
                              FetchDoneCallback callback) {
    SetUpFetchResponse(json);
    EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    provider->Fetch(category, known_ids, callback);
    base::RunLoop().RunUntilIdle();
  }

  void SetOrderNewRemoteCategoriesBasedOnArticlesCategoryParam(bool value) {
    // params_manager supports only one
    // |SetVariationParamsWithFeatureAssociations| at a time, so we clear
    // previous settings first and then set everything we need.
    params_manager_.ClearAllVariationParams();
    params_manager_.SetVariationParamsWithFeatureAssociations(
        kArticleSuggestionsFeature.name,
        {{"order_new_remote_categories_based_on_articles_category",
          value ? "true" : "false"},
         {"content_suggestions_backend",
          kTestContentSuggestionsServerEndpoint}},
        {kArticleSuggestionsFeature.name});
  }

 private:
  variations::testing::VariationParamsManager params_manager_;
  test::RemoteSuggestionsTestUtils utils_;
  base::MessageLoop message_loop_;
  FailingFakeURLFetcherFactory failing_url_fetcher_factory_;
  // Instantiation of factory automatically sets itself as URLFetcher's factory.
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  const GURL test_url_;
  std::unique_ptr<CategoryRanker> category_ranker_;
  UserClassifier user_classifier_;
  std::unique_ptr<FakeContentSuggestionsProviderObserver> observer_;
  RemoteSuggestionsFetcher* suggestions_fetcher_;
  NiceMock<MockImageFetcher>* image_fetcher_;
  FakeImageDecoder image_decoder_;
  std::unique_ptr<MockScheduler> scheduler_;

  base::ScopedTempDir database_dir_;
  RemoteSuggestionsDatabase* database_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsProviderImplTest);
};

TEST_F(RemoteSuggestionsProviderImplTest, Full) {
  std::string json_str(GetTestJson({GetSuggestion()}));

  auto provider = MakeSuggestionsProvider();

  LoadFromJSONString(provider.get(), json_str);

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
  auto provider = MakeSuggestionsProvider(/*set_empty_response=*/false);

  // The articles category should be there by default, and have a title.
  CategoryInfo info_before = provider->GetCategoryInfo(articles_category());
  ASSERT_THAT(info_before.title(), Not(IsEmpty()));
  ASSERT_THAT(info_before.title(), Not(Eq(test_default_title)));
  EXPECT_THAT(info_before.additional_action(),
              Eq(ContentSuggestionsAdditionalAction::FETCH));
  EXPECT_THAT(info_before.show_if_empty(), Eq(true));

  std::string json_str_with_title(GetTestJson({GetSuggestion()}));
  LoadFromJSONString(provider.get(), json_str_with_title);

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
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSuggestionN(0)}, /*remote_category_id=*/1)
          .AddCategory({GetSuggestionN(1)}, /*remote_category_id=*/2)
          .Build();
  LoadFromJSONString(provider.get(), json_str);

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
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSuggestionN(0)}, /*remote_category_id=*/1)
          .AddCategory({GetSuggestionN(1)}, kUnknownRemoteCategoryId)
          .Build();
  // Load data with multiple categories so that a new experimental category gets
  // registered.
  LoadFromJSONString(provider.get(), json_str);

  CategoryInfo info = provider->GetCategoryInfo(unknown_category());
  EXPECT_THAT(info.additional_action(),
              Eq(ContentSuggestionsAdditionalAction::NONE));
  EXPECT_THAT(info.show_if_empty(), Eq(false));
}

TEST_F(RemoteSuggestionsProviderImplTest, AddRemoteCategoriesToCategoryRanker) {
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSuggestionN(0)}, /*remote_category_id=*/11)
          .AddCategory({GetSuggestionN(1)}, /*remote_category_id=*/13)
          .AddCategory({GetSuggestionN(2)}, /*remote_category_id=*/12)
          .Build();
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
  auto provider = MakeSuggestionsProvider(/*set_empty_response=*/false);
  LoadFromJSONString(provider.get(), json_str);
}

TEST_F(RemoteSuggestionsProviderImplTest,
       AddRemoteCategoriesToCategoryRankerRelativeToArticles) {
  SetOrderNewRemoteCategoriesBasedOnArticlesCategoryParam(true);
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSuggestionN(0)}, /*remote_category_id=*/14)
          .AddCategory({GetSuggestionN(1)}, /*remote_category_id=*/13)
          .AddCategory({GetSuggestionN(2)}, /*remote_category_id=*/1)
          .AddCategory({GetSuggestionN(3)}, /*remote_category_id=*/12)
          .AddCategory({GetSuggestionN(4)}, /*remote_category_id=*/11)
          .Build();
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
  auto provider = MakeSuggestionsProvider(/*set_empty_response=*/false);
  LoadFromJSONString(provider.get(), json_str);
}

TEST_F(
    RemoteSuggestionsProviderImplTest,
    AddRemoteCategoriesToCategoryRankerRelativeToArticlesWithArticlesAbsent) {
  SetOrderNewRemoteCategoriesBasedOnArticlesCategoryParam(true);
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSuggestionN(0)}, /*remote_category_id=*/11)
          .Build();

  EXPECT_CALL(*raw_mock_ranker, InsertCategoryBeforeIfNecessary(_, _)).Times(0);
  EXPECT_CALL(*raw_mock_ranker,
              AppendCategoryIfNecessary(Category::FromRemoteCategory(11)));
  auto provider = MakeSuggestionsProvider(/*set_empty_response=*/false);
  LoadFromJSONString(provider.get(), json_str);
}

TEST_F(RemoteSuggestionsProviderImplTest, PersistCategoryInfos) {
  auto provider = MakeSuggestionsProvider();
  // TODO(vitaliii): Use |articles_category()| instead of constant ID below.
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategoryWithCustomTitle(
              {GetSuggestionN(0)}, /*remote_category_id=*/1, "Articles for You")
          .AddCategoryWithCustomTitle({GetSuggestionN(1)},
                                      kUnknownRemoteCategoryId, "Other Things")
          .Build();
  LoadFromJSONString(provider.get(), json_str);

  ASSERT_EQ(observer().StatusForCategory(articles_category()),
            CategoryStatus::AVAILABLE);
  ASSERT_EQ(observer().StatusForCategory(unknown_category()),
            CategoryStatus::AVAILABLE);

  CategoryInfo info_articles_before =
      provider->GetCategoryInfo(articles_category());
  CategoryInfo info_unknown_before =
      provider->GetCategoryInfo(unknown_category());

  // Recreate the provider to simulate a Chrome restart.
  ResetSuggestionsProvider(&provider, /*set_empty_response=*/true);

  // The categories should have been restored.
  ASSERT_NE(observer().StatusForCategory(articles_category()),
            CategoryStatus::NOT_PROVIDED);
  ASSERT_NE(observer().StatusForCategory(unknown_category()),
            CategoryStatus::NOT_PROVIDED);

  EXPECT_EQ(observer().StatusForCategory(articles_category()),
            CategoryStatus::AVAILABLE);
  EXPECT_EQ(observer().StatusForCategory(unknown_category()),
            CategoryStatus::AVAILABLE);

  CategoryInfo info_articles_after =
      provider->GetCategoryInfo(articles_category());
  CategoryInfo info_unknown_after =
      provider->GetCategoryInfo(unknown_category());

  EXPECT_EQ(info_articles_before.title(), info_articles_after.title());
  EXPECT_EQ(info_unknown_before.title(), info_unknown_after.title());
}

TEST_F(RemoteSuggestionsProviderImplTest, PersistRemoteCategoryOrder) {
  // We create a provider with a normal ranker to store the order.
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSuggestionN(0)}, /*remote_category_id=*/11)
          .AddCategory({GetSuggestionN(1)}, /*remote_category_id=*/13)
          .AddCategory({GetSuggestionN(2)}, /*remote_category_id=*/12)
          .Build();
  auto provider = MakeSuggestionsProvider(/*set_empty_response=*/false);
  LoadFromJSONString(provider.get(), json_str);

  // We manually recreate the provider to simulate Chrome restart and enforce a
  // mock ranker. The response is cleared to ensure that the order is not
  // fetched.
  SetUpFetchResponse("");
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));
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
  ResetSuggestionsProvider(&provider, /*set_empty_response=*/false);
}

TEST_F(RemoteSuggestionsProviderImplTest, PersistSuggestions) {
  auto provider = MakeSuggestionsProvider();
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSuggestionN(0)}, /*remote_category_id=*/1)
          .AddCategory({GetSuggestionN(2)}, /*remote_category_id=*/2)
          .Build();
  LoadFromJSONString(provider.get(), json_str);

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));

  // Recreate the provider to simulate a Chrome restart.
  ResetSuggestionsProvider(&provider, /*set_empty_response=*/true);

  // The suggestions in both categories should have been restored.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  EXPECT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, DontNotifyIfNotAvailable) {
  // Get some suggestions into the database.
  auto provider = MakeSuggestionsProvider();
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSuggestionN(0)},
                       /*remote_category_id=*/1)
          .AddCategory({GetSuggestionN(1)}, /*remote_category_id=*/2)
          .Build();
  LoadFromJSONString(provider.get(), json_str);

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));

  provider.reset();

  // Set the pref that disables remote suggestions.
  pref_service()->SetBoolean(prefs::kEnableSnippets, false);

  // Recreate the provider to simulate a Chrome start.
  ResetSuggestionsProvider(&provider, /*set_empty_response=*/true);

  ASSERT_THAT(RemoteSuggestionsProviderImpl::State::DISABLED,
              Eq(provider->state_));

  // Now the observer should not have received any suggestions.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              IsEmpty());
  EXPECT_THAT(observer().SuggestionsForCategory(other_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, Clear) {
  auto provider = MakeSuggestionsProvider();

  std::string json_str(GetTestJson({GetSuggestion()}));

  LoadFromJSONString(provider.get(), json_str);
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));

  provider->ClearCachedSuggestions(articles_category());
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, ReplaceSuggestions) {
  auto provider = MakeSuggestionsProvider();

  std::string first("http://first");
  LoadFromJSONString(provider.get(),
                     GetTestJson({GetSuggestionWithUrl(first)}));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              ElementsAre(IdEq(first)));

  std::string second("http://second");
  LoadFromJSONString(provider.get(),
                     GetTestJson({GetSuggestionWithUrl(second)}));
  // The suggestions loaded last replace all that was loaded previously.
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              ElementsAre(IdEq(second)));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldResolveFetchedSuggestionThumbnail) {
  auto provider = MakeSuggestionsProvider();

  LoadFromJSONString(provider.get(),
                     GetTestJson({GetSuggestionWithUrl("http://first")}));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              ElementsAre(IdEq("http://first")));

  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  ServeImageCallback serve_one_by_one_image_callback =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke(CreateFunctor(serve_one_by_one_image_callback))));

  gfx::Image image = FetchImage(provider.get(), MakeArticleID("http://first"));
  ASSERT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());
}

TEST_F(RemoteSuggestionsProviderImplTest, ShouldFetchMore) {
  auto provider = MakeSuggestionsProvider();

  LoadFromJSONString(provider.get(),
                     GetTestJson({GetSuggestionWithUrl("http://first")}));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              ElementsAre(IdEq("http://first")));

  auto expect_only_second_suggestion_received =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        EXPECT_THAT(suggestions, SizeIs(1));
        EXPECT_THAT(suggestions[0].id().id_within_category(),
                    Eq("http://second"));
      });
  LoadMoreFromJSONString(provider.get(), articles_category(),
                         GetTestJson({GetSuggestionWithUrl("http://second")}),
                         /*known_ids=*/std::set<std::string>(),
                         expect_only_second_suggestion_received);
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldResolveFetchedMoreSuggestionThumbnail) {
  auto provider = MakeSuggestionsProvider();

  auto assert_only_first_suggestion_received =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        ASSERT_THAT(suggestions, SizeIs(1));
        ASSERT_THAT(suggestions[0].id().id_within_category(),
                    Eq("http://first"));
      });
  LoadMoreFromJSONString(provider.get(), articles_category(),
                         GetTestJson({GetSuggestionWithUrl("http://first")}),
                         /*known_ids=*/std::set<std::string>(),
                         assert_only_first_suggestion_received);

  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  ServeImageCallback serve_one_by_one_image_callback =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke(CreateFunctor(serve_one_by_one_image_callback))));

  gfx::Image image = FetchImage(provider.get(), MakeArticleID("http://first"));
  ASSERT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());
}

// Imagine that we have surfaces A and B. The user fetches more in A, this
// should not add any suggestions to B.
TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldNotChangeSuggestionsInOtherSurfacesWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_suggestions_fetcher=*/true);
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);

  // Fetch a suggestion.
  auto* mock_fetcher = static_cast<StrictMock<MockRemoteSuggestionsFetcher>*>(
      suggestions_fetcher());
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));
  fetched_categories[0].suggestions.push_back(
      CreateTestRemoteSuggestion("http://old.com/"));

  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_fetcher, FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  FetchSuggestions(provider.get(), /*interactive_request=*/true,
                   RemoteSuggestionsProvider::FetchStatusCallback());
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"),
           std::move(fetched_categories));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              ElementsAre(IdWithinCategoryEq("http://old.com/")));

  // Now fetch more, but first prepare a response.
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));
  fetched_categories[0].suggestions.push_back(
      CreateTestRemoteSuggestion("http://fetched-more.com/"));

  // The surface issuing the fetch more gets response via callback.
  auto assert_receiving_one_new_suggestion =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        ASSERT_THAT(suggestions, SizeIs(1));
        ASSERT_THAT(suggestions[0],
                    IdWithinCategoryEq("http://fetched-more.com/"));
      });
  EXPECT_CALL(*mock_fetcher, FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  provider->Fetch(articles_category(),
                  /*known_suggestion_ids=*/{"http://old.com/"},
                  assert_receiving_one_new_suggestion);
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"),
           std::move(fetched_categories));
  // Other surfaces should remain the same.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              ElementsAre(IdWithinCategoryEq("http://old.com/")));
}

// Imagine that we have surfaces A and B. The user fetches more in A. This
// should not affect the next fetch more in B, i.e. assuming the same server
// response the same suggestions must be fetched in B if the user fetches more
// there as well.
TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldNotAffectFetchMoreInOtherSurfacesWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_suggestions_fetcher=*/true);
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);
  auto* mock_fetcher = static_cast<StrictMock<MockRemoteSuggestionsFetcher>*>(
      suggestions_fetcher());

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
        ASSERT_THAT(suggestions[0],
                    IdWithinCategoryEq("http://fetched-more.com/"));
      });
  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_fetcher, FetchSnippets(_, _))
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
        EXPECT_THAT(suggestions[0],
                    IdWithinCategoryEq("http://fetched-more.com/"));
      });
  // The provider should not ask the fetcher to exclude the suggestion fetched
  // more on A.
  EXPECT_CALL(*mock_fetcher,
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
  auto provider = MakeSuggestionsProvider(/*set_empty_response=*/false);
  // First get suggestions into the archived state which happens through
  // subsequent fetches. Then we verify the entries are gone from the 'archived'
  // state by trying to load their images (and we shouldn't even know the URLs
  // anymore).
  LoadFromJSONString(provider.get(),
                     GetTestJson({GetSuggestionWithUrl("http://id-1"),
                                  GetSuggestionWithUrl("http://id-2")}));
  LoadFromJSONString(provider.get(),
                     GetTestJson({GetSuggestionWithUrl("http://new-id-1"),
                                  GetSuggestionWithUrl("http://new-id-2")}));
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
      /*use_mock_suggestions_fetcher=*/false);
  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  provider->Fetch(articles_category(), std::set<std::string>(),
                  base::Bind(&SuggestionsLoaded, &loaded));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsProviderImplTest, ReturnTemporaryErrorForInvalidJson) {
  auto provider = MakeSuggestionsProvider();

  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  LoadMoreFromJSONString(provider.get(), articles_category(),
                         "invalid json string}]}",
                         /*known_ids=*/std::set<std::string>(),
                         base::Bind(&SuggestionsLoaded, &loaded));
  EXPECT_THAT(suggestions_fetcher()->GetLastStatusForDebugging(),
              StartsWith("Received invalid JSON"));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ReturnTemporaryErrorForInvalidSuggestion) {
  auto provider = MakeSuggestionsProvider();

  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  LoadMoreFromJSONString(provider.get(), articles_category(),
                         GetTestJson({GetIncompleteSuggestion()}),
                         /*known_ids=*/std::set<std::string>(),
                         base::Bind(&SuggestionsLoaded, &loaded));
  EXPECT_THAT(suggestions_fetcher()->GetLastStatusForDebugging(),
              StartsWith("Invalid / empty list"));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ReturnTemporaryErrorForRequestFailure) {
  // Created SuggestionsProvider will fail by default with unsuccessful request.
  auto provider = MakeSuggestionsProvider(/*set_empty_response=*/false);

  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  provider->Fetch(articles_category(),
                  /*known_ids=*/std::set<std::string>(),
                  base::Bind(&SuggestionsLoaded, &loaded));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsProviderImplTest, ReturnTemporaryErrorForHttpFailure) {
  auto provider = MakeSuggestionsProvider();
  SetUpHttpError();

  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  provider->Fetch(articles_category(),
                  /*known_ids=*/std::set<std::string>(),
                  base::Bind(&SuggestionsLoaded, &loaded));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsProviderImplTest, LoadInvalidJson) {
  auto provider = MakeSuggestionsProvider();

  LoadFromJSONString(provider.get(), GetTestJson({GetInvalidSuggestion()}));
  EXPECT_THAT(suggestions_fetcher()->GetLastStatusForDebugging(),
              StartsWith("Received invalid JSON"));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       LoadInvalidJsonWithExistingSuggestions) {
  auto provider = MakeSuggestionsProvider();

  LoadFromJSONString(provider.get(), GetTestJson({GetSuggestion()}));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
  ASSERT_EQ("OK", suggestions_fetcher()->GetLastStatusForDebugging());

  LoadFromJSONString(provider.get(), GetTestJson({GetInvalidSuggestion()}));
  EXPECT_THAT(suggestions_fetcher()->GetLastStatusForDebugging(),
              StartsWith("Received invalid JSON"));
  // This should not have changed the existing suggestions.
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, LoadIncompleteJson) {
  auto provider = MakeSuggestionsProvider();

  LoadFromJSONString(provider.get(), GetTestJson({GetIncompleteSuggestion()}));
  EXPECT_EQ("Invalid / empty list.",
            suggestions_fetcher()->GetLastStatusForDebugging());
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       LoadIncompleteJsonWithExistingSuggestions) {
  auto provider = MakeSuggestionsProvider();

  LoadFromJSONString(provider.get(), GetTestJson({GetSuggestion()}));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));

  LoadFromJSONString(provider.get(), GetTestJson({GetIncompleteSuggestion()}));
  EXPECT_EQ("Invalid / empty list.",
            suggestions_fetcher()->GetLastStatusForDebugging());
  // This should not have changed the existing suggestions.
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, Dismiss) {
  auto provider = MakeSuggestionsProvider();

  std::string json_str(GetTestJson(
      {GetSuggestionWithSources("http://site.com", "Source 1", "")}));

  LoadFromJSONString(provider.get(), json_str);

  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
  // Load the image to store it in the database.
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(CreateFunctor(cb))));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  gfx::Image image = FetchImage(provider.get(), MakeArticleID(kSuggestionUrl));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Dismissing a non-existent suggestion shouldn't do anything.
  provider->DismissSuggestion(MakeArticleID("http://othersite.com"));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));

  // Dismiss the suggestion.
  provider->DismissSuggestion(MakeArticleID(kSuggestionUrl));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());

  // Verify we can still load the image of the discarded suggestion (other NTPs
  // might still reference it). This should come from the database -- no network
  // fetch necessary.
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  image = FetchImage(provider.get(), MakeArticleID(kSuggestionUrl));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Make sure that fetching the same suggestion again does not re-add it.
  LoadFromJSONString(provider.get(), json_str);
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());

  // The suggestion should stay dismissed even after re-creating the provider.
  ResetSuggestionsProvider(&provider, /*set_empty_response=*/true);
  LoadFromJSONString(provider.get(), json_str);
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());

  // The suggestion can be added again after clearing dismissed suggestions.
  provider->ClearDismissedSuggestionsForDebugging(articles_category());
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
  LoadFromJSONString(provider.get(), json_str);
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, GetDismissed) {
  auto provider = MakeSuggestionsProvider();

  LoadFromJSONString(provider.get(), GetTestJson({GetSuggestion()}));

  provider->DismissSuggestion(MakeArticleID(kSuggestionUrl));

  provider->GetDismissedSuggestionsForDebugging(
      articles_category(),
      base::Bind(
          [](RemoteSuggestionsProviderImpl* provider,
             RemoteSuggestionsProviderImplTest* test,
             std::vector<ContentSuggestion> dismissed_suggestions) {
            EXPECT_EQ(1u, dismissed_suggestions.size());
            for (auto& suggestion : dismissed_suggestions) {
              EXPECT_EQ(test->MakeArticleID(kSuggestionUrl), suggestion.id());
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

TEST_F(RemoteSuggestionsProviderImplTest, CreationTimestampParseFail) {
  auto provider = MakeSuggestionsProvider();

  std::string json = GetSuggestionWithTimes(GetDefaultCreationTime(),
                                            GetDefaultExpirationTime());
  base::ReplaceFirstSubstringAfterOffset(
      &json, 0, FormatTime(GetDefaultCreationTime()), "aaa1448459205");
  std::string json_str(GetTestJson({json}));

  LoadFromJSONString(provider.get(), json_str);
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, RemoveExpiredDismissedContent) {
  auto provider = MakeSuggestionsProvider();

  std::string json_str1(GetTestJson({GetExpiredSuggestion()}));
  // Load it.
  LoadFromJSONString(provider.get(), json_str1);
  // Load the image to store it in the database.
  // TODO(tschumann): Introduce some abstraction to nicely work with image
  // fetching expectations.
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &provider->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(CreateFunctor(cb))));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  gfx::Image image = FetchImage(provider.get(), MakeArticleID(kSuggestionUrl));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Dismiss the suggestion
  provider->DismissSuggestion(
      ContentSuggestion::ID(articles_category(), kSuggestionUrl));

  // Load a different suggestion - this will clear the expired dismissed ones.
  std::string json_str2(GetTestJson({GetSuggestionWithUrl(kSuggestionUrl2)}));
  LoadFromJSONString(provider.get(), json_str2);

  EXPECT_THAT(provider->GetDismissedSuggestionsForTesting(articles_category()),
              IsEmpty());

  // Verify the image got removed, too.
  EXPECT_TRUE(
      FetchImage(provider.get(), MakeArticleID(kSuggestionUrl)).IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, ExpiredContentNotRemoved) {
  auto provider = MakeSuggestionsProvider();

  std::string json_str(GetTestJson({GetExpiredSuggestion()}));

  LoadFromJSONString(provider.get(), json_str);
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, TestSingleSource) {
  auto provider = MakeSuggestionsProvider();

  std::string json_str(GetTestJson({GetSuggestionWithSources(
      "http://source1.com", "Source 1", "http://source1.amp.com")}));

  LoadFromJSONString(provider.get(), json_str);
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
  const RemoteSuggestion& suggestion =
      *provider->GetSuggestionsForTesting(articles_category()).front();
  EXPECT_EQ(suggestion.id(), kSuggestionUrl);
  EXPECT_EQ(suggestion.url(), GURL("http://source1.com"));
  EXPECT_EQ(suggestion.publisher_name(), std::string("Source 1"));
  EXPECT_EQ(suggestion.amp_url(), GURL("http://source1.amp.com"));
}

TEST_F(RemoteSuggestionsProviderImplTest, TestSingleSourceWithMalformedUrl) {
  auto provider = MakeSuggestionsProvider();

  std::string json_str(GetTestJson({GetSuggestionWithSources(
      "ceci n'est pas un url", "Source 1", "http://source1.amp.com")}));

  LoadFromJSONString(provider.get(), json_str);
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, TestSingleSourceWithMissingData) {
  auto provider = MakeSuggestionsProvider();

  std::string json_str(
      GetTestJson({GetSuggestionWithSources("http://source1.com", "", "")}));

  LoadFromJSONString(provider.get(), json_str);
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, LogNumArticlesHistogram) {
  auto provider = MakeSuggestionsProvider();

  base::HistogramTester tester;
  LoadFromJSONString(provider.get(), GetTestJson({GetInvalidSuggestion()}));

  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));

  // Invalid JSON shouldn't contribute to NumArticlesFetched.
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              IsEmpty());

  // Valid JSON with empty list.
  LoadFromJSONString(provider.get(), GetTestJson(std::vector<std::string>()));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));

  // Suggestion list should be populated with size 1.
  LoadFromJSONString(provider.get(), GetTestJson({GetSuggestion()}));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2),
                          base::Bucket(/*min=*/1, /*count=*/1)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/1)));

  // Duplicate suggestion shouldn't increase the list size.
  LoadFromJSONString(provider.get(), GetTestJson({GetSuggestion()}));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      IsEmpty());

  // Dismissing a suggestion should decrease the list size. This will only be
  // logged after the next fetch.
  provider->DismissSuggestion(MakeArticleID(kSuggestionUrl));
  LoadFromJSONString(provider.get(), GetTestJson({GetSuggestion()}));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/3),
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

  const base::Time creation = GetDefaultCreationTime();
  const base::Time expiry = GetDefaultExpirationTime();
  const std::vector<std::string> source_urls = {
      "http://mashable.com/2016/05/11/stolen",
      "http://www.aol.com/article/2016/05/stolen-doggie"};
  const std::vector<std::string> publishers = {"Mashable", "AOL"};
  const std::vector<std::string> amp_urls = {
      "http://mashable-amphtml.googleusercontent.com/1",
      "http://t2.gstatic.com/images?q=tbn:3"};

  // Add the suggestion from the mashable domain.
  LoadFromJSONString(provider.get(),
                     GetTestJson({GetSuggestionWithUrlAndTimesAndSource(
                         source_urls, source_urls[0], creation, expiry,
                         publishers[0], amp_urls[0])}));
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(1));
  // Dismiss the suggestion via the mashable source corpus ID.
  provider->DismissSuggestion(MakeArticleID(source_urls[0]));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());

  // The same article from the AOL domain should now be detected as dismissed.
  LoadFromJSONString(provider.get(),
                     GetTestJson({GetSuggestionWithUrlAndTimesAndSource(
                         source_urls, source_urls[1], creation, expiry,
                         publishers[1], amp_urls[1])}));
  EXPECT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, ImageReturnedWithTheSameId) {
  auto provider = MakeSuggestionsProvider();

  LoadFromJSONString(provider.get(), GetTestJson({GetSuggestion()}));

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
      MakeArticleID(kSuggestionUrl2),
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

  std::string first_suggestion = GetSuggestionWithUrl("http://url1.com");
  std::string second_suggestion = GetSuggestionWithUrl("http://url2.com");
  std::string json_str = GetTestJson({first_suggestion, second_suggestion});
  LoadFromJSONString(provider.get(), json_str);
  ASSERT_THAT(provider->GetSuggestionsForTesting(articles_category()),
              SizeIs(2));

  provider->DismissSuggestion(MakeArticleID("http://url1.com"));
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

  LoadFromJSONString(provider.get(), GetTestJson({GetSuggestion()}));
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
  LoadFromJSONString(provider.get(),
                     GetTestJson({GetSuggestionWithUrl(
                         "http://something.com/pletely/unrelated")}));
  // The image should still be available until a restart happens.
  EXPECT_FALSE(
      FetchImage(provider.get(), MakeArticleID(kSuggestionUrl)).IsEmpty());
  ResetSuggestionsProvider(&provider, /*set_empty_response=*/true);
  // After the restart, the image should be garbage collected.
  EXPECT_TRUE(
      FetchImage(provider.get(), MakeArticleID(kSuggestionUrl)).IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldHandleMoreThanMaxSuggestionsInResponse) {
  auto provider = MakeSuggestionsProvider();

  std::vector<std::string> suggestions;
  for (int i = 0; i < provider->GetMaxSuggestionCountForTesting() + 1; ++i) {
    suggestions.push_back(GetSuggestionWithUrl(
        base::StringPrintf("http://localhost/suggestion-id-%d", i)));
  }
  LoadFromJSONString(provider.get(), GetTestJson(suggestions));
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
      /*use_mock_suggestions_fetcher=*/false);

  auto simple_test_clock = base::MakeUnique<base::SimpleTestClock>();
  base::SimpleTestClock* simple_test_clock_ptr = simple_test_clock.get();
  provider->SetClockForTesting(std::move(simple_test_clock));

  // Test that the preference is correctly initialized with the default value 0.
  EXPECT_EQ(
      0, pref_service()->GetInt64(prefs::kLastSuccessfulBackgroundFetchTime));

  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);
  EXPECT_EQ(
      simple_test_clock_ptr->Now().ToInternalValue(),
      pref_service()->GetInt64(prefs::kLastSuccessfulBackgroundFetchTime));

  // Advance the time and check whether the time was updated correctly after the
  // background fetch.
  simple_test_clock_ptr->Advance(TimeDelta::FromHours(1));

  provider->RefetchInTheBackground(
      RemoteSuggestionsProvider::FetchStatusCallback());
  base::RunLoop().RunUntilIdle();
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
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);
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
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);

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
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);

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
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);

  // The scheduler should be notified of clearing the history.
  EXPECT_CALL(*scheduler(), OnSuggestionsCleared());
  provider->OnStatusChanged(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
                            RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN);
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldExcludeKnownSuggestionsWithoutTruncatingWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_suggestions_fetcher=*/true);
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);
  auto* mock_fetcher = static_cast<StrictMock<MockRemoteSuggestionsFetcher>*>(
      suggestions_fetcher());

  std::set<std::string> known_ids;
  for (int i = 0; i < 200; ++i) {
    known_ids.insert(base::IntToString(i));
  }

  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_fetcher,
              FetchSnippets(Field(&RequestParams::excluded_ids, known_ids), _));
  provider->Fetch(
      articles_category(), known_ids,
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldExcludeDismissedSuggestionsWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_suggestions_fetcher=*/true);
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);

  // Initialize fetcher to return a given suggestion.
  auto* mock_fetcher = static_cast<StrictMock<MockRemoteSuggestionsFetcher>*>(
      suggestions_fetcher());

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));

  fetched_categories[0].suggestions.push_back(
      CreateTestRemoteSuggestion("http://abc.com/"));
  ASSERT_TRUE(fetched_categories[0].suggestions[0]->is_complete());

  // Fetch the suggestion.
  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_fetcher, FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  FetchSuggestions(provider.get(), /*interactive_request=*/true,
                   RemoteSuggestionsProvider::FetchStatusCallback());
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"),
           std::move(fetched_categories));

  provider->DismissSuggestion(MakeArticleID("http://abc.com/"));

  std::set<std::string> expected_excluded_ids({"http://abc.com/"});
  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_fetcher, FetchSnippets(Field(&RequestParams::excluded_ids,
                                                 expected_excluded_ids),
                                           _));
  provider->Fetch(
      articles_category(), std::set<std::string>(),
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldTruncateExcludedDismissedSuggestionsWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_suggestions_fetcher=*/true);
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);

  // Initialize fetcher to return given suggestions.
  auto* mock_fetcher = static_cast<StrictMock<MockRemoteSuggestionsFetcher>*>(
      suggestions_fetcher());

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));

  const int kSuggestionsCount = kMaxExcludedDismissedIds + 1;
  for (int i = 0; i < kSuggestionsCount; ++i) {
    fetched_categories[0].suggestions.push_back(CreateTestRemoteSuggestion(
        base::StringPrintf("http://abc.com/%d/", i)));
  }

  // Fetch the suggestions.
  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_fetcher, FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  FetchSuggestions(provider.get(), /*interactive_request=*/true,
                   RemoteSuggestionsProvider::FetchStatusCallback());
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"),
           std::move(fetched_categories));

  // Dismiss them.
  for (int i = 0; i < kSuggestionsCount; ++i) {
    provider->DismissSuggestion(
        MakeArticleID(base::StringPrintf("http://abc.com/%d/", i)));
  }

  EXPECT_CALL(*scheduler(), AcquireQuotaForInteractiveFetch())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_fetcher,
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
      /*use_mock_suggestions_fetcher=*/true);
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);

  // Initialize fetcher to return given suggestions.
  auto* mock_fetcher = static_cast<StrictMock<MockRemoteSuggestionsFetcher>*>(
      suggestions_fetcher());

  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));

  const int kSuggestionsCount = kMaxExcludedDismissedIds + 1;
  for (int i = 0; i < kSuggestionsCount; ++i) {
    fetched_categories[0].suggestions.push_back(CreateTestRemoteSuggestion(
        base::StringPrintf("http://abc.com/%d/", i)));
  }

  // Fetch the suggestions.
  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_fetcher, FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  FetchSuggestions(provider.get(), /*interactive_request=*/true,
                   RemoteSuggestionsProvider::FetchStatusCallback());
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"),
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
  EXPECT_CALL(*mock_fetcher,
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
      /*use_mock_suggestions_fetcher=*/true);
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);

  // Initialize fetcher to return given suggestions.
  auto* mock_fetcher = static_cast<StrictMock<MockRemoteSuggestionsFetcher>*>(
      suggestions_fetcher());

  // Add article suggestions.
  std::vector<FetchedCategory> fetched_categories;
  fetched_categories.push_back(FetchedCategory(
      articles_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));

  const int kSuggestionsPerCategory = 2;
  for (int i = 0; i < kSuggestionsPerCategory; ++i) {
    fetched_categories[0].suggestions.push_back(CreateTestRemoteSuggestion(
        base::StringPrintf("http://abc.com/%d/", i)));
  }
  // Add other category suggestions.
  fetched_categories.push_back(FetchedCategory(
      other_category(),
      BuildRemoteCategoryInfo(base::UTF8ToUTF16("title"),
                              /*allow_fetching_more_results=*/true)));
  for (int i = 0; i < kSuggestionsPerCategory; ++i) {
    fetched_categories[1].suggestions.push_back(CreateTestRemoteSuggestion(
        base::StringPrintf("http://other.com/%d/", i)));
  }

  // Fetch the suggestions.
  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_fetcher, FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  FetchSuggestions(provider.get(), /*interactive_request=*/true,
                   RemoteSuggestionsProvider::FetchStatusCallback());
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"),
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
  EXPECT_CALL(*mock_fetcher, FetchSnippets(Field(&RequestParams::excluded_ids,
                                                 expected_excluded_ids),
                                           _));
  provider->Fetch(
      articles_category(), std::set<std::string>(),
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldPreferTargetCategoryExcludedDismissedSuggestionsWhenFetchingMore) {
  auto provider = MakeSuggestionsProviderWithoutInitialization(
      /*use_mock_suggestions_fetcher=*/true);
  WaitForSuggestionsProviderInitialization(provider.get(),
                                           /*set_empty_response=*/true);

  // Initialize fetcher to return given suggestions.
  auto* mock_fetcher = static_cast<StrictMock<MockRemoteSuggestionsFetcher>*>(
      suggestions_fetcher());

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

  // Fetch the suggestions.
  RemoteSuggestionsFetcher::SnippetsAvailableCallback snippets_callback;
  EXPECT_CALL(*mock_fetcher, FetchSnippets(_, _))
      .WillOnce(MoveSecondArgumentPointeeTo(&snippets_callback))
      .RetiresOnSaturation();
  FetchSuggestions(provider.get(), /*interactive_request=*/true,
                   RemoteSuggestionsProvider::FetchStatusCallback());
  std::move(snippets_callback)
      .Run(Status(StatusCode::SUCCESS, "message"),
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
  EXPECT_CALL(*mock_fetcher,
              FetchSnippets(Field(&RequestParams::excluded_ids,
                                  Not(Contains("http://other.com/"))),
                            _));
  provider->Fetch(
      articles_category(), std::set<std::string>(),
      base::Bind([](Status status_code,
                    std::vector<ContentSuggestion> suggestions) {}));
}

}  // namespace ntp_snippets
