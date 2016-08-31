// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_service.h"

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
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/ntp_snippets_database.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/ntp_snippets_test_utils.h"
#include "components/ntp_snippets/switches.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
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
using testing::Return;
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

const base::Time::Exploded kDefaultCreationTime = {2015, 11, 4, 25, 13, 46, 45};
const char kTestContentSuggestionsServerEndpoint[] =
    "https://localunittest-chromecontentsuggestions-pa.googleapis.com/v1/"
    "suggestions/fetch";
const char kTestContentSuggestionsServerFormat[] =
    "https://localunittest-chromecontentsuggestions-pa.googleapis.com/v1/"
    "suggestions/fetch?key=%s";

const char kSnippetUrl[] = "http://localhost/foobar";
const char kSnippetTitle[] = "Title";
const char kSnippetText[] = "Snippet";
const char kSnippetSalientImage[] = "http://localhost/salient_image";
const char kSnippetPublisherName[] = "Foo News";
const char kSnippetAmpUrl[] = "http://localhost/amp";

const char kSnippetUrl2[] = "http://foo.com/bar";

base::Time GetDefaultCreationTime() {
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCExploded(kDefaultCreationTime, &out_time));
  return out_time;
}

base::Time GetDefaultExpirationTime() {
  return base::Time::Now() + base::TimeDelta::FromHours(1);
}

std::string GetTestJson(const std::vector<std::string>& snippets) {
  return base::StringPrintf(
      "{\n"
      "  \"categories\": [{\n"
      "    \"id\": 1,\n"
      "    \"localizedTitle\": \"Articles for You\",\n"
      "    \"suggestions\": [%s]\n"
      "  }]\n"
      "}\n",
      base::JoinString(snippets, ", ").c_str());
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

void ServeOneByOneImage(
    const std::string& id,
    base::Callback<void(const std::string&, const gfx::Image&)> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, id, gfx::test::CreateImage(1, 1)));
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
  MOCK_METHOD4(Schedule,
               bool(base::TimeDelta period_wifi_charging,
                    base::TimeDelta period_wifi,
                    base::TimeDelta period_fallback,
                    base::Time reschedule_time));
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
    loaded_.Signal();
    statuses_[category] = new_status;
  }

  void OnSuggestionInvalidated(ContentSuggestionsProvider* provider,
                               Category category,
                               const std::string& suggestion_id) override {}

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

}  // namespace

class NTPSnippetsServiceTest : public ::testing::Test {
 public:
  NTPSnippetsServiceTest()
      : params_manager_(ntp_snippets::kStudyName,
                        {{"content_suggestions_backend",
                          kTestContentSuggestionsServerEndpoint}}),
        fake_url_fetcher_factory_(
            /*default_factory=*/&failing_url_fetcher_factory_),
        test_url_(base::StringPrintf(kTestContentSuggestionsServerFormat,
                                     google_apis::GetAPIKey().c_str())),

        observer_(base::MakeUnique<FakeContentSuggestionsProviderObserver>()) {
    NTPSnippetsService::RegisterProfilePrefs(utils_.pref_service()->registry());
    RequestThrottler::RegisterProfilePrefs(utils_.pref_service()->registry());

    // Since no SuggestionsService is injected in tests, we need to force the
    // service to fetch from all hosts.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDontRestrict);
    EXPECT_TRUE(database_dir_.CreateUniqueTempDir());
  }

  ~NTPSnippetsServiceTest() override {
    // We need to run the message loop after deleting the database, because
    // ProtoDatabaseImpl deletes the actual LevelDB asynchronously on the task
    // runner. Without this, we'd get reports of memory leaks.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<NTPSnippetsService> MakeSnippetsService() {
    CHECK(!observer_->Loaded());

    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    scoped_refptr<net::TestURLRequestContextGetter> request_context_getter =
        new net::TestURLRequestContextGetter(task_runner.get());

    utils_.ResetSigninManager();
    std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher =
        base::MakeUnique<NTPSnippetsFetcher>(
            utils_.fake_signin_manager(), fake_token_service_.get(),
            std::move(request_context_getter), utils_.pref_service(),
            &category_factory_, base::Bind(&ParseJson),
            /*is_stable_channel=*/true);

    utils_.fake_signin_manager()->SignIn("foo@bar.com");
    snippets_fetcher->SetPersonalizationForTesting(
        NTPSnippetsFetcher::Personalization::kNonPersonal);

    auto image_fetcher = base::MakeUnique<NiceMock<MockImageFetcher>>();
    image_fetcher_ = image_fetcher.get();

    // Add an initial fetch response, as the service tries to fetch when there
    // is nothing in the DB.
    SetUpFetchResponse(GetTestJson(std::vector<std::string>()));

    auto service = base::MakeUnique<NTPSnippetsService>(
        observer_.get(), &category_factory_, utils_.pref_service(), nullptr,
        nullptr, "fr", &scheduler_, std::move(snippets_fetcher),
        std::move(image_fetcher), /*image_decoder=*/nullptr,
        base::MakeUnique<NTPSnippetsDatabase>(database_dir_.path(),
                                              task_runner),
        base::MakeUnique<NTPSnippetsStatusService>(utils_.fake_signin_manager(),
                                                   utils_.pref_service()));

    base::RunLoop().RunUntilIdle();
    observer_->WaitForLoad();
    return service;
  }

  void ResetSnippetsService(std::unique_ptr<NTPSnippetsService>* service) {
    service->reset();
    observer_ = base::MakeUnique<FakeContentSuggestionsProviderObserver>();
    *service = MakeSnippetsService();
  }

  std::string MakeUniqueID(const NTPSnippetsService& service,
                           const std::string& within_category_id) {
    return service.MakeUniqueID(articles_category(), within_category_id);
  }

  Category articles_category() {
    return category_factory_.FromKnownCategory(KnownCategories::ARTICLES);
  }

 protected:
  const GURL& test_url() { return test_url_; }
  FakeContentSuggestionsProviderObserver& observer() { return *observer_; }
  MockScheduler& mock_scheduler() { return scheduler_; }
  NiceMock<MockImageFetcher>* image_fetcher() { return image_fetcher_; }

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

 private:
  variations::testing::VariationParamsManager params_manager_;
  test::NTPSnippetsTestUtils utils_;
  base::MessageLoop message_loop_;
  FailingFakeURLFetcherFactory failing_url_fetcher_factory_;
  // Instantiation of factory automatically sets itself as URLFetcher's factory.
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  const GURL test_url_;
  std::unique_ptr<OAuth2TokenService> fake_token_service_;
  NiceMock<MockScheduler> scheduler_;
  std::unique_ptr<FakeContentSuggestionsProviderObserver> observer_;
  CategoryFactory category_factory_;
  NiceMock<MockImageFetcher>* image_fetcher_;

  base::ScopedTempDir database_dir_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsServiceTest);
};

TEST_F(NTPSnippetsServiceTest, ScheduleOnStart) {
  EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _));
  auto service = MakeSnippetsService();

  // When we have no snippets are all, loading the service initiates a fetch.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ("OK", service->snippets_fetcher()->last_status());
}

TEST_F(NTPSnippetsServiceTest, Full) {
  std::string json_str(GetTestJson({GetSnippet()}));

  EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _));
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), json_str);
  ASSERT_THAT(observer().SuggestionsForCategory(articles_category()),
              SizeIs(1));
  ASSERT_THAT(service->GetSnippetsForTesting(), SizeIs(1));
  const ContentSuggestion& suggestion =
      observer().SuggestionsForCategory(articles_category()).front();

  EXPECT_EQ(MakeUniqueID(*service, kSnippetUrl), suggestion.id());
  EXPECT_EQ(kSnippetTitle, base::UTF16ToUTF8(suggestion.title()));
  EXPECT_EQ(kSnippetText, base::UTF16ToUTF8(suggestion.snippet_text()));
  EXPECT_EQ(GetDefaultCreationTime(), suggestion.publish_date());
  EXPECT_EQ(kSnippetPublisherName,
            base::UTF16ToUTF8(suggestion.publisher_name()));
  EXPECT_EQ(GURL(kSnippetAmpUrl), suggestion.amp_url());
}

TEST_F(NTPSnippetsServiceTest, Clear) {
  EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _));
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetSnippet()}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(), SizeIs(1));

  service->ClearCachedSuggestions(articles_category());
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, InsertAtFront) {
  EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _));
  auto service = MakeSnippetsService();

  std::string first("http://first");
  LoadFromJSONString(service.get(), GetTestJson({GetSnippetWithUrl(first)}));
  EXPECT_THAT(service->GetSnippetsForTesting(), ElementsAre(IdEq(first)));

  std::string second("http://second");
  LoadFromJSONString(service.get(), GetTestJson({GetSnippetWithUrl(second)}));
  // The snippet loaded last should be at the first position in the list now.
  EXPECT_THAT(service->GetSnippetsForTesting(),
              ElementsAre(IdEq(second), IdEq(first)));
}

TEST_F(NTPSnippetsServiceTest, LimitNumSnippets) {
  auto service = MakeSnippetsService();

  int max_snippet_count = NTPSnippetsService::GetMaxSnippetCountForTesting();
  int snippets_per_load = max_snippet_count / 2 + 1;
  char url_format[] = "http://localhost/%i";

  std::vector<std::string> snippets1;
  std::vector<std::string> snippets2;
  for (int i = 0; i < snippets_per_load; i++) {
    snippets1.push_back(GetSnippetWithUrl(base::StringPrintf(url_format, i)));
    snippets2.push_back(GetSnippetWithUrl(
        base::StringPrintf(url_format, snippets_per_load + i)));
  }

  LoadFromJSONString(service.get(), GetTestJson(snippets1));
  ASSERT_THAT(service->GetSnippetsForTesting(), SizeIs(snippets1.size()));

  LoadFromJSONString(service.get(), GetTestJson(snippets2));
  EXPECT_THAT(service->GetSnippetsForTesting(), SizeIs(max_snippet_count));
}

TEST_F(NTPSnippetsServiceTest, LoadInvalidJson) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetInvalidSnippet()}));
  EXPECT_THAT(service->snippets_fetcher()->last_status(),
              StartsWith("Received invalid JSON"));
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, LoadInvalidJsonWithExistingSnippets) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  ASSERT_THAT(service->GetSnippetsForTesting(), SizeIs(1));
  ASSERT_EQ("OK", service->snippets_fetcher()->last_status());

  LoadFromJSONString(service.get(), GetTestJson({GetInvalidSnippet()}));
  EXPECT_THAT(service->snippets_fetcher()->last_status(),
              StartsWith("Received invalid JSON"));
  // This should not have changed the existing snippets.
  EXPECT_THAT(service->GetSnippetsForTesting(), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, LoadIncompleteJson) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetIncompleteSnippet()}));
  EXPECT_EQ("Invalid / empty list.",
            service->snippets_fetcher()->last_status());
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, LoadIncompleteJsonWithExistingSnippets) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));
  ASSERT_THAT(service->GetSnippetsForTesting(), SizeIs(1));

  LoadFromJSONString(service.get(), GetTestJson({GetIncompleteSnippet()}));
  EXPECT_EQ("Invalid / empty list.",
            service->snippets_fetcher()->last_status());
  // This should not have changed the existing snippets.
  EXPECT_THAT(service->GetSnippetsForTesting(), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, Dismiss) {
  EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _)).Times(2);
  auto service = MakeSnippetsService();

  std::string json_str(
      GetTestJson({GetSnippetWithSources("http://site.com", "Source 1", "")}));

  LoadFromJSONString(service.get(), json_str);

  ASSERT_THAT(service->GetSnippetsForTesting(), SizeIs(1));

  // Dismissing a non-existent snippet shouldn't do anything.
  service->DismissSuggestion(MakeUniqueID(*service, "http://othersite.com"));
  EXPECT_THAT(service->GetSnippetsForTesting(), SizeIs(1));

  // Dismiss the snippet.
  service->DismissSuggestion(MakeUniqueID(*service, kSnippetUrl));
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());

  // Make sure that fetching the same snippet again does not re-add it.
  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());

  // The snippet should stay dismissed even after re-creating the service.
  ResetSnippetsService(&service);
  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());

  // The snippet can be added again after clearing dismissed snippets.
  service->ClearDismissedSuggestionsForDebugging(articles_category());
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());
  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, GetDismissed) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));

  service->DismissSuggestion(MakeUniqueID(*service, kSnippetUrl));

  service->GetDismissedSuggestionsForDebugging(
      articles_category(),
      base::Bind(
          [](NTPSnippetsService* service, NTPSnippetsServiceTest* test,
             std::vector<ContentSuggestion> dismissed_suggestions) {
            EXPECT_EQ(1u, dismissed_suggestions.size());
            for (auto& suggestion : dismissed_suggestions) {
              EXPECT_EQ(test->MakeUniqueID(*service, kSnippetUrl),
                        suggestion.id());
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
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, RemoveExpiredContent) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetExpiredSnippet()}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, TestSingleSource) {
  auto service = MakeSnippetsService();

  std::string json_str(GetTestJson({GetSnippetWithSources(
      "http://source1.com", "Source 1", "http://source1.amp.com")}));

  LoadFromJSONString(service.get(), json_str);
  ASSERT_THAT(service->GetSnippetsForTesting(), SizeIs(1));
  const NTPSnippet& snippet = *service->GetSnippetsForTesting().front();
  EXPECT_EQ(snippet.sources().size(), 1u);
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
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, TestSingleSourceWithMissingData) {
  auto service = MakeSnippetsService();

  std::string json_str(
      GetTestJson({GetSnippetWithSources("http://source1.com", "", "")}));

  LoadFromJSONString(service.get(), json_str);
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());
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
  service->DismissSuggestion(MakeUniqueID(*service, kSnippetUrl));
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
  ASSERT_THAT(service->GetSnippetsForTesting(), SizeIs(1));
  // Dismiss the snippet via the mashable source corpus ID.
  service->DismissSuggestion(MakeUniqueID(*service, source_urls[0]));
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());

  // The same article from the AOL domain should now be detected as dismissed.
  LoadFromJSONString(service.get(),
                     GetTestJson({GetSnippetWithUrlAndTimesAndSource(
                         source_urls, source_urls[1], creation, expiry,
                         publishers[1], amp_urls[1])}));
  EXPECT_THAT(service->GetSnippetsForTesting(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, StatusChanges) {
  {
    InSequence s;
    EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _));
    EXPECT_CALL(mock_scheduler(), Unschedule());
    EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _));
  }
  auto service = MakeSnippetsService();

  // Simulate user signed out
  SetUpFetchResponse(GetTestJson({GetSnippet()}));
  service->OnDisabledReasonChanged(DisabledReason::SIGNED_OUT);

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::SIGNED_OUT));
  EXPECT_THAT(NTPSnippetsService::State::DISABLED, Eq(service->state_));
  EXPECT_THAT(service->GetSnippetsForTesting(),
              IsEmpty());  // No fetch should be made.

  // Simulate user sign in. The service should be ready again and load snippets.
  SetUpFetchResponse(GetTestJson({GetSnippet()}));
  service->OnDisabledReasonChanged(DisabledReason::NONE);
  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE_LOADING));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(observer().StatusForCategory(articles_category()),
              Eq(CategoryStatus::AVAILABLE));
  EXPECT_THAT(NTPSnippetsService::State::READY, Eq(service->state_));
  EXPECT_FALSE(service->GetSnippetsForTesting().empty());
}

TEST_F(NTPSnippetsServiceTest, ImageReturnedWithTheSameId) {
  auto service = MakeSnippetsService();

  LoadFromJSONString(service.get(), GetTestJson({GetSnippet()}));

  gfx::Image image;
  MockFunction<void(const gfx::Image&)> image_fetched;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher(), StartOrQueueNetworkRequest(_, _, _))
        .WillOnce(WithArgs<0, 2>(Invoke(ServeOneByOneImage)));
    EXPECT_CALL(image_fetched, Call(_)).WillOnce(SaveArg<0>(&image));
  }

  service->FetchSuggestionImage(
      MakeUniqueID(*service, kSnippetUrl),
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
      MakeUniqueID(*service, kSnippetUrl2),
      base::Bind(&MockFunction<void(const gfx::Image&)>::Call,
                 base::Unretained(&image_fetched)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(image.IsEmpty());
}

}  // namespace ntp_snippets
