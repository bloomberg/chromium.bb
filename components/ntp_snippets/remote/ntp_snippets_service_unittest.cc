// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/ntp_snippets_service.h"

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
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/ntp_snippet.h"
#include "components/ntp_snippets/remote/ntp_snippets_database.h"
#include "components/ntp_snippets/remote/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/remote/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/remote/ntp_snippets_test_utils.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/variations/variations_associated_data.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using image_fetcher::ImageFetcher;
using image_fetcher::ImageFetcherDelegate;
using testing::ElementsAre;
using testing::Eq;
using testing::InSequence;
using testing::Invoke;
using testing::IsEmpty;
using testing::Mock;
using testing::MockFunction;
using testing::NiceMock;
using testing::SaveArg;
using testing::SizeIs;
using testing::StartsWith;
using testing::WithArgs;
using testing::_;

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

std::string GetTestJson(const std::vector<std::string>& snippets,
                        const std::string& category_title) {
  return base::StringPrintf(
      "{\n"
      "  \"categories\": [{\n"
      "    \"id\": 1,\n"
      "    \"localizedTitle\": \"%s\",\n"
      "    \"suggestions\": [%s]\n"
      "  }]\n"
      "}\n",
      category_title.c_str(),
      base::JoinString(snippets, ", ").c_str());
}

std::string GetTestJson(const std::vector<std::string>& snippets) {
  return GetTestJson(snippets, kTestJsonDefaultCategoryTitle);
}

std::string GetTestJsonWithoutTitle(const std::vector<std::string>& snippets) {
  return GetTestJson(snippets, std::string());
}

std::string GetMultiCategoryJson(const std::vector<std::string>& articles,
                                 const std::vector<std::string>& others,
                                 int other_id = 2) {
  return base::StringPrintf(
      "{\n"
      "  \"categories\": [{\n"
      "    \"id\": 1,\n"
      "    \"localizedTitle\": \"Articles for You\",\n"
      "    \"suggestions\": [%s]\n"
      "  }, {\n"
      "    \"id\": %i,\n"
      "    \"localizedTitle\": \"Other Things\",\n"
      "    \"suggestions\": [%s]\n"
      "  }]\n"
      "}\n",
      base::JoinString(articles, ", ").c_str(), other_id,
      base::JoinString(others, ", ").c_str());
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

void ParseJson(
    const std::string& json,
    const ntp_snippets::NTPSnippetsFetcher::SuccessCallback& success_callback,
    const ntp_snippets::NTPSnippetsFetcher::ErrorCallback& error_callback) {
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
      int id, const GURL& url, net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) override {
    return base::MakeUnique<net::FakeURLFetcher>(
        url, d, /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
        net::URLRequestStatus::FAILED);
  }
};

class MockScheduler : public NTPSnippetsScheduler {
 public:
  MOCK_METHOD2(Schedule,
               bool(base::TimeDelta period_wifi,
                    base::TimeDelta period_fallback));
  MOCK_METHOD0(Unschedule, bool());
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

class FakeContentSuggestionsProviderObserver
    : public ContentSuggestionsProvider::Observer {
 public:
  FakeContentSuggestionsProviderObserver()
      : loaded_(base::WaitableEvent::ResetPolicy::MANUAL,
                base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void OnNewSuggestions(ContentSuggestionsProvider* provider,
                        Category category,
                        std::vector<ContentSuggestion> suggestions) override {
    suggestions_[category] = std::move(suggestions);
  }

  void OnCategoryStatusChanged(ContentSuggestionsProvider* provider,
                               Category category,
                               CategoryStatus new_status) override {
    if (category.IsKnownCategory(KnownCategories::ARTICLES) &&
        IsCategoryStatusAvailable(new_status)) {
      loaded_.Signal();
    }
    statuses_[category] = new_status;
  }

  void OnSuggestionInvalidated(
      ContentSuggestionsProvider* provider,
      const ContentSuggestion::ID& suggestion_id) override {}

  const std::map<Category, CategoryStatus, Category::CompareByID>& statuses()
      const {
    return statuses_;
  }

  CategoryStatus StatusForCategory(Category category) const {
    auto it = statuses_.find(category);
    if (it == statuses_.end()) {
      return CategoryStatus::NOT_PROVIDED;
    }
    return it->second;
  }

  const std::vector<ContentSuggestion>& SuggestionsForCategory(
      Category category) {
    return suggestions_[category];
  }

  void WaitForLoad() { loaded_.Wait(); }
  bool Loaded() { return loaded_.IsSignaled(); }

  void Reset() {
    loaded_.Reset();
    statuses_.clear();
  }

 private:
  base::WaitableEvent loaded_;
  std::map<Category, CategoryStatus, Category::CompareByID> statuses_;
  std::map<Category, std::vector<ContentSuggestion>, Category::CompareByID>
      suggestions_;

  DISALLOW_COPY_AND_ASSIGN(FakeContentSuggestionsProviderObserver);
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

class NTPSnippetsServiceTest : public ::testing::Test {
 public:
  NTPSnippetsServiceTest()
      : params_manager_(ntp_snippets::kStudyName,
                        {{"content_suggestions_backend",
                          kTestContentSuggestionsServerEndpoint}}),
        fake_url_fetcher_factory_(
            /*default_factory=*/&failing_url_fetcher_factory_),
        test_url_(kTestContentSuggestionsServerWithAPIKey),
        user_classifier_(/*pref_service=*/nullptr),
        image_fetcher_(nullptr),
        image_decoder_(nullptr) {
    NTPSnippetsService::RegisterProfilePrefs(utils_.pref_service()->registry());
    RequestThrottler::RegisterProfilePrefs(utils_.pref_service()->registry());

    EXPECT_TRUE(database_dir_.CreateUniqueTempDir());
  }

  ~NTPSnippetsServiceTest() override {
    // We need to run the message loop after deleting the database, because
    // ProtoDatabaseImpl deletes the actual LevelDB asynchronously on the task
    // runner. Without this, we'd get reports of memory leaks.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<NTPSnippetsService> MakeSnippetsService(
      bool set_empty_response = true) {
    auto service = MakeSnippetsServiceWithoutInitialization();
    WaitForSnippetsServiceInitialization(set_empty_response);
    return service;
  }

  std::unique_ptr<NTPSnippetsService>
  MakeSnippetsServiceWithoutInitialization() {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    scoped_refptr<net::TestURLRequestContextGetter> request_context_getter =
        new net::TestURLRequestContextGetter(task_runner.get());

    utils_.ResetSigninManager();
    std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher =
        base::MakeUnique<NTPSnippetsFetcher>(
            utils_.fake_signin_manager(), fake_token_service_.get(),
            std::move(request_context_getter), utils_.pref_service(),
            &category_factory_, nullptr, base::Bind(&ParseJson), kAPIKey,
            &user_classifier_);

    utils_.fake_signin_manager()->SignIn("foo@bar.com");
    snippets_fetcher->SetPersonalizationForTesting(
        NTPSnippetsFetcher::Personalization::kNonPersonal);

    auto image_fetcher = base::MakeUnique<NiceMock<MockImageFetcher>>();

    image_fetcher_ = image_fetcher.get();
    EXPECT_CALL(*image_fetcher, SetImageFetcherDelegate(_));
    auto image_decoder = base::MakeUnique<FakeImageDecoder>();
    image_decoder_ = image_decoder.get();
    EXPECT_FALSE(observer_);
    observer_ = base::MakeUnique<FakeContentSuggestionsProviderObserver>();
    return base::MakeUnique<NTPSnippetsService>(
        observer_.get(), &category_factory_, utils_.pref_service(), "fr",
        &user_classifier_, &scheduler_, std::move(snippets_fetcher),
        std::move(image_fetcher), std::move(image_decoder),
        base::MakeUnique<NTPSnippetsDatabase>(database_dir_.GetPath(),
                                              task_runner),
        base::MakeUnique<NTPSnippetsStatusService>(utils_.fake_signin_manager(),
                                                   utils_.pref_service()));
  }

  void WaitForSnippetsServiceInitialization(bool set_empty_response = true) {
    EXPECT_TRUE(observer_);
    EXPECT_FALSE(observer_->Loaded());

    // Add an initial fetch response, as the service tries to fetch when there
    // is nothing in the DB.
    if (set_empty_response)
      SetUpFetchResponse(GetTestJsonWithoutTitle(std::vector<std::string>()));

    base::RunLoop().RunUntilIdle();
    observer_->WaitForLoad();
  }

  void ResetSnippetsService(std::unique_ptr<NTPSnippetsService>* service) {
    service->reset();
    observer_.reset();
    *service = MakeSnippetsService();
  }

  ContentSuggestion::ID MakeArticleID(const std::string& id_within_category) {
    return ContentSuggestion::ID(articles_category(), id_within_category);
  }

  Category articles_category() {
    return category_factory_.FromKnownCategory(KnownCategories::ARTICLES);
  }

  ContentSuggestion::ID MakeOtherID(const std::string& id_within_category) {
    return ContentSuggestion::ID(other_category(), id_within_category);
  }

  Category other_category() { return category_factory_.FromRemoteCategory(2); }

  Category unknown_category() {
    return category_factory_.FromRemoteCategory(kUnknownRemoteCategoryId);
  }

 protected:
  const GURL& test_url() { return test_url_; }
  FakeContentSuggestionsProviderObserver& observer() { return *observer_; }
  MockScheduler& mock_scheduler() { return scheduler_; }
  NiceMock<MockImageFetcher>* image_fetcher() { return image_fetcher_; }
  FakeImageDecoder* image_decoder() { return image_decoder_; }

  // Provide the json to be returned by the fake fetcher.
  void SetUpFetchResponse(const std::string& json) {
    fake_url_fetcher_factory_.SetFakeResponse(test_url_, json, net::HTTP_OK,
                                              net::URLRequestStatus::SUCCESS);
  }

  void LoadFromJSONString(NTPSnippetsService* service,
                          const std::string& json) {
    SetUpFetchResponse(json);
    service->FetchSnippets(true);
    base::RunLoop().RunUntilIdle();
  }

  void LoadMoreFromJSONString(NTPSnippetsService* service,
                              const Category& category,
                              const std::string& json,
                              const std::set<std::string>& known_ids,
                              NTPSnippetsService::FetchingCallback callback) {
    SetUpFetchResponse(json);
    service->Fetch(category, known_ids, callback);
    base::RunLoop().RunUntilIdle();
  }

 private:
  variations::testing::VariationParamsManager params_manager_;
  test::NTPSnippetsTestUtils utils_;
  base::MessageLoop message_loop_;
  FailingFakeURLFetcherFactory failing_url_fetcher_factory_;
  // Instantiation of factory automatically sets itself as URLFetcher's factory.
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  const GURL test_url_;
  std::unique_ptr<OAuth2TokenService> fake_token_service_;
  UserClassifier user_classifier_;
  NiceMock<MockScheduler> scheduler_;
  std::unique_ptr<FakeContentSuggestionsProviderObserver> observer_;
  CategoryFactory category_factory_;
  NiceMock<MockImageFetcher>* image_fetcher_;
  FakeImageDecoder* image_decoder_;

  base::ScopedTempDir database_dir_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsServiceTest);
};

TEST_F(NTPSnippetsServiceTest, ScheduleOnStart) {
  // We should get two |Schedule| calls: The first when initialization
  // completes, the second one after the automatic (since the service doesn't
  // have any data yet) fetch finishes.
  EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(2);
  EXPECT_CALL(mock_scheduler(), Unschedule()).Times(0);
  auto service = MakeSnippetsService();

  // When we have no snippets are all, loading the service initiates a fetch.
  EXPECT_EQ("OK", service->snippets_fetcher()->last_status());
}

TEST_F(NTPSnippetsServiceTest, DontRescheduleOnStart) {
  EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(2);
  EXPECT_CALL(mock_scheduler(), Unschedule()).Times(0);
  SetUpFetchResponse(GetTestJson({GetSnippet()}));
  auto service = MakeSnippetsService(/*set_empty_response=*/false);

  // When recreating the service, we should not get any |Schedule| calls:
  // The tasks are already scheduled with the correct intervals, so nothing on
  // initialization, and the service has data from the DB, so no automatic fetch
  // should happen.
  Mock::VerifyAndClearExpectations(&mock_scheduler());
  EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(0);
  EXPECT_CALL(mock_scheduler(), Unschedule()).Times(0);
  ResetSnippetsService(&service);
}

TEST_F(NTPSnippetsServiceTest, RescheduleAfterSuccessfulFetch) {
  // We should get two |Schedule| calls: The first when initialization
  // completes, the second one after the automatic (since the service doesn't
  // have any data yet) fetch finishes.
  EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(2);
  auto service = MakeSnippetsService();

  // A successful fetch should trigger another |Schedule|.
  EXPECT_CALL(mock_scheduler(), Schedule(_, _));
  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
}

TEST_F(NTPSnippetsServiceTest, DontRescheduleAfterFailedFetch) {
  // We should get two |Schedule| calls: The first when initialization
  // completes, the second one after the automatic (since the service doesn't
  // have any data yet) fetch finishes.
  EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(2);
  auto service = MakeSnippetsService();

  // A failed fetch should NOT trigger another |Schedule|.
  EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(0);
  LoadFromJSONString(service.get(), GetTestJson({GetInvalidSnippet()}));
}

TEST_F(NTPSnippetsServiceTest, IgnoreRescheduleBeforeInit) {
  // We should get two |Schedule| calls: The first when initialization
  // completes, the second one after the automatic (since the service doesn't
  // have any data yet) fetch finishes.
  EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(2);
  // The |RescheduleFetching| call shouldn't do anything (in particular not
  // result in an |Unschedule|), since the service isn't initialized yet.
  EXPECT_CALL(mock_scheduler(), Unschedule()).Times(0);
  auto service = MakeSnippetsServiceWithoutInitialization();
  service->RescheduleFetching(false);
  WaitForSnippetsServiceInitialization();
}

TEST_F(NTPSnippetsServiceTest, HandleForcedRescheduleBeforeInit) {
  {
    InSequence s;
    // The |RescheduleFetching| call with force=true should result in an
    // |Unschedule|, since the service isn't initialized yet.
    EXPECT_CALL(mock_scheduler(), Unschedule()).Times(1);
    // We should get two |Schedule| calls: The first when initialization
    // completes, the second one after the automatic (since the service doesn't
    // have any data yet) fetch finishes.
    EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(2);
  }
  auto service = MakeSnippetsServiceWithoutInitialization();
  service->RescheduleFetching(true);
  WaitForSnippetsServiceInitialization();
}

TEST_F(NTPSnippetsServiceTest, RescheduleOnStateChange) {
  {
    InSequence s;
    // Initial startup.
    EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(2);
    // Service gets disabled.
    EXPECT_CALL(mock_scheduler(), Unschedule());
    // Service gets enabled again.
    EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(2);
  }
  auto service = MakeSnippetsService();
  ASSERT_TRUE(service->ready());

  service->OnSnippetsStatusChanged(SnippetsStatus::ENABLED_AND_SIGNED_IN,
                                   SnippetsStatus::EXPLICITLY_DISABLED);
  ASSERT_FALSE(service->ready());
  base::RunLoop().RunUntilIdle();

  service->OnSnippetsStatusChanged(SnippetsStatus::EXPLICITLY_DISABLED,
                                   SnippetsStatus::ENABLED_AND_SIGNED_OUT);
  ASSERT_TRUE(service->ready());
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsServiceTest, DontUnscheduleOnShutdown) {
  EXPECT_CALL(mock_scheduler(), Schedule(_, _)).Times(2);
  EXPECT_CALL(mock_scheduler(), Unschedule()).Times(0);

  auto service = MakeSnippetsService();

  service.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsServiceTest, Full) {
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
  EXPECT_EQ(GURL(kSnippetAmpUrl), suggestion.amp_url());
}

TEST_F(NTPSnippetsServiceTest, CategoryTitle) {
  const base::string16 response_title =
      base::UTF8ToUTF16(kTestJsonDefaultCategoryTitle);

  auto service = MakeSnippetsService();

  // The articles category should be there by default, and have a title.
  CategoryInfo info_before = service->GetCategoryInfo(articles_category());
  ASSERT_FALSE(info_before.title().empty());
  ASSERT_NE(info_before.title(), response_title);

  std::string json_str_no_title(GetTestJsonWithoutTitle({GetSnippet()}));
  LoadFromJSONString(service.get(), json_str_no_title);

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  // The response didn't contain a category title. Make sure we didn't touch
  // the existing one.
  CategoryInfo info_no_title = service->GetCategoryInfo(articles_category());
  EXPECT_EQ(info_before.title(), info_no_title.title());

  std::string json_str_with_title(GetTestJson({GetSnippet()}));
  LoadFromJSONString(service.get(), json_str_with_title);

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  // This time, the response contained a title, |kTestJsonDefaultCategoryTitle|.
  // Make sure we updated the title in the CategoryInfo.
  CategoryInfo info_with_title = service->GetCategoryInfo(articles_category());
  EXPECT_NE(info_before.title(), info_with_title.title());
  EXPECT_EQ(response_title, info_with_title.title());
}

TEST_F(NTPSnippetsServiceTest, MultipleCategories) {
  std::string json_str(
      GetMultiCategoryJson({GetSnippetN(0)}, {GetSnippetN(1)}));

  auto service = MakeSnippetsService();

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
    EXPECT_EQ(GURL(kSnippetAmpUrl), suggestion.amp_url());
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
    EXPECT_EQ(GURL(kSnippetAmpUrl), suggestion.amp_url());
  }
}

TEST_F(NTPSnippetsServiceTest, PersistCategoryInfos) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(),
                     GetMultiCategoryJson({GetSnippetN(0)}, {GetSnippetN(1)},
                                          kUnknownRemoteCategoryId));

  ASSERT_EQ(observer().StatusForCategory(articles_category()),
            CategoryStatus::AVAILABLE);
  ASSERT_EQ(observer().StatusForCategory(unknown_category()),
            CategoryStatus::AVAILABLE);

  CategoryInfo info_articles_before =
      service->GetCategoryInfo(articles_category());
  CategoryInfo info_unknown_before =
      service->GetCategoryInfo(unknown_category());

  // Recreate the service to simulate a Chrome restart.
  ResetSnippetsService(&service);

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

TEST_F(NTPSnippetsServiceTest, PersistSuggestions) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(),
                     GetMultiCategoryJson({GetSnippetN(0)}, {GetSnippetN(1)}));

  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));

  // Recreate the service to simulate a Chrome restart.
  ResetSnippetsService(&service);

  // The suggestions in both categories should have been restored.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  EXPECT_THAT(observer().SuggestionsForCategory(other_category()), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, Clear) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetSnippet()}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  service->ClearCachedSuggestions(articles_category());
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, ReplaceSnippets) {
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

TEST_F(NTPSnippetsServiceTest, LoadsAdditionalSnippets) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(),
                     GetTestJson({GetSnippetWithUrl("http://first")}));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()),
              ElementsAre(IdEq("http://first")));

  auto expect_only_second_suggestion_received =
      base::Bind([](std::vector<ContentSuggestion> suggestions) {
        EXPECT_THAT(suggestions, SizeIs(1));
        EXPECT_THAT(suggestions[0].id().id_within_category(),
                    Eq("http://second"));
      });
  LoadMoreFromJSONString(service.get(), articles_category(),
                         GetTestJson({GetSnippetWithUrl("http://second")}),
                         /*known_ids=*/std::set<std::string>(),
                         expect_only_second_suggestion_received);

  // Verify that the observer did not get updated about the additional snippets.
  // Rationale: It's not clear that snippets fetched in one context (i.e. one
  // potentially old NTP) should be shown in other NTPs as well.
  // We can revisit this decision, but for now it's easiest to keep them
  // independent.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              ElementsAre(IdWithinCategoryEq("http://first")));
}

// TODO(tschumann): We don't have test making sure the NTPSnippetsFetcher
// actually gets the proper parameters. Add tests with an injected
// NTPSnippetsFetcher to verify the parameters, including proper handling of
// dismissed and known_ids.

namespace {

// Workaround for gMock's lack of support for movable types.
void SuggestionsLoaded(
    MockFunction<void(const std::vector<ContentSuggestion>& v)>* loaded,
    std::vector<ContentSuggestion> v) {
  loaded->Call(v);
}

}  // namespace

TEST_F(NTPSnippetsServiceTest, InvokesOnlyCallbackOnFetchingMore) {
  auto service = MakeSnippetsService();

  MockFunction<void(const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(SizeIs(1)));

  LoadMoreFromJSONString(service.get(), articles_category(),
                         GetTestJson({GetSnippetWithUrl("http://some")}),
                         std::set<std::string>(),
                         base::Bind(&SuggestionsLoaded, &loaded));

  // The observer shouldn't have been triggered.
  EXPECT_THAT(observer().SuggestionsForCategory(articles_category()),
              IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, ReturnFetchRequestEmptyBeforeInit) {
  auto service = MakeSnippetsServiceWithoutInitialization();
  MockFunction<void(const std::vector<ContentSuggestion>&)> loaded;
  EXPECT_CALL(loaded, Call(SizeIs(0)));
  service->Fetch(articles_category(), std::set<std::string>(),
                 base::Bind(&SuggestionsLoaded, &loaded));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsServiceTest, LoadInvalidJson) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetInvalidSnippet()}));
  EXPECT_THAT(service->snippets_fetcher()->last_status(),
              StartsWith("Received invalid JSON"));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, LoadInvalidJsonWithExistingSnippets) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
  ASSERT_EQ("OK", service->snippets_fetcher()->last_status());

  LoadFromJSONString(service.get(), GetTestJson({GetInvalidSnippet()}));
  EXPECT_THAT(service->snippets_fetcher()->last_status(),
              StartsWith("Received invalid JSON"));
  // This should not have changed the existing snippets.
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, LoadIncompleteJson) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetIncompleteSnippet()}));
  EXPECT_EQ("Invalid / empty list.",
            service->snippets_fetcher()->last_status());
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, LoadIncompleteJsonWithExistingSnippets) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  LoadFromJSONString(service.get(), GetTestJson({GetIncompleteSnippet()}));
  EXPECT_EQ("Invalid / empty list.",
            service->snippets_fetcher()->last_status());
  // This should not have changed the existing snippets.
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, Dismiss) {
  auto service = MakeSnippetsService();

  std::string json_str(
      GetTestJson({GetSnippetWithSources("http://site.com", "Source 1", "")}));

  LoadFromJSONString(service.get(), json_str);

  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  // Dismissing a non-existent snippet shouldn't do anything.
  service->DismissSuggestion(MakeArticleID("http://othersite.com"));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  // Dismiss the snippet.
  service->DismissSuggestion(MakeArticleID(kSnippetUrl));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());

  // Make sure that fetching the same snippet again does not re-add it.
  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());

  // The snippet should stay dismissed even after re-creating the service.
  ResetSnippetsService(&service);
  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());

  // The snippet can be added again after clearing dismissed snippets.
  service->ClearDismissedSuggestionsForDebugging(articles_category());
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, GetDismissed) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));

  service->DismissSuggestion(MakeArticleID(kSnippetUrl));

  service->GetDismissedSuggestionsForDebugging(
      articles_category(),
      base::Bind(
          [](NTPSnippetsService* service, NTPSnippetsServiceTest* test,
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
          [](NTPSnippetsService* service, NTPSnippetsServiceTest* test,
             std::vector<ContentSuggestion> dismissed_suggestions) {
            EXPECT_EQ(0u, dismissed_suggestions.size());
          },
          service.get(), this));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsServiceTest, CreationTimestampParseFail) {
  auto service = MakeSnippetsService();

  std::string json =
      GetSnippetWithTimes(GetDefaultCreationTime(), GetDefaultExpirationTime());
  base::ReplaceFirstSubstringAfterOffset(
      &json, 0, FormatTime(GetDefaultCreationTime()), "aaa1448459205");
  std::string json_str(GetTestJson({json}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, RemoveExpiredDismissedContent) {
  auto service = MakeSnippetsService();

  std::string json_str1(GetTestJson({GetExpiredSnippet()}));
  // Load it.
  LoadFromJSONString(service.get(), json_str1);
  // Dismiss the suggestion
  service->DismissSuggestion(
      ContentSuggestion::ID(articles_category(), kSnippetUrl));

  // Load a different snippet - this will clear the expired dismissed ones.
  std::string json_str2(GetTestJson({GetSnippetWithUrl(kSnippetUrl2)}));
  LoadFromJSONString(service.get(), json_str2);

  EXPECT_THAT(service->GetDismissedSnippetsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, ExpiredContentNotRemoved) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetExpiredSnippet()}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, TestSingleSource) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetSnippetWithSources(
      "http://source1.com", "Source 1", "http://source1.amp.com")}));

  LoadFromJSONString(service.get(), json_str);
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
  const NTPSnippet& snippet =
      *service->GetSnippetsForTesting(articles_category()).front();
  EXPECT_EQ(snippet.id(), kSnippetUrl);
  EXPECT_EQ(snippet.best_source().url, GURL("http://source1.com"));
  EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 1"));
  EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source1.amp.com"));
}

TEST_F(NTPSnippetsServiceTest, TestSingleSourceWithMalformedUrl) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetSnippetWithSources(
      "ceci n'est pas un url", "Source 1", "http://source1.amp.com")}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, TestSingleSourceWithMissingData) {
  auto service = MakeSnippetsService();

  std::string json_str(
      GetTestJson({GetSnippetWithSources("http://source1.com", "", "")}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, LogNumArticlesHistogram) {
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
  ResetSnippetsService(&service);
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
  ResetSnippetsService(&service);
  tester.ExpectTotalCount("NewTabPage.Snippets.NumArticlesFetched", 6);
}

TEST_F(NTPSnippetsServiceTest, DismissShouldRespectAllKnownUrls) {
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

TEST_F(NTPSnippetsServiceTest, StatusChanges) {
  auto service = MakeSnippetsService();

  // Simulate user signed out
  SetUpFetchResponse(GetTestJson({GetSnippet()}));
  service->OnSnippetsStatusChanged(SnippetsStatus::ENABLED_AND_SIGNED_IN,
                                   SnippetsStatus::SIGNED_OUT_AND_DISABLED);

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::SIGNED_OUT));
  EXPECT_THAT(NTPSnippetsService::State::DISABLED, Eq(service->state_));
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()),
              IsEmpty());  // No fetch should be made.

  // Simulate user sign in. The service should be ready again and load snippets.
  SetUpFetchResponse(GetTestJson({GetSnippet()}));
  service->OnSnippetsStatusChanged(SnippetsStatus::SIGNED_OUT_AND_DISABLED,
                                   SnippetsStatus::ENABLED_AND_SIGNED_IN);
  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE_LOADING));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(NTPSnippetsService::State::READY, Eq(service->state_));
  EXPECT_FALSE(service->GetSnippetsForTesting(articles_category()).empty());
}

TEST_F(NTPSnippetsServiceTest, ImageReturnedWithTheSameId) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));

  gfx::Image image;
  MockFunction<void(const gfx::Image&)> image_fetched;
  ServeImageCallback cb = base::Bind(&ServeOneByOneImage, service.get());
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

TEST_F(NTPSnippetsServiceTest, EmptyImageReturnedForNonExistentId) {
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

TEST_F(NTPSnippetsServiceTest, ClearHistoryRemovesAllSuggestions) {
  auto service = MakeSnippetsService();

  std::string first_snippet = GetSnippetWithUrl("http://url1.com");
  std::string second_snippet = GetSnippetWithUrl("http://url2.com");
  std::string json_str = GetTestJson({first_snippet, second_snippet});
  LoadFromJSONString(service.get(), json_str);
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(2));

  service->DismissSuggestion(MakeArticleID("http://url1.com"));
  ASSERT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));
  ASSERT_THAT(service->GetDismissedSnippetsForTesting(articles_category()),
              SizeIs(1));

  base::Time begin = base::Time::FromTimeT(123),
             end = base::Time::FromTimeT(456);
  base::Callback<bool(const GURL& url)> filter;
  service->ClearHistory(begin, end, filter);

  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), IsEmpty());
  EXPECT_THAT(service->GetDismissedSnippetsForTesting(articles_category()),
              IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, SuggestionsFetchedOnSignInAndSignOut) {
  auto service = MakeSnippetsService();
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(0));

  // |MakeSnippetsService()| creates a service where user is signed in already,
  // so we start by signing out.
  SetUpFetchResponse(GetTestJson({GetSnippetN(1)}));
  service->OnSnippetsStatusChanged(SnippetsStatus::ENABLED_AND_SIGNED_IN,
                                   SnippetsStatus::ENABLED_AND_SIGNED_OUT);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(1));

  // Sign in to check a transition from signed out to signed in.
  SetUpFetchResponse(GetTestJson({GetSnippetN(1), GetSnippetN(2)}));
  service->OnSnippetsStatusChanged(SnippetsStatus::ENABLED_AND_SIGNED_OUT,
                                   SnippetsStatus::ENABLED_AND_SIGNED_IN);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()), SizeIs(2));
}

namespace {

gfx::Image FetchImage(NTPSnippetsService* service,
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

}  // namespace

TEST_F(NTPSnippetsServiceTest, ShouldClearOrphanedImagesOnRestart) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  ServeImageCallback cb = base::Bind(&ServeOneByOneImage, service.get());

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
  ResetSnippetsService(&service);
  // After the restart, the image should be garbage collected.
  EXPECT_TRUE(FetchImage(service.get(), MakeArticleID(kSnippetUrl)).IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, ShouldHandleMoreThanMaxSnippetsInResponse) {
  auto service = MakeSnippetsService();

  std::vector<std::string> suggestions;
  for (int i = 0 ; i < service->GetMaxSnippetCountForTesting() + 1; ++i) {
    suggestions.push_back(GetSnippetWithUrl(
        base::StringPrintf("http://localhost/snippet-id-%d", i)));
  }
  LoadFromJSONString(service.get(), GetTestJson(suggestions));
  // TODO(tschumann): We should probably trim out any additional results and
  // only serve the MaxSnippetCount items.
  EXPECT_THAT(service->GetSnippetsForTesting(articles_category()),
              SizeIs(service->GetMaxSnippetCountForTesting() + 1));
}

}  // namespace ntp_snippets
