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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/ntp_snippets/category_rankers/mock_category_ranker.h"
#include "components/ntp_snippets/fake_content_suggestions_provider_observer.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/ntp_snippet.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/variations/variations_params_manager.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using image_fetcher::ImageFetcher;
using image_fetcher::ImageFetcherDelegate;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::InSequence;
using testing::Invoke;
using testing::IsEmpty;
using testing::Mock;
using testing::MockFunction;
using testing::NiceMock;
using testing::Not;
using testing::SaveArg;
using testing::SizeIs;
using testing::StartsWith;
using testing::StrictMock;
using testing::WithArgs;

namespace ntp_snippets {

namespace {

MATCHER_P(IdEq, value, "") {
  return arg->id() == value;
}

MATCHER_P(IdWithinCategoryEq, expected_id, "") {
  return arg.id().id_within_category() == expected_id;
}

MATCHER_P(IsCategory, id, "") {
  return arg.id() == static_cast<int>(id);
}

MATCHER_P(HasCode, code, "") {
  return arg.code == code;
}

const base::Time::Exploded kDefaultCreationTime = {2015, 11, 4, 25, 13, 46, 45};
const char kTestContentSuggestionsServerEndpoint[] =
    "https://localunittest-chromecontentsuggestions-pa.googleapis.com/v1/"
    "suggestions/fetch";
const char kAPIKey[] = "fakeAPIkey";
const char kTestContentSuggestionsServerWithAPIKey[] =
    "https://localunittest-chromecontentsuggestions-pa.googleapis.com/v1/"
    "suggestions/fetch?key=fakeAPIkey";

const char kSnippetUrl[] = "http://localhost/foobar";
const char kSnippetTitle[] = "Title";
const char kSnippetText[] = "Snippet";
const char kSnippetSalientImage[] = "http://localhost/salient_image";
const char kSnippetPublisherName[] = "Foo News";
const char kSnippetAmpUrl[] = "http://localhost/amp";

const char kSnippetUrl2[] = "http://foo.com/bar";

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

std::string GetCategoryJson(const std::vector<std::string>& snippets,
                            int remote_category_id,
                            const std::string& category_title) {
  return base::StringPrintf(
      "  {\n"
      "    \"id\": %d,\n"
      "    \"localizedTitle\": \"%s\",\n"
      "    \"suggestions\": [%s]\n"
      "  }\n",
      remote_category_id, category_title.c_str(),
      base::JoinString(snippets, ", ").c_str());
}

class MultiCategoryJsonBuilder {
 public:
  MultiCategoryJsonBuilder() {}

  MultiCategoryJsonBuilder& AddCategoryWithCustomTitle(
      const std::vector<std::string>& snippets,
      int remote_category_id,
      const std::string& category_title) {
    category_json_.push_back(
        GetCategoryJson(snippets, remote_category_id, category_title));
    return *this;
  }

  MultiCategoryJsonBuilder& AddCategory(
      const std::vector<std::string>& snippets,
      int remote_category_id) {
    return AddCategoryWithCustomTitle(
        snippets, remote_category_id,
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
std::string GetTestJson(const std::vector<std::string>& snippets,
                        const std::string& category_title) {
  return MultiCategoryJsonBuilder()
      .AddCategoryWithCustomTitle(snippets, /*remote_category_id=*/1,
                                  category_title)
      .Build();
}

std::string GetTestJson(const std::vector<std::string>& snippets) {
  return GetTestJson(snippets, kTestJsonDefaultCategoryTitle);
}

std::string FormatTime(const base::Time& t) {
  base::Time::Exploded x;
  t.UTCExplode(&x);
  return base::StringPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ", x.year, x.month,
                            x.day_of_month, x.hour, x.minute, x.second);
}

std::string GetSnippetWithUrlAndTimesAndSource(
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
      ids_string.c_str(), kSnippetTitle, kSnippetText, url.c_str(),
      FormatTime(creation_time).c_str(), FormatTime(expiry_time).c_str(),
      publisher.c_str(), kSnippetSalientImage, amp_url.c_str());
}

std::string GetSnippetWithSources(const std::string& source_url,
                                  const std::string& publisher,
                                  const std::string& amp_url) {
  return GetSnippetWithUrlAndTimesAndSource(
      {kSnippetUrl}, source_url, GetDefaultCreationTime(),
      GetDefaultExpirationTime(), publisher, amp_url);
}

std::string GetSnippetWithUrlAndTimes(const std::string& url,
                                      const base::Time& content_creation_time,
                                      const base::Time& expiry_time) {
  return GetSnippetWithUrlAndTimesAndSource({url}, url, content_creation_time,
                                            expiry_time, kSnippetPublisherName,
                                            kSnippetAmpUrl);
}

std::string GetSnippetWithTimes(const base::Time& content_creation_time,
                                const base::Time& expiry_time) {
  return GetSnippetWithUrlAndTimes(kSnippetUrl, content_creation_time,
                                   expiry_time);
}

std::string GetSnippetWithUrl(const std::string& url) {
  return GetSnippetWithUrlAndTimes(url, GetDefaultCreationTime(),
                                   GetDefaultExpirationTime());
}

std::string GetSnippet() {
  return GetSnippetWithUrlAndTimes(kSnippetUrl, GetDefaultCreationTime(),
                                   GetDefaultExpirationTime());
}

std::string GetSnippetN(int n) {
  return GetSnippetWithUrlAndTimes(base::StringPrintf("%s/%d", kSnippetUrl, n),
                                   GetDefaultCreationTime(),
                                   GetDefaultExpirationTime());
}

std::string GetExpiredSnippet() {
  return GetSnippetWithTimes(GetDefaultCreationTime(), base::Time::Now());
}

std::string GetInvalidSnippet() {
  std::string json_str = GetSnippet();
  // Make the json invalid by removing the final closing brace.
  return json_str.substr(0, json_str.size() - 1);
}

std::string GetIncompleteSnippet() {
  std::string json_str = GetSnippet();
  // Rename the "url" entry. The result is syntactically valid json that will
  // fail to parse as snippets.
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
    base::Callback<void(const std::string&, const gfx::Image&)>)>;

void ServeOneByOneImage(
    image_fetcher::ImageFetcherDelegate* notify,
    const std::string& id,
    base::Callback<void(const std::string&, const gfx::Image&)> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, id, gfx::test::CreateImage(1, 1)));
  notify->OnImageDataFetched(id, "1-by-1-image-data");
}

gfx::Image FetchImage(RemoteSuggestionsProviderImpl* service,
                      const ContentSuggestion::ID& suggestion_id) {
  gfx::Image result;
  base::RunLoop run_loop;
  service->FetchSuggestionImage(suggestion_id,
                                base::Bind(
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
      net::URLFetcherDelegate* d) override {
    return base::MakeUnique<net::FakeURLFetcher>(
        url, d, /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
        net::URLRequestStatus::FAILED);
  }
};

class MockImageFetcher : public ImageFetcher {
 public:
  MOCK_METHOD1(SetImageFetcherDelegate, void(ImageFetcherDelegate*));
  MOCK_METHOD1(SetDataUseServiceName, void(DataUseServiceName));
  MOCK_METHOD3(
      StartOrQueueNetworkRequest,
      void(const std::string&,
           const GURL&,
           base::Callback<void(const std::string&, const gfx::Image&)>));
};

class FakeImageDecoder : public image_fetcher::ImageDecoder {
 public:
  FakeImageDecoder() {}
  ~FakeImageDecoder() override = default;
  void DecodeImage(
      const std::string& image_data,
      const image_fetcher::ImageDecodedCallback& callback) override {
    callback.Run(decoded_image_);
  }

  void SetDecodedImage(const gfx::Image& image) { decoded_image_ = image; }

 private:
  gfx::Image decoded_image_;
};

}  // namespace

class RemoteSuggestionsProviderImplTest : public ::testing::Test {
 public:
  RemoteSuggestionsProviderImplTest()
      : params_manager_(ntp_snippets::kStudyName,
                        {{"content_suggestions_backend",
                          kTestContentSuggestionsServerEndpoint},
                         {"fetching_personalization", "non_personal"}}),
        fake_url_fetcher_factory_(
            /*default_factory=*/&failing_url_fetcher_factory_),
        test_url_(kTestContentSuggestionsServerWithAPIKey),
        category_ranker_(base::MakeUnique<ConstantCategoryRanker>()),
        user_classifier_(/*pref_service=*/nullptr),
        suggestions_fetcher_(nullptr),
        image_fetcher_(nullptr),
        image_decoder_(nullptr),
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
  // instead of creating a new service.
  std::unique_ptr<RemoteSuggestionsProviderImpl> MakeSnippetsService(
      bool set_empty_response = true) {
    auto service = MakeSnippetsServiceWithoutInitialization();
    WaitForSnippetsServiceInitialization(service.get(), set_empty_response);
    return service;
  }

  std::unique_ptr<RemoteSuggestionsProviderImpl>
  MakeSnippetsServiceWithoutInitialization() {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    scoped_refptr<net::TestURLRequestContextGetter> request_context_getter =
        new net::TestURLRequestContextGetter(task_runner.get());

    utils_.ResetSigninManager();
    auto suggestions_fetcher = base::MakeUnique<RemoteSuggestionsFetcher>(
        utils_.fake_signin_manager(), fake_token_service_.get(),
        std::move(request_context_getter), utils_.pref_service(), nullptr,
        base::Bind(&ParseJson), kAPIKey, &user_classifier_);
    suggestions_fetcher_ = suggestions_fetcher.get();

    utils_.fake_signin_manager()->SignIn("foo@bar.com");

    auto image_fetcher = base::MakeUnique<NiceMock<MockImageFetcher>>();

    image_fetcher_ = image_fetcher.get();
    EXPECT_CALL(*image_fetcher, SetImageFetcherDelegate(_));
    auto image_decoder = base::MakeUnique<FakeImageDecoder>();
    image_decoder_ = image_decoder.get();
    EXPECT_FALSE(observer_);
    observer_ = base::MakeUnique<FakeContentSuggestionsProviderObserver>();
    auto database = base::MakeUnique<RemoteSuggestionsDatabase>(
        database_dir_.GetPath(), task_runner);
    database_ = database.get();
    return base::MakeUnique<RemoteSuggestionsProviderImpl>(
        observer_.get(), utils_.pref_service(), "fr", category_ranker_.get(),
        std::move(suggestions_fetcher), std::move(image_fetcher),
        std::move(image_decoder), std::move(database),
        base::MakeUnique<RemoteSuggestionsStatusService>(
            utils_.fake_signin_manager(), utils_.pref_service()));
  }

  void WaitForSnippetsServiceInitialization(
      RemoteSuggestionsProviderImpl* service,
      bool set_empty_response) {
    EXPECT_EQ(RemoteSuggestionsProviderImpl::State::NOT_INITED,
              service->state_);

    // Add an initial fetch response, as the service tries to fetch when there
    // is nothing in the DB.
    if (set_empty_response) {
      SetUpFetchResponse(GetTestJson(std::vector<std::string>()));
    }

    // TODO(treib): Find a better way to wait for initialization to finish.
    base::RunLoop().RunUntilIdle();
    EXPECT_NE(RemoteSuggestionsProviderImpl::State::NOT_INITED,
              service->state_);
  }

  void ResetSnippetsService(
      std::unique_ptr<RemoteSuggestionsProviderImpl>* service,
      bool set_empty_response) {
    service->reset();
    observer_.reset();
    *service = MakeSnippetsService(set_empty_response);
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
  FakeImageDecoder* image_decoder() { return image_decoder_; }
  PrefService* pref_service() { return utils_.pref_service(); }
  RemoteSuggestionsDatabase* database() { return database_; }

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

  void LoadFromJSONString(RemoteSuggestionsProviderImpl* service,
                          const std::string& json) {
    SetUpFetchResponse(json);
    service->FetchSnippets(/*interactive_request=*/true,
                           /*callback=*/nullptr);
    base::RunLoop().RunUntilIdle();
  }

  void LoadMoreFromJSONString(RemoteSuggestionsProviderImpl* service,
                              const Category& category,
                              const std::string& json,
                              const std::set<std::string>& known_ids,
                              FetchDoneCallback callback) {
    SetUpFetchResponse(json);
    service->Fetch(category, known_ids, callback);
    base::RunLoop().RunUntilIdle();
  }

 private:
  variations::testing::VariationParamsManager params_manager_;
  test::RemoteSuggestionsTestUtils utils_;
  base::MessageLoop message_loop_;
  FailingFakeURLFetcherFactory failing_url_fetcher_factory_;
  // Instantiation of factory automatically sets itself as URLFetcher's factory.
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  const GURL test_url_;
  std::unique_ptr<OAuth2TokenService> fake_token_service_;
  std::unique_ptr<CategoryRanker> category_ranker_;
  UserClassifier user_classifier_;
  std::unique_ptr<FakeContentSuggestionsProviderObserver> observer_;
  RemoteSuggestionsFetcher* suggestions_fetcher_;
  NiceMock<MockImageFetcher>* image_fetcher_;
  FakeImageDecoder* image_decoder_;

  base::ScopedTempDir database_dir_;
  RemoteSuggestionsDatabase* database_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsProviderImplTest);
};

TEST_F(RemoteSuggestionsProviderImplTest, Full) {
  std::string json_str(GetTestJson({GetSnippet()}));

  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), json_str);

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  const ContentSuggestion& suggestion =
      observer().SuggestionsForCategory(articles_category()).front();

  EXPECT_EQ(MakeArticleID(kSnippetUrl), suggestion.id());
  EXPECT_EQ(kSnippetTitle, base::UTF16ToUTF8(suggestion.title()));
  EXPECT_EQ(kSnippetText, base::UTF16ToUTF8(suggestion.snippet_text()));
  EXPECT_EQ(GetDefaultCreationTime(), suggestion.publish_date());
  EXPECT_EQ(kSnippetPublisherName,
            base::UTF16ToUTF8(suggestion.publisher_name()));
}

TEST_F(RemoteSuggestionsProviderImplTest, CategoryTitle) {
  const base::string16 test_default_title =
      base::UTF8ToUTF16(kTestJsonDefaultCategoryTitle);

  // Don't send an initial response -- we want to test what happens without any
  // server status.
  auto service = MakeSnippetsService(/*set_empty_response=*/false);

  // The articles category should be there by default, and have a title.
  CategoryInfo info_before = service->GetCategoryInfo(articles_category());
  ASSERT_THAT(info_before.title(), Not(IsEmpty()));
  ASSERT_THAT(info_before.title(), Not(Eq(test_default_title)));
  EXPECT_THAT(info_before.has_more_action(), Eq(true));
  EXPECT_THAT(info_before.has_reload_action(), Eq(true));
  EXPECT_THAT(info_before.has_view_all_action(), Eq(false));
  EXPECT_THAT(info_before.show_if_empty(), Eq(true));

  std::string json_str_with_title(GetTestJson({GetSnippet()}));
  LoadFromJSONString(service.get(), json_str_with_title);

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  // The response contained a title, |kTestJsonDefaultCategoryTitle|.
  // Make sure we updated the title in the CategoryInfo.
  CategoryInfo info_with_title = service->GetCategoryInfo(articles_category());
  EXPECT_THAT(info_before.title(), Not(Eq(info_with_title.title())));
  EXPECT_THAT(test_default_title, Eq(info_with_title.title()));
  EXPECT_THAT(info_before.has_more_action(), Eq(true));
  EXPECT_THAT(info_before.has_reload_action(), Eq(true));
  EXPECT_THAT(info_before.has_view_all_action(), Eq(false));
  EXPECT_THAT(info_before.show_if_empty(), Eq(true));
}

TEST_F(RemoteSuggestionsProviderImplTest, MultipleCategories) {
  auto service = MakeSnippetsService();
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSnippetN(0)}, /*remote_category_id=*/1)
          .AddCategory({GetSnippetN(1)}, /*remote_category_id=*/2)
          .Build();
  LoadFromJSONString(service.get(), json_str);

  ASSERT_THAT(observer().statuses(),
              Eq(std::map<Category, CategoryStatus, Category::CompareByID>{
                  {articles_category(), CategoryStatus::AVAILABLE},
                  {other_category(), CategoryStatus::AVAILABLE},
              }));

  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
  EXPECT_THAT(service->GetSnippetsForTesting(other_category()), SizeIs(1));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));

  ASSERT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));

  {
    const ContentSuggestion& suggestion =
        observer().SuggestionsForCategory(articles_category()).front();
    EXPECT_EQ(MakeArticleID(std::string(kSnippetUrl) + "/0"), suggestion.id());
    EXPECT_EQ(kSnippetTitle, base::UTF16ToUTF8(suggestion.title()));
    EXPECT_EQ(kSnippetText, base::UTF16ToUTF8(suggestion.snippet_text()));
    EXPECT_EQ(GetDefaultCreationTime(), suggestion.publish_date());
    EXPECT_EQ(kSnippetPublisherName,
              base::UTF16ToUTF8(suggestion.publisher_name()));
  }

  {
    const ContentSuggestion& suggestion =
        observer().SuggestionsForCategory(other_category()).front();
    EXPECT_EQ(MakeOtherID(std::string(kSnippetUrl) + "/1"), suggestion.id());
    EXPECT_EQ(kSnippetTitle, base::UTF16ToUTF8(suggestion.title()));
    EXPECT_EQ(kSnippetText, base::UTF16ToUTF8(suggestion.snippet_text()));
    EXPECT_EQ(GetDefaultCreationTime(), suggestion.publish_date());
    EXPECT_EQ(kSnippetPublisherName,
              base::UTF16ToUTF8(suggestion.publisher_name()));
  }
}

TEST_F(RemoteSuggestionsProviderImplTest, ArticleCategoryInfo) {
  auto service = MakeSnippetsService();
  CategoryInfo article_info = service->GetCategoryInfo(articles_category());
  EXPECT_THAT(article_info.has_more_action(), Eq(true));
  EXPECT_THAT(article_info.has_reload_action(), Eq(true));
  EXPECT_THAT(article_info.has_view_all_action(), Eq(false));
  EXPECT_THAT(article_info.show_if_empty(), Eq(true));
}

TEST_F(RemoteSuggestionsProviderImplTest, ExperimentalCategoryInfo) {
  auto service = MakeSnippetsService();
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSnippetN(0)}, /*remote_category_id=*/1)
          .AddCategory({GetSnippetN(1)}, kUnknownRemoteCategoryId)
          .Build();
  // Load data with multiple categories so that a new experimental category gets
  // registered.
  LoadFromJSONString(service.get(), json_str);

  CategoryInfo info = service->GetCategoryInfo(unknown_category());
  EXPECT_THAT(info.has_more_action(), Eq(false));
  EXPECT_THAT(info.has_reload_action(), Eq(false));
  EXPECT_THAT(info.has_view_all_action(), Eq(false));
  EXPECT_THAT(info.show_if_empty(), Eq(false));
}

TEST_F(RemoteSuggestionsProviderImplTest, AddRemoteCategoriesToCategoryRanker) {
  auto mock_ranker = base::MakeUnique<MockCategoryRanker>();
  MockCategoryRanker* raw_mock_ranker = mock_ranker.get();
  SetCategoryRanker(std::move(mock_ranker));
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSnippetN(0)}, /*remote_category_id=*/11)
          .AddCategory({GetSnippetN(1)}, /*remote_category_id=*/13)
          .AddCategory({GetSnippetN(2)}, /*remote_category_id=*/12)
          .Build();
  SetUpFetchResponse(json_str);
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
  auto service = MakeSnippetsService(/*set_empty_response=*/false);
}

TEST_F(RemoteSuggestionsProviderImplTest, PersistCategoryInfos) {
  auto service = MakeSnippetsService();
  // TODO(vitaliii): Use |articles_category()| instead of constant ID below.
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategoryWithCustomTitle(
              {GetSnippetN(0)}, /*remote_category_id=*/1, "Articles for You")
          .AddCategoryWithCustomTitle({GetSnippetN(1)},
                                      kUnknownRemoteCategoryId, "Other Things")
          .Build();
  LoadFromJSONString(service.get(), json_str);

  ASSERT_EQ(observer().StatusForCategory(articles_category()),
            CategoryStatus::AVAILABLE);
  ASSERT_EQ(observer().StatusForCategory(unknown_category()),
            CategoryStatus::AVAILABLE);

  CategoryInfo info_articles_before =
      service->GetCategoryInfo(articles_category());
  CategoryInfo info_unknown_before =
      service->GetCategoryInfo(unknown_category());

  // Recreate the service to simulate a Chrome restart.
  ResetSnippetsService(&service, /*set_empty_response=*/true);

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
      service->GetCategoryInfo(articles_category());
  CategoryInfo info_unknown_after =
      service->GetCategoryInfo(unknown_category());

  EXPECT_EQ(info_articles_before.title(), info_articles_after.title());
  EXPECT_EQ(info_unknown_before.title(), info_unknown_after.title());
}

TEST_F(RemoteSuggestionsProviderImplTest, PersistRemoteCategoryOrder) {
  // We create a service with a normal ranker to store the order.
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSnippetN(0)}, /*remote_category_id=*/11)
          .AddCategory({GetSnippetN(1)}, /*remote_category_id=*/13)
          .AddCategory({GetSnippetN(2)}, /*remote_category_id=*/12)
          .Build();
  SetUpFetchResponse(json_str);
  auto service = MakeSnippetsService(/*set_empty_response=*/false);

  // We manually recreate the service to simulate Chrome restart and enforce a
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
  ResetSnippetsService(&service, /*set_empty_response=*/false);
}

TEST_F(RemoteSuggestionsProviderImplTest, PersistSuggestions) {
  auto service = MakeSnippetsService();
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSnippetN(0)}, /*remote_category_id=*/1)
          .AddCategory({GetSnippetN(2)}, /*remote_category_id=*/2)
          .Build();
  LoadFromJSONString(service.get(), json_str);

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));

  // Recreate the service to simulate a Chrome restart.
  ResetSnippetsService(&service, /*set_empty_response=*/true);

  // The suggestions in both categories should have been restored.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  EXPECT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, DontNotifyIfNotAvailable) {
  // Get some suggestions into the database.
  auto service = MakeSnippetsService();
  std::string json_str =
      MultiCategoryJsonBuilder()
          .AddCategory({GetSnippetN(0)},
                       /*remote_category_id=*/1)
          .AddCategory({GetSnippetN(1)}, /*remote_category_id=*/2)
          .Build();
  LoadFromJSONString(service.get(), json_str);

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));

  service.reset();

  // Set the pref that disables remote suggestions.
  pref_service()->SetBoolean(prefs::kEnableSnippets, false);

  // Recreate the service to simulate a Chrome start.
  ResetSnippetsService(&service, /*set_empty_response=*/true);

  ASSERT_THAT(RemoteSuggestionsProviderImpl::State::DISABLED,
              Eq(service->state_));

  // Now the observer should not have received any suggestions.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              IsEmpty());
  EXPECT_THAT(observer().SuggestionsForCategory(other_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, Clear) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetSnippet()}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  service->ClearCachedSuggestions(articles_category());
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, ReplaceSnippets) {
  auto service = MakeSnippetsService();

  std::string first("http://first");
  LoadFromJSONString(service.get(), GetTestJson({GetSnippetWithUrl(first)}));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()),
              ElementsAre(IdEq(first)));

  std::string second("http://second");
  LoadFromJSONString(service.get(), GetTestJson({GetSnippetWithUrl(second)}));
  // The snippets loaded last replace all that was loaded previously.
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()),
              ElementsAre(IdEq(second)));
}

TEST_F(RemoteSuggestionsProviderImplTest, LoadsAdditionalSnippets) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(),
                     GetTestJson({GetSnippetWithUrl("http://first")}));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()),
              ElementsAre(IdEq("http://first")));

  auto expect_only_second_suggestion_received = base::Bind([](
      Status status, std::vector<ContentSuggestion> suggestions) {
    EXPECT_THAT(suggestions, SizeIs(1));
    EXPECT_THAT(suggestions[0].id().id_within_category(), Eq("http://second"));
  });
  LoadMoreFromJSONString(service.get(), articles_category(),
                         GetTestJson({GetSnippetWithUrl("http://second")}),
                         /*known_ids=*/std::set<std::string>(),
                         expect_only_second_suggestion_received);

  // Verify we can resolve the image of the new snippets.
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &service->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _))
      .Times(2)
      .WillRepeatedly(WithArgs<0, 2>(Invoke(&cb, &ServeImageCallback::Run)));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  gfx::Image image = FetchImage(service.get(), MakeArticleID("http://first"));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  image = FetchImage(service.get(), MakeArticleID("http://second"));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Verify that the observer received the update as well. We should see the
  // newly-fetched items filled up with existing ones.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              ElementsAre(IdWithinCategoryEq("http://first"),
                          IdWithinCategoryEq("http://second")));
}

// The tests TestMergingFetchedMoreSnippetsFillup and
// TestMergingFetchedMoreSnippetsReplaceAll simulate the following user story:
// 1) fetch suggestions in NTP A
// 2) fetch more suggestions in NTP A.
// 3) open new NTP B: See the last 10 results visible in step 2).
// 4) fetch more suggestions in NTP B. Make sure no results from step 1) which
//    were superseded in step 2) get merged back in again.
// TODO(tschumann): Test step 4) on a higher level instead of peeking into the
// internal 'dismissed' data. The proper check is to make sure we tell the
// backend to exclude these snippets.
TEST_F(RemoteSuggestionsProviderImplTest,
       TestMergingFetchedMoreSnippetsFillup) {
  auto service = MakeSnippetsService(/*set_empty_response=*/false);
  LoadFromJSONString(
      service.get(),
      GetTestJson(
          {GetSnippetWithUrl("http://id-1"), GetSnippetWithUrl("http://id-2"),
           GetSnippetWithUrl("http://id-3"), GetSnippetWithUrl("http://id-4"),
           GetSnippetWithUrl("http://id-5"), GetSnippetWithUrl("http://id-6"),
           GetSnippetWithUrl("http://id-7"), GetSnippetWithUrl("http://id-8"),
           GetSnippetWithUrl("http://id-9"),
           GetSnippetWithUrl("http://id-10")}));
  EXPECT_THAT(
      observer().SuggestionsForCategory(articles_category()),
      ElementsAre(
          IdWithinCategoryEq("http://id-1"), IdWithinCategoryEq("http://id-2"),
          IdWithinCategoryEq("http://id-3"), IdWithinCategoryEq("http://id-4"),
          IdWithinCategoryEq("http://id-5"), IdWithinCategoryEq("http://id-6"),
          IdWithinCategoryEq("http://id-7"), IdWithinCategoryEq("http://id-8"),
          IdWithinCategoryEq("http://id-9"),
          IdWithinCategoryEq("http://id-10")));

  auto expect_receiving_two_new_snippets =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        ASSERT_THAT(suggestions, SizeIs(2));
        EXPECT_THAT(suggestions[0], IdWithinCategoryEq("http://more-id-1"));
        EXPECT_THAT(suggestions[1], IdWithinCategoryEq("http://more-id-2"));
      });
  LoadMoreFromJSONString(
      service.get(), articles_category(),
      GetTestJson({GetSnippetWithUrl("http://more-id-1"),
                   GetSnippetWithUrl("http://more-id-2")}),
      /*known_ids=*/{"http://id-1", "http://id-2", "http://id-3", "http://id-4",
                     "http://id-5", "http://id-6", "http://id-7", "http://id-8",
                     "http://id-9", "http://id-10"},
      expect_receiving_two_new_snippets);

  // Verify that the observer received the update as well. We should see the
  // newly-fetched items filled up with existing ones. The merging is done
  // mimicking a scrolling behavior.
  EXPECT_THAT(
      observer().SuggestionsForCategory(articles_category()),
      ElementsAre(
          IdWithinCategoryEq("http://id-3"), IdWithinCategoryEq("http://id-4"),
          IdWithinCategoryEq("http://id-5"), IdWithinCategoryEq("http://id-6"),
          IdWithinCategoryEq("http://id-7"), IdWithinCategoryEq("http://id-8"),
          IdWithinCategoryEq("http://id-9"), IdWithinCategoryEq("http://id-10"),
          IdWithinCategoryEq("http://more-id-1"),
          IdWithinCategoryEq("http://more-id-2")));
  // Verify the superseded suggestions got marked as dismissed.
  EXPECT_THAT(service->GetDismissedSnippetsForTesting(articles_category()),
              ElementsAre(IdEq("http://id-1"), IdEq("http://id-2")));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ClearHistoryShouldDeleteArchivedSnippets) {
  auto service = MakeSnippetsService(/*set_empty_response=*/false);
  // First get suggestions into the archived state which happens through
  // subsequent fetches. Then we verify the entries are gone from the 'archived'
  // state by trying to load their images (and we shouldn't even know the URLs
  // anymore).
  LoadFromJSONString(service.get(),
                     GetTestJson({GetSnippetWithUrl("http://id-1"),
                                  GetSnippetWithUrl("http://id-2")}));
  LoadFromJSONString(service.get(),
                     GetTestJson({GetSnippetWithUrl("http://new-id-1"),
                                  GetSnippetWithUrl("http://new-id-2")}));
  // Make sure images of both batches are available. This is to sanity check our
  // assumptions for the test are right.
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &service->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _))
      .Times(2)
      .WillRepeatedly(WithArgs<0, 2>(Invoke(&cb, &ServeImageCallback::Run)));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  gfx::Image image = FetchImage(service.get(), MakeArticleID("http://id-1"));
  ASSERT_FALSE(image.IsEmpty());
  ASSERT_EQ(1, image.Width());
  image = FetchImage(service.get(), MakeArticleID("http://new-id-1"));
  ASSERT_FALSE(image.IsEmpty());
  ASSERT_EQ(1, image.Width());

  service->ClearHistory(base::Time::UnixEpoch(), base::Time::Max(),
                        base::Callback<bool(const GURL& url)>());

  // Make sure images of both batches are gone.
  // Verify we cannot resolve the image of the new snippets.
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  EXPECT_TRUE(
      FetchImage(service.get(), MakeArticleID("http://id-1")).IsEmpty());
  EXPECT_TRUE(
      FetchImage(service.get(), MakeArticleID("http://new-id-1")).IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       TestMergingFetchedMoreSnippetsReplaceAll) {
  auto service = MakeSnippetsService(/*set_empty_response=*/false);
  LoadFromJSONString(
      service.get(),
      GetTestJson(
          {GetSnippetWithUrl("http://id-1"), GetSnippetWithUrl("http://id-2"),
           GetSnippetWithUrl("http://id-3"), GetSnippetWithUrl("http://id-4"),
           GetSnippetWithUrl("http://id-5"), GetSnippetWithUrl("http://id-6"),
           GetSnippetWithUrl("http://id-7"), GetSnippetWithUrl("http://id-8"),
           GetSnippetWithUrl("http://id-9"),
           GetSnippetWithUrl("http://id-10")}));
  EXPECT_THAT(
      observer().SuggestionsForCategory(articles_category()),
      ElementsAre(
          IdWithinCategoryEq("http://id-1"), IdWithinCategoryEq("http://id-2"),
          IdWithinCategoryEq("http://id-3"), IdWithinCategoryEq("http://id-4"),
          IdWithinCategoryEq("http://id-5"), IdWithinCategoryEq("http://id-6"),
          IdWithinCategoryEq("http://id-7"), IdWithinCategoryEq("http://id-8"),
          IdWithinCategoryEq("http://id-9"),
          IdWithinCategoryEq("http://id-10")));

  auto expect_receiving_ten_new_snippets =
      base::Bind([](Status status, std::vector<ContentSuggestion> suggestions) {
        EXPECT_THAT(suggestions,
                    ElementsAre(IdWithinCategoryEq("http://more-id-1"),
                                IdWithinCategoryEq("http://more-id-2"),
                                IdWithinCategoryEq("http://more-id-3"),
                                IdWithinCategoryEq("http://more-id-4"),
                                IdWithinCategoryEq("http://more-id-5"),
                                IdWithinCategoryEq("http://more-id-6"),
                                IdWithinCategoryEq("http://more-id-7"),
                                IdWithinCategoryEq("http://more-id-8"),
                                IdWithinCategoryEq("http://more-id-9"),
                                IdWithinCategoryEq("http://more-id-10")));
      });
  LoadMoreFromJSONString(
      service.get(), articles_category(),
      GetTestJson({GetSnippetWithUrl("http://more-id-1"),
                   GetSnippetWithUrl("http://more-id-2"),
                   GetSnippetWithUrl("http://more-id-3"),
                   GetSnippetWithUrl("http://more-id-4"),
                   GetSnippetWithUrl("http://more-id-5"),
                   GetSnippetWithUrl("http://more-id-6"),
                   GetSnippetWithUrl("http://more-id-7"),
                   GetSnippetWithUrl("http://more-id-8"),
                   GetSnippetWithUrl("http://more-id-9"),
                   GetSnippetWithUrl("http://more-id-10")}),
      /*known_ids=*/{"http://id-1", "http://id-2", "http://id-3", "http://id-4",
                     "http://id-5", "http://id-6", "http://id-7", "http://id-8",
                     "http://id-9", "http://id-10"},
      expect_receiving_ten_new_snippets);
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              ElementsAre(IdWithinCategoryEq("http://more-id-1"),
                          IdWithinCategoryEq("http://more-id-2"),
                          IdWithinCategoryEq("http://more-id-3"),
                          IdWithinCategoryEq("http://more-id-4"),
                          IdWithinCategoryEq("http://more-id-5"),
                          IdWithinCategoryEq("http://more-id-6"),
                          IdWithinCategoryEq("http://more-id-7"),
                          IdWithinCategoryEq("http://more-id-8"),
                          IdWithinCategoryEq("http://more-id-9"),
                          IdWithinCategoryEq("http://more-id-10")));
  // Verify the superseded suggestions got marked as dismissed.
  EXPECT_THAT(
      service->GetDismissedSnippetsForTesting(articles_category()),
      ElementsAre(IdEq("http://id-1"), IdEq("http://id-2"), IdEq("http://id-3"),
                  IdEq("http://id-4"), IdEq("http://id-5"), IdEq("http://id-6"),
                  IdEq("http://id-7"), IdEq("http://id-8"), IdEq("http://id-9"),
                  IdEq("http://id-10")));
}

// TODO(tschumann): We don't have test making sure the RemoteSuggestionsFetcher
// actually gets the proper parameters. Add tests with an injected
// RemoteSuggestionsFetcher to verify the parameters, including proper handling
// of dismissed and known_ids.

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
  auto service = MakeSnippetsServiceWithoutInitialization();
  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  service->Fetch(articles_category(), std::set<std::string>(),
                 base::Bind(&SuggestionsLoaded, &loaded));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsProviderImplTest, ReturnTemporaryErrorForInvalidJson) {
  auto service = MakeSnippetsService();

  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  LoadMoreFromJSONString(service.get(), articles_category(),
                         "invalid json string}]}",
                         /*known_ids=*/std::set<std::string>(),
                         base::Bind(&SuggestionsLoaded, &loaded));
  EXPECT_THAT(suggestions_fetcher()->last_status(),
              StartsWith("Received invalid JSON"));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ReturnTemporaryErrorForInvalidSnippet) {
  auto service = MakeSnippetsService();

  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  LoadMoreFromJSONString(service.get(), articles_category(),
                         GetTestJson({GetIncompleteSnippet()}),
                         /*known_ids=*/std::set<std::string>(),
                         base::Bind(&SuggestionsLoaded, &loaded));
  EXPECT_THAT(suggestions_fetcher()->last_status(),
              StartsWith("Invalid / empty list"));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ReturnTemporaryErrorForRequestFailure) {
  // Created SnippetsService will fail by default with unsuccessful request.
  auto service = MakeSnippetsService(/*set_empty_response=*/false);

  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  service->Fetch(articles_category(),
                 /*known_ids=*/std::set<std::string>(),
                 base::Bind(&SuggestionsLoaded, &loaded));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsProviderImplTest, ReturnTemporaryErrorForHttpFailure) {
  auto service = MakeSnippetsService();
  SetUpHttpError();

  MockFunction<void(Status, const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(HasCode(StatusCode::TEMPORARY_ERROR), IsEmpty()));
  service->Fetch(articles_category(),
                 /*known_ids=*/std::set<std::string>(),
                 base::Bind(&SuggestionsLoaded, &loaded));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsProviderImplTest, LoadInvalidJson) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetInvalidSnippet()}));
  EXPECT_THAT(suggestions_fetcher()->last_status(),
              StartsWith("Received invalid JSON"));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, LoadInvalidJsonWithExistingSnippets) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
  ASSERT_EQ("OK", suggestions_fetcher()->last_status());

  LoadFromJSONString(service.get(), GetTestJson({GetInvalidSnippet()}));
  EXPECT_THAT(suggestions_fetcher()->last_status(),
              StartsWith("Received invalid JSON"));
  // This should not have changed the existing snippets.
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, LoadIncompleteJson) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetIncompleteSnippet()}));
  EXPECT_EQ("Invalid / empty list.", suggestions_fetcher()->last_status());
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       LoadIncompleteJsonWithExistingSnippets) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  LoadFromJSONString(service.get(), GetTestJson({GetIncompleteSnippet()}));
  EXPECT_EQ("Invalid / empty list.", suggestions_fetcher()->last_status());
  // This should not have changed the existing snippets.
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, Dismiss) {
  auto service = MakeSnippetsService();

  std::string json_str(
      GetTestJson({GetSnippetWithSources("http://site.com", "Source 1", "")}));

  LoadFromJSONString(service.get(), json_str);

  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
  // Load the image to store it in the database.
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &service->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(&cb, &ServeImageCallback::Run)));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  gfx::Image image = FetchImage(service.get(), MakeArticleID(kSnippetUrl));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Dismissing a non-existent snippet shouldn't do anything.
  service->DismissSuggestion(MakeArticleID("http://othersite.com"));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  // Dismiss the snippet.
  service->DismissSuggestion(MakeArticleID(kSnippetUrl));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());

  // Verify we can still load the image of the discarded snippet (other NTPs
  // might still reference it). This should come from the database -- no network
  // fetch necessary.
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  image = FetchImage(service.get(), MakeArticleID(kSnippetUrl));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Make sure that fetching the same snippet again does not re-add it.
  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());

  // The snippet should stay dismissed even after re-creating the service.
  ResetSnippetsService(&service, /*set_empty_response=*/true);
  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());

  // The snippet can be added again after clearing dismissed snippets.
  service->ClearDismissedSuggestionsForDebugging(articles_category());
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, GetDismissed) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));

  service->DismissSuggestion(MakeArticleID(kSnippetUrl));

  service->GetDismissedSuggestionsForDebugging(
      articles_category(),
      base::Bind(
          [](RemoteSuggestionsProviderImpl* service,
             RemoteSuggestionsProviderImplTest* test,
             std::vector<ContentSuggestion> dismissed_suggestions) {
            EXPECT_EQ(1u, dismissed_suggestions.size());
            for (auto& suggestion : dismissed_suggestions) {
              EXPECT_EQ(test->MakeArticleID(kSnippetUrl), suggestion.id());
            }
          },
          service.get(), this));
  base::RunLoop().RunUntilIdle();

  // There should be no dismissed snippet after clearing the list.
  service->ClearDismissedSuggestionsForDebugging(articles_category());
  service->GetDismissedSuggestionsForDebugging(
      articles_category(),
      base::Bind(
          [](RemoteSuggestionsProviderImpl* service,
             RemoteSuggestionsProviderImplTest* test,
             std::vector<ContentSuggestion> dismissed_suggestions) {
            EXPECT_EQ(0u, dismissed_suggestions.size());
          },
          service.get(), this));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsProviderImplTest, CreationTimestampParseFail) {
  auto service = MakeSnippetsService();

  std::string json =
      GetSnippetWithTimes(GetDefaultCreationTime(), GetDefaultExpirationTime());
  base::ReplaceFirstSubstringAfterOffset(
      &json, 0, FormatTime(GetDefaultCreationTime()), "aaa1448459205");
  std::string json_str(GetTestJson({json}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, RemoveExpiredDismissedContent) {
  auto service = MakeSnippetsService();

  std::string json_str1(GetTestJson({GetExpiredSnippet()}));
  // Load it.
  LoadFromJSONString(service.get(), json_str1);
  // Load the image to store it in the database.
  // TODO(tschumann): Introduce some abstraction to nicely work with image
  // fetching expectations.
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &service->GetImageFetcherForTesting());
  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(&cb, &ServeImageCallback::Run)));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));
  gfx::Image image = FetchImage(service.get(), MakeArticleID(kSnippetUrl));
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_EQ(1, image.Width());

  // Dismiss the suggestion
  service->DismissSuggestion(
      ContentSuggestion::ID(articles_category(), kSnippetUrl));

  // Load a different snippet - this will clear the expired dismissed ones.
  std::string json_str2(GetTestJson({GetSnippetWithUrl(kSnippetUrl2)}));
  LoadFromJSONString(service.get(), json_str2);

  EXPECT_THAT(service->GetDismissedSnippetsForTesting(articles_category()),
              IsEmpty());

  // Verify the image got removed, too.
  EXPECT_TRUE(FetchImage(service.get(), MakeArticleID(kSnippetUrl)).IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, ExpiredContentNotRemoved) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetExpiredSnippet()}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
}

TEST_F(RemoteSuggestionsProviderImplTest, TestSingleSource) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetSnippetWithSources(
      "http://source1.com", "Source 1", "http://source1.amp.com")}));

  LoadFromJSONString(service.get(), json_str);
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
  const NTPSnippet& snippet =
      *service->GetSnippetsForTesting(articles_category()).front();
  EXPECT_EQ(snippet.id(), kSnippetUrl);
  EXPECT_EQ(snippet.url(), GURL("http://source1.com"));
  EXPECT_EQ(snippet.publisher_name(), std::string("Source 1"));
  EXPECT_EQ(snippet.amp_url(), GURL("http://source1.amp.com"));
}

TEST_F(RemoteSuggestionsProviderImplTest, TestSingleSourceWithMalformedUrl) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetSnippetWithSources(
      "ceci n'est pas un url", "Source 1", "http://source1.amp.com")}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, TestSingleSourceWithMissingData) {
  auto service = MakeSnippetsService();

  std::string json_str(
      GetTestJson({GetSnippetWithSources("http://source1.com", "", "")}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, LogNumArticlesHistogram) {
  auto service = MakeSnippetsService();

  base::HistogramTester tester;
  LoadFromJSONString(service.get(), GetTestJson({GetInvalidSnippet()}));

  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));

  // Invalid JSON shouldn't contribute to NumArticlesFetched.
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              IsEmpty());

  // Valid JSON with empty list.
  LoadFromJSONString(service.get(), GetTestJson(std::vector<std::string>()));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));

  // Snippet list should be populated with size 1.
  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2),
                          base::Bucket(/*min=*/1, /*count=*/1)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/1)));

  // Duplicate snippet shouldn't increase the list size.
  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      IsEmpty());

  // Dismissing a snippet should decrease the list size. This will only be
  // logged after the next fetch.
  service->DismissSuggestion(MakeArticleID(kSnippetUrl));
  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/3),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  // Dismissed snippets shouldn't influence NumArticlesFetched.
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/3)));
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));

  // There is only a single, dismissed snippet in the database, so recreating
  // the service will require us to re-fetch.
  tester.ExpectTotalCount("NewTabPage.Snippets.NumArticlesFetched", 4);
  ResetSnippetsService(&service, /*set_empty_response=*/true);
  EXPECT_EQ(observer().StatusForCategory(articles_category()),
            CategoryStatus::AVAILABLE);
  tester.ExpectTotalCount("NewTabPage.Snippets.NumArticlesFetched", 5);
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      ElementsAre(base::Bucket(/*min=*/1, /*count=*/2)));

  // But if there's a non-dismissed snippet in the database, recreating it
  // shouldn't trigger a fetch.
  LoadFromJSONString(
      service.get(),
      GetTestJson({GetSnippetWithUrl("http://not-dismissed.com")}));
  tester.ExpectTotalCount("NewTabPage.Snippets.NumArticlesFetched", 6);
  ResetSnippetsService(&service, /*set_empty_response=*/true);
  tester.ExpectTotalCount("NewTabPage.Snippets.NumArticlesFetched", 6);
}

TEST_F(RemoteSuggestionsProviderImplTest, DismissShouldRespectAllKnownUrls) {
  auto service = MakeSnippetsService();

  const base::Time creation = GetDefaultCreationTime();
  const base::Time expiry = GetDefaultExpirationTime();
  const std::vector<std::string> source_urls = {
      "http://mashable.com/2016/05/11/stolen",
      "http://www.aol.com/article/2016/05/stolen-doggie"};
  const std::vector<std::string> publishers = {"Mashable", "AOL"};
  const std::vector<std::string> amp_urls = {
      "http://mashable-amphtml.googleusercontent.com/1",
      "http://t2.gstatic.com/images?q=tbn:3"};

  // Add the snippet from the mashable domain.
  LoadFromJSONString(service.get(),
                     GetTestJson({GetSnippetWithUrlAndTimesAndSource(
                         source_urls, source_urls[0], creation, expiry,
                         publishers[0], amp_urls[0])}));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
  // Dismiss the snippet via the mashable source corpus ID.
  service->DismissSuggestion(MakeArticleID(source_urls[0]));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());

  // The same article from the AOL domain should now be detected as dismissed.
  LoadFromJSONString(service.get(),
                     GetTestJson({GetSnippetWithUrlAndTimesAndSource(
                         source_urls, source_urls[1], creation, expiry,
                         publishers[1], amp_urls[1])}));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest, StatusChanges) {
  auto service = MakeSnippetsService();

  // Simulate user signed out
  SetUpFetchResponse(GetTestJson({GetSnippet()}));
  service->OnStatusChanged(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN,
                           RemoteSuggestionsStatus::SIGNED_OUT_AND_DISABLED);

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::SIGNED_OUT));
  EXPECT_THAT(RemoteSuggestionsProviderImpl::State::DISABLED,
              Eq(service->state_));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()),
              IsEmpty());  // No fetch should be made.

  // Simulate user sign in. The service should be ready again and load snippets.
  SetUpFetchResponse(GetTestJson({GetSnippet()}));
  service->OnStatusChanged(RemoteSuggestionsStatus::SIGNED_OUT_AND_DISABLED,
                           RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN);
  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE_LOADING));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(RemoteSuggestionsProviderImpl::State::READY, Eq(service->state_));
  EXPECT_FALSE(service->GetSnippetsForTesting(articles_category()).empty());
}

TEST_F(RemoteSuggestionsProviderImplTest, ImageReturnedWithTheSameId) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));

  gfx::Image image;
  MockFunction<void(const gfx::Image&)> image_fetched;
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &service->GetImageFetcherForTesting());
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _))
        .WillOnce(WithArgs<0, 2>(Invoke(&cb, &ServeImageCallback::Run)));
    EXPECT_CALL(image_fetched, Call(_)).WillOnce(SaveArg<0>(&image));
  }

  service->FetchSuggestionImage(
      MakeArticleID(kSnippetUrl),
      base::Bind(&MockFunction<void(const gfx::Image&)>::Call,
                 base::Unretained(&image_fetched)));
  base::RunLoop().RunUntilIdle();
  // Check that the image by ServeOneByOneImage is really served.
  EXPECT_EQ(1, image.Width());
}

TEST_F(RemoteSuggestionsProviderImplTest, EmptyImageReturnedForNonExistentId) {
  auto service = MakeSnippetsService();

  // Create a non-empty image so that we can test the image gets updated.
  gfx::Image image = gfx::test::CreateImage(1, 1);
  MockFunction<void(const gfx::Image&)> image_fetched;
  EXPECT_CALL(image_fetched, Call(_)).WillOnce(SaveArg<0>(&image));

  service->FetchSuggestionImage(
      MakeArticleID(kSnippetUrl2),
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
  auto service = MakeSnippetsService();

  ContentSuggestion::ID unknown_id = MakeArticleID(kSnippetUrl2);
  database()->SaveImage(unknown_id.id_within_category(), "some image blob");
  // Set up the image decoder to always return the 1x1 test image.
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));

  // Create a non-empty image so that we can test the image gets updated.
  gfx::Image image = gfx::test::CreateImage(2, 2);
  MockFunction<void(const gfx::Image&)> image_fetched;
  EXPECT_CALL(image_fetched, Call(_)).WillOnce(SaveArg<0>(&image));

  service->FetchSuggestionImage(
      MakeArticleID(kSnippetUrl2),
      base::Bind(&MockFunction<void(const gfx::Image&)>::Call,
                 base::Unretained(&image_fetched)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(image.IsEmpty()) << "got image with width: " << image.Width();
}

TEST_F(RemoteSuggestionsProviderImplTest, ClearHistoryRemovesAllSuggestions) {
  auto service = MakeSnippetsService();

  std::string first_snippet = GetSnippetWithUrl("http://url1.com");
  std::string second_snippet = GetSnippetWithUrl("http://url2.com");
  std::string json_str = GetTestJson({first_snippet, second_snippet});
  LoadFromJSONString(service.get(), json_str);
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(2));

  service->DismissSuggestion(MakeArticleID("http://url1.com"));
  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              Not(IsEmpty()));
  ASSERT_THAT(service->GetDismissedSnippetsForTesting(articles_category()),
              SizeIs(1));

  base::Time begin = base::Time::FromTimeT(123),
             end = base::Time::FromTimeT(456);
  base::Callback<bool(const GURL& url)> filter;
  service->ClearHistory(begin, end, filter);

  // Verify that the observer received the update with the empty data as well.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              IsEmpty());
  EXPECT_THAT(service->GetDismissedSnippetsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldKeepArticlesCategoryAvailableAfterClearHistory) {
  // If the provider marks that category as NOT_PROVIDED, then it won't be shown
  // at all in the UI and the user cannot load new data :-/.
  auto service = MakeSnippetsService();

  ASSERT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE));
  service->ClearHistory(base::Time::UnixEpoch(), base::Time::Max(),
                        base::Callback<bool(const GURL& url)>());

  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       SuggestionsFetchedOnSignInAndSignOut) {
  auto service = MakeSnippetsService();
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());

  // |MakeSnippetsService()| creates a service where user is signed in already,
  // so we start by signing out.
  SetUpFetchResponse(GetTestJson({GetSnippetN(1)}));
  service->OnStatusChanged(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN,
                           RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  // Sign in to check a transition from signed out to signed in.
  SetUpFetchResponse(GetTestJson({GetSnippetN(1), GetSnippetN(2)}));
  service->OnStatusChanged(RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT,
                           RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(2));
}

TEST_F(RemoteSuggestionsProviderImplTest, ShouldClearOrphanedImagesOnRestart) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  ServeImageCallback cb =
      base::Bind(&ServeOneByOneImage, &service->GetImageFetcherForTesting());

  EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(&cb, &ServeImageCallback::Run)));
  image_decoder()->SetDecodedImage(gfx::test::CreateImage(1, 1));

  gfx::Image image = FetchImage(service.get(), MakeArticleID(kSnippetUrl));
  EXPECT_EQ(1, image.Width());
  EXPECT_FALSE(image.IsEmpty());

  // Send new suggestion which don't include the snippet referencing the image.
  LoadFromJSONString(service.get(),
                     GetTestJson({GetSnippetWithUrl(
                         "http://something.com/pletely/unrelated")}));
  // The image should still be available until a restart happens.
  EXPECT_FALSE(FetchImage(service.get(), MakeArticleID(kSnippetUrl)).IsEmpty());
  ResetSnippetsService(&service, /*set_empty_response=*/true);
  // After the restart, the image should be garbage collected.
  EXPECT_TRUE(FetchImage(service.get(), MakeArticleID(kSnippetUrl)).IsEmpty());
}

TEST_F(RemoteSuggestionsProviderImplTest,
       ShouldHandleMoreThanMaxSnippetsInResponse) {
  auto service = MakeSnippetsService();

  std::vector<std::string> suggestions;
  for (int i = 0; i < service->GetMaxSnippetCountForTesting() + 1; ++i) {
    suggestions.push_back(GetSnippetWithUrl(
        base::StringPrintf("http://localhost/snippet-id-%d", i)));
  }
  LoadFromJSONString(service.get(), GetTestJson(suggestions));
  // TODO(tschumann): We should probably trim out any additional results and
  // only serve the MaxSnippetCount items.
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()),
              SizeIs(service->GetMaxSnippetCountForTesting() + 1));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       StoreLastSuccessfullBackgroundFetchTime) {
  // On initialization of the RemoteSuggestionsProviderImpl a background fetch
  // is triggered since the snippets DB is empty. Therefore the service must not
  // be initialized until the test clock is set.
  auto service = MakeSnippetsServiceWithoutInitialization();

  auto simple_test_clock = base::MakeUnique<base::SimpleTestClock>();
  base::SimpleTestClock* simple_test_clock_ptr = simple_test_clock.get();
  service->SetClockForTesting(std::move(simple_test_clock));

  // Test that the preference is correctly initialized with the default value 0.
  EXPECT_EQ(
      0, pref_service()->GetInt64(prefs::kLastSuccessfulBackgroundFetchTime));

  WaitForSnippetsServiceInitialization(service.get(),
                                       /*set_empty_response=*/true);
  EXPECT_EQ(
      simple_test_clock_ptr->Now().ToInternalValue(),
      pref_service()->GetInt64(prefs::kLastSuccessfulBackgroundFetchTime));

  // Advance the time and check whether the time was updated correctly after the
  // background fetch.
  simple_test_clock_ptr->Advance(TimeDelta::FromHours(1));

  service->RefetchInTheBackground(/*callback=*/nullptr);
  base::RunLoop().RunUntilIdle();
  // TODO(jkrcal): Move together with the pref storage into the scheduler.
  EXPECT_EQ(
      simple_test_clock_ptr->Now().ToInternalValue(),
      pref_service()->GetInt64(prefs::kLastSuccessfulBackgroundFetchTime));
  // TODO(markusheintz): Add a test that simulates a browser restart once the
  // scheduler refactoring is done (crbug.com/672434).
}

TEST_F(RemoteSuggestionsProviderImplTest, CallsProviderStatusCallbackIfReady) {
  // Initiate the service so that it is already READY.
  auto service = MakeSnippetsService();

  StrictMock<MockFunction<void(RemoteSuggestionsProvider::ProviderStatus)>>
      status_callback;
  // The callback should be called on registering.
  EXPECT_CALL(status_callback,
              Call(RemoteSuggestionsProvider::ProviderStatus::ACTIVE));
  service->SetProviderStatusCallback(
      base::MakeUnique<RemoteSuggestionsProvider::ProviderStatusCallback>(
          base::Bind(&MockFunction<void(
                         RemoteSuggestionsProvider::ProviderStatus)>::Call,
                     base::Unretained(&status_callback))));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       DoesNotCallProviderStatusCallbackIfNotInited) {
  auto service = MakeSnippetsServiceWithoutInitialization();

  StrictMock<MockFunction<void(RemoteSuggestionsProvider::ProviderStatus)>>
      status_callback;
  // The provider is not initialized yet, no callback should be called on
  // registering.
  service->SetProviderStatusCallback(
      base::MakeUnique<RemoteSuggestionsProvider::ProviderStatusCallback>(
          base::Bind(&MockFunction<void(
                         RemoteSuggestionsProvider::ProviderStatus)>::Call,
                     base::Unretained(&status_callback))));
}

TEST_F(RemoteSuggestionsProviderImplTest,
       CallsProviderStatusCallbackWhenReady) {
  auto service = MakeSnippetsServiceWithoutInitialization();
  StrictMock<MockFunction<void(RemoteSuggestionsProvider::ProviderStatus)>>
      status_callback;
  service->SetProviderStatusCallback(
      base::MakeUnique<RemoteSuggestionsProvider::ProviderStatusCallback>(
          base::Bind(&MockFunction<void(
                         RemoteSuggestionsProvider::ProviderStatus)>::Call,
                     base::Unretained(&status_callback))));

  // Should be called when becoming ready.
  EXPECT_CALL(status_callback,
              Call(RemoteSuggestionsProvider::ProviderStatus::ACTIVE));
  WaitForSnippetsServiceInitialization(service.get(),
                                       /*set_empty_response=*/true);
}

TEST_F(RemoteSuggestionsProviderImplTest, CallsProviderStatusCallbackOnError) {
  auto service = MakeSnippetsServiceWithoutInitialization();
  StrictMock<MockFunction<void(RemoteSuggestionsProvider::ProviderStatus)>>
      status_callback;
  service->SetProviderStatusCallback(
      base::MakeUnique<RemoteSuggestionsProvider::ProviderStatusCallback>(
          base::Bind(&MockFunction<void(
                         RemoteSuggestionsProvider::ProviderStatus)>::Call,
                     base::Unretained(&status_callback))));

  // Should be called on error.
  EXPECT_CALL(status_callback,
              Call(RemoteSuggestionsProvider::ProviderStatus::INACTIVE));
  service->EnterState(RemoteSuggestionsProviderImpl::State::ERROR_OCCURRED);
}

TEST_F(RemoteSuggestionsProviderImplTest,
       CallsProviderStatusCallbackWhenDisabled) {
  auto service = MakeSnippetsServiceWithoutInitialization();
  StrictMock<MockFunction<void(RemoteSuggestionsProvider::ProviderStatus)>>
      status_callback;
  service->SetProviderStatusCallback(
      base::MakeUnique<RemoteSuggestionsProvider::ProviderStatusCallback>(
          base::Bind(&MockFunction<void(
                         RemoteSuggestionsProvider::ProviderStatus)>::Call,
                     base::Unretained(&status_callback))));

  // Should be called when becoming disabled.
  EXPECT_CALL(status_callback,
              Call(RemoteSuggestionsProvider::ProviderStatus::INACTIVE));
  service->EnterState(RemoteSuggestionsProviderImpl::State::DISABLED);
}

}  // namespace ntp_snippets
