// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/password_protection/password_protection_service.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPasswordReuseMatchWhitelistHistogramName[] =
    "PasswordManager.PasswordReuse.MainFrameMatchCsdWhitelist";
const char kWhitelistedUrl[] = "http://inwhitelist.com";
const char kNoneWhitelistedUrl[] = "http://notinwhitelist.com";
const char kRequestOutcomeHistogramName[] = "PasswordProtection.RequestOutcome";
const char kTargetUrl[] = "http://foo.com";

}  // namespace

namespace safe_browsing {

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD1(MatchCsdWhitelistUrl, bool(const GURL&));

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

class DummyURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  DummyURLRequestContextGetter()
      : dummy_task_runner_(new base::NullTaskRunner) {}

  net::URLRequestContext* GetURLRequestContext() override { return nullptr; }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return dummy_task_runner_;
  }

 private:
  ~DummyURLRequestContextGetter() override {}

  scoped_refptr<base::SingleThreadTaskRunner> dummy_task_runner_;
};

class TestPasswordProtectionService : public PasswordProtectionService {
 public:
  TestPasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<HostContentSettingsMap> content_setting_map)
      : PasswordProtectionService(database_manager, request_context_getter),
        content_setting_map_(content_setting_map) {}

  HostContentSettingsMap* GetSettingMapForActiveProfile() override {
    return content_setting_map_.get();
  }

  void RequestFinished(
      PasswordProtectionRequest* request,
      std::unique_ptr<LoginReputationClientResponse> response) override {
    latest_response_ = std::move(response);
  }

  LoginReputationClientResponse* latest_response() {
    return latest_response_.get();
  }

 private:
  std::unique_ptr<LoginReputationClientResponse> latest_response_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  DISALLOW_COPY_AND_ASSIGN(TestPasswordProtectionService);
};

class PasswordProtectionServiceTest : public testing::Test {
 public:
  PasswordProtectionServiceTest(){};

  LoginReputationClientResponse CreateVerdictProto(
      LoginReputationClientResponse::VerdictType verdict,
      int cache_duration_sec,
      const std::string& cache_expression,
      bool exact_match) {
    LoginReputationClientResponse verdict_proto;
    verdict_proto.set_verdict_type(verdict);
    verdict_proto.set_cache_duration_sec(cache_duration_sec);
    verdict_proto.set_cache_expression(cache_expression);
    verdict_proto.set_cache_expression_exact_match(exact_match);
    return verdict_proto;
  }

  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* incognito */, false /* guest_profile */);
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    dummy_request_context_getter_ = new DummyURLRequestContextGetter();
    password_protection_service_ =
        base::MakeUnique<TestPasswordProtectionService>(
            database_manager_, dummy_request_context_getter_,
            content_setting_map_);
  }

  void TearDown() override { content_setting_map_->ShutdownOnUIThread(); }

  // Sets up |database_manager_| and |requests_| as needed.
  void InitializeAndStartRequest(bool is_extended_reporting,
                                 bool incognito,
                                 bool match_whitelist,
                                 int timeout_in_ms) {
    GURL target_url(kTargetUrl);
    EXPECT_CALL(*database_manager_.get(), MatchCsdWhitelistUrl(target_url))
        .WillRepeatedly(testing::Return(match_whitelist));

    request_ = base::MakeUnique<PasswordProtectionRequest>(
        target_url, LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
        is_extended_reporting, incognito,
        password_protection_service_->GetWeakPtr(), timeout_in_ms);
    request_->Start();
  }

  bool PathVariantsMatchCacheExpression(const GURL& url,
                                        const std::string& cache_expression) {
    std::vector<std::string> paths;
    PasswordProtectionService::GeneratePathVariantsWithoutQuery(url, &paths);
    return PasswordProtectionService::PathVariantsMatchCacheExpression(
        paths,
        PasswordProtectionService::GetCacheExpressionPath(cache_expression));
  }

  bool PathMatchCacheExpressionExactly(const GURL& url,
                                       const std::string& cache_expression) {
    std::vector<std::string> paths;
    PasswordProtectionService::GeneratePathVariantsWithoutQuery(url, &paths);
    return PasswordProtectionService::PathMatchCacheExpressionExactly(
        paths,
        PasswordProtectionService::GetCacheExpressionPath(cache_expression));
  }

  void CacheVerdict(const GURL& url,
                    LoginReputationClientResponse::VerdictType verdict,
                    int cache_duration_sec,
                    const std::string& cache_expression,
                    bool exact_match,
                    const base::Time& verdict_received_time) {
    LoginReputationClientResponse response(CreateVerdictProto(
        verdict, cache_duration_sec, cache_expression, exact_match));
    password_protection_service_->CacheVerdict(
        url, &response, verdict_received_time, content_setting_map_.get());
  }

  size_t GetStoredVerdictCount() {
    return password_protection_service_->GetStoredVerdictCount();
  }

 protected:
  // |thread_bundle_| is needed here because this test involves both UI and IO
  // threads.
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  scoped_refptr<DummyURLRequestContextGetter> dummy_request_context_getter_;
  std::unique_ptr<TestPasswordProtectionService> password_protection_service_;
  std::unique_ptr<PasswordProtectionRequest> request_;
  base::HistogramTester histograms_;
};

TEST_F(PasswordProtectionServiceTest,
       TestPasswordReuseMatchWhitelistHistogram) {
  const GURL whitelisted_url(kWhitelistedUrl);
  const GURL not_whitelisted_url(kNoneWhitelistedUrl);
  EXPECT_CALL(*database_manager_.get(), MatchCsdWhitelistUrl(whitelisted_url))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*database_manager_.get(),
              MatchCsdWhitelistUrl(not_whitelisted_url))
      .WillOnce(testing::Return(false));
  histograms_.ExpectTotalCount(kPasswordReuseMatchWhitelistHistogramName, 0);

  // Empty url should not increment metric.
  password_protection_service_->RecordPasswordReuse(GURL());
  base::RunLoop().RunUntilIdle();
  histograms_.ExpectTotalCount(kPasswordReuseMatchWhitelistHistogramName, 0);

  // Whitelisted url should increase "True" bucket by 1.
  password_protection_service_->RecordPasswordReuse(whitelisted_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordReuseMatchWhitelistHistogramName),
      testing::ElementsAre(base::Bucket(1, 1)));

  // Non-whitelisted url should increase "False" bucket by 1.
  password_protection_service_->RecordPasswordReuse(not_whitelisted_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordReuseMatchWhitelistHistogramName),
      testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestParseInvalidVerdictEntry) {
  std::unique_ptr<base::DictionaryValue> invalid_verdict_entry =
      base::MakeUnique<base::DictionaryValue>();
  invalid_verdict_entry->SetString("cache_creation_time", "invalid_time");

  int cache_creation_time;
  LoginReputationClientResponse response;
  // ParseVerdictEntry fails if input is empty.
  EXPECT_FALSE(PasswordProtectionService::ParseVerdictEntry(
      nullptr, &cache_creation_time, &response));

  // ParseVerdictEntry fails if the input dict value is invalid.
  EXPECT_FALSE(PasswordProtectionService::ParseVerdictEntry(
      invalid_verdict_entry.get(), &cache_creation_time, &response));
}

TEST_F(PasswordProtectionServiceTest, TestParseValidVerdictEntry) {
  base::Time expected_creation_time = base::Time::Now();
  LoginReputationClientResponse expected_verdict(CreateVerdictProto(
      LoginReputationClientResponse::SAFE, 10 * 60, "test.com/foo", true));
  std::unique_ptr<base::DictionaryValue> valid_verdict_entry =
      PasswordProtectionService::CreateDictionaryFromVerdict(
          &expected_verdict, expected_creation_time);

  int actual_cache_creation_time;
  LoginReputationClientResponse actual_verdict;
  ASSERT_TRUE(PasswordProtectionService::ParseVerdictEntry(
      valid_verdict_entry.get(), &actual_cache_creation_time, &actual_verdict));

  EXPECT_EQ(static_cast<int>(expected_creation_time.ToDoubleT()),
            actual_cache_creation_time);
  EXPECT_EQ(expected_verdict.cache_duration_sec(),
            actual_verdict.cache_duration_sec());
  EXPECT_EQ(expected_verdict.verdict_type(), actual_verdict.verdict_type());
  EXPECT_EQ(expected_verdict.cache_expression(),
            actual_verdict.cache_expression());
  EXPECT_EQ(expected_verdict.cache_expression_exact_match(),
            actual_verdict.cache_expression_exact_match());
}

TEST_F(PasswordProtectionServiceTest, TestPathVariantsMatchCacheExpression) {
  // Cache expression without path.
  std::string cache_expression("google.com");
  std::string cache_expression_with_slash("google.com/");
  EXPECT_TRUE(PathVariantsMatchCacheExpression(GURL("https://www.google.com"),
                                               cache_expression));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(GURL("https://www.google.com"),
                                               cache_expression_with_slash));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(GURL("https://www.google.com/"),
                                               cache_expression));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(GURL("https://www.google.com/"),
                                               cache_expression_with_slash));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(
      GURL("https://www.google.com/maps/local/"), cache_expression));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(
      GURL("https://www.google.com/maps/local/"), cache_expression_with_slash));

  // Cache expression with path.
  cache_expression = "evil.com/bad";
  cache_expression_with_slash = "evil.com/bad/";
  EXPECT_TRUE(PathVariantsMatchCacheExpression(GURL("http://evil.com/bad/"),
                                               cache_expression));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(GURL("http://evil.com/bad/"),
                                               cache_expression_with_slash));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(
      GURL("http://evil.com/bad/index.html"), cache_expression));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(
      GURL("http://evil.com/bad/index.html"), cache_expression_with_slash));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(
      GURL("http://evil.com/bad/foo/index.html"), cache_expression));
  EXPECT_TRUE(PathVariantsMatchCacheExpression(
      GURL("http://evil.com/bad/foo/index.html"), cache_expression_with_slash));
  EXPECT_FALSE(PathVariantsMatchCacheExpression(
      GURL("http://evil.com/worse/index.html"), cache_expression));
  EXPECT_FALSE(PathVariantsMatchCacheExpression(
      GURL("http://evil.com/worse/index.html"), cache_expression_with_slash));
}

TEST_F(PasswordProtectionServiceTest, TestPathMatchCacheExpressionExactly) {
  // Cache expression without path.
  std::string cache_expression("www.google.com");
  EXPECT_TRUE(PathMatchCacheExpressionExactly(GURL("https://www.google.com"),
                                              cache_expression));
  EXPECT_TRUE(PathMatchCacheExpressionExactly(GURL("https://www.google.com/"),
                                              cache_expression));
  EXPECT_TRUE(PathMatchCacheExpressionExactly(
      GURL("https://www.google.com/index.html"), cache_expression));
  EXPECT_FALSE(PathMatchCacheExpressionExactly(
      GURL("https://www.google.com/abc/"), cache_expression));
  EXPECT_FALSE(PathMatchCacheExpressionExactly(
      GURL("https://www.google.com/def/login"), cache_expression));

  // Cache expression with path.
  cache_expression = "evil.com/bad";
  EXPECT_FALSE(PathMatchCacheExpressionExactly(GURL("http://evil.com"),
                                               cache_expression));
  EXPECT_FALSE(PathMatchCacheExpressionExactly(GURL("http://evil.com/"),
                                               cache_expression));
  EXPECT_TRUE(PathMatchCacheExpressionExactly(GURL("http://evil.com/bad/"),
                                              cache_expression));
  EXPECT_TRUE(PathMatchCacheExpressionExactly(
      GURL("http://evil.com/bad/index.html"), cache_expression));
  EXPECT_FALSE(PathMatchCacheExpressionExactly(GURL("http://evil.com/bad/abc/"),
                                               cache_expression));
  EXPECT_FALSE(PathMatchCacheExpressionExactly(
      GURL("http://evil.com/bad/abc/login.jsp"), cache_expression));
}

TEST_F(PasswordProtectionServiceTest, TestCachedVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount());
  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/foo",
               false, base::Time::Now());

  EXPECT_EQ(1U, GetStoredVerdictCount());

  // Cache another verdict with the some origin and cache_expression should
  // override the cache.
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientResponse::PHISHING, 10 * 60, "test.com/foo",
               false, base::Time::Now());
  EXPECT_EQ(1U, GetStoredVerdictCount());
  LoginReputationClientResponse out_verdict;
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(),
                GURL("http://www.test.com/foo/index2.html"), &out_verdict));

  // Cache another verdict with the same origin but different cache_expression
  // will not increase setting count, but will increase the number of verdicts
  // in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/bar",
               false, base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount());
}

TEST_F(PasswordProtectionServiceTest, TestGetCachedVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount());
  // Prepare 3 verdicts of the same origin with different cache expressions:
  // (1) require exact match, not expired.
  // (2) not require exact match, not expired.
  // (3) require exact match, expired.
  base::Time now = base::Time::Now();
  CacheVerdict(GURL("http://test.com/login.html"),
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com", true,
               now);
  CacheVerdict(GURL("http://test.com/abc/index.jsp"),
               LoginReputationClientResponse::LOW_REPUTATION, 10 * 60,
               "test.com/abc", false, now);
  CacheVerdict(
      GURL("http://test.com/def/index.jsp"),
      LoginReputationClientResponse::PHISHING, 10 * 60, "test.com/def", false,
      base::Time::FromDoubleT(now.ToDoubleT() -
                              24.0 * 60.0 * 60.0));  // Yesterday, expired.
  ASSERT_EQ(3U, GetStoredVerdictCount());

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL with unknown origin.
  LoginReputationClientResponse actual_verdict;
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(), GURL("http://www.unknown.com/"),
                &actual_verdict));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL with http://test.com
  // origin, but doesn't match any known cache_expression.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(), GURL("http://test.com/xyz/foo.jsp"),
                &actual_verdict));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL whose variants match
  // test.com/def, since corresponding entry is expired.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(),
                GURL("http://test.com/def/ghi/index.html"), &actual_verdict));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL whose variants match
  // test.com, but not match it exactly. Return SAFE if it is a exact match of
  // test.com.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(),
                GURL("http://test.com/ghi/index.html"), &actual_verdict));
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(),
                GURL("http://test.com/term_of_service.html"), &actual_verdict));

  // Return LOW_REPUTATION if look up for a URL whose variants match
  // test.com/abc.
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(), GURL("http://test.com/abc/"),
                &actual_verdict));
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(), GURL("http://test.com/abc/bar.jsp"),
                &actual_verdict));
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(),
                GURL("http://test.com/abc/foo/bar.html"), &actual_verdict));
}

TEST_F(PasswordProtectionServiceTest, TestCleanUpCachedVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount());
  // Prepare 2 verdicts. One is for origin "http://foo.com", and the other is
  // for "http://bar.com".
  base::Time now = base::Time::Now();
  CacheVerdict(GURL("http://foo.com/abc/index.jsp"),
               LoginReputationClientResponse::LOW_REPUTATION, 10 * 60,
               "foo.com/abc", false, now);
  CacheVerdict(GURL("http://bar.com/index.jsp"),
               LoginReputationClientResponse::PHISHING, 10 * 60, "bar.com",
               false, now);
  ASSERT_EQ(2U, GetStoredVerdictCount());

  // Delete a bar.com URL. Corresponding content setting keyed on
  // origin "http://bar.com" should be removed,
  history::URLRows deleted_urls;
  deleted_urls.push_back(history::URLRow(GURL("http://bar.com")));
  password_protection_service_->RemoveContentSettingsOnURLsDeleted(
      false /* all_history */, deleted_urls, content_setting_map_.get());
  EXPECT_EQ(1U, GetStoredVerdictCount());
  LoginReputationClientResponse actual_verdict;
  EXPECT_EQ(
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      password_protection_service_->GetCachedVerdict(
          content_setting_map_.get(), GURL("http://bar.com"), &actual_verdict));

  // If delete all history. All password protection content settings should be
  // gone.
  password_protection_service_->RemoveContentSettingsOnURLsDeleted(
      true /* all_history */, history::URLRows(), content_setting_map_.get());
  EXPECT_EQ(0U, GetStoredVerdictCount());
}

TEST_F(PasswordProtectionServiceTest, TestNoRequestSentForIncognito) {
  histograms_.ExpectTotalCount(kRequestOutcomeHistogramName, 0);
  InitializeAndStartRequest(true /* extended_reporting */, true /* incognito */,
                            false /* match whitelist */, 10 /* timeout */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(histograms_.GetAllSamples(kRequestOutcomeHistogramName),
              testing::ElementsAre(base::Bucket(7 /* INCOGNITO */, 1)));
}

TEST_F(PasswordProtectionServiceTest,
       TestNoRequestSentForNonExtendedReporting) {
  histograms_.ExpectTotalCount(kRequestOutcomeHistogramName, 0);

  InitializeAndStartRequest(false /* extended_reporting */,
                            false /* incognito */, false /* match whitelist */,
                            10000 /* timeout in ms*/);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kRequestOutcomeHistogramName),
      testing::ElementsAre(base::Bucket(6 /* NO_EXTENDED_REPORTING */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestNoRequestSentForWhitelistedURL) {
  histograms_.ExpectTotalCount(kRequestOutcomeHistogramName, 0);
  InitializeAndStartRequest(true /* extended_reporting */,
                            false /* incognito */, true /* match whitelist */,
                            10000 /* timeout in ms*/);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(histograms_.GetAllSamples(kRequestOutcomeHistogramName),
              testing::ElementsAre(base::Bucket(4 /* MATCHED_WHITELIST */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestNoRequestSentIfVerdictAlreadyCached) {
  histograms_.ExpectTotalCount(kRequestOutcomeHistogramName, 0);
  CacheVerdict(GURL(kTargetUrl), LoginReputationClientResponse::LOW_REPUTATION,
               600, GURL(kTargetUrl).host(), true, base::Time::Now());
  InitializeAndStartRequest(true /* extended_reporting */,
                            false /* incognito */, false /* match whitelist */,
                            10000 /* timeout in ms*/);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms_.GetAllSamples(kRequestOutcomeHistogramName),
      testing::ElementsAre(base::Bucket(5 /* RESPONSE_ALREADY_CACHED */, 1)));
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            password_protection_service_->latest_response()->verdict_type());
}

TEST_F(PasswordProtectionServiceTest, TestResponseFetchFailed) {
  histograms_.ExpectTotalCount(kRequestOutcomeHistogramName, 0);
  net::TestURLFetcher failed_fetcher(0, GURL("http://bar.com"), nullptr);
  // Set up failed response.
  failed_fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));

  InitializeAndStartRequest(true /* extended_reporting */,
                            false /* incognito */, false /* match whitelist */,
                            10000 /* timeout in ms*/);
  request_->OnURLFetchComplete(&failed_fetcher);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(histograms_.GetAllSamples(kRequestOutcomeHistogramName),
              testing::ElementsAre(base::Bucket(9 /* FETCH_FAILED */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestMalformedResponse) {
  histograms_.ExpectTotalCount(kRequestOutcomeHistogramName, 0);
  // Set up malformed response.
  net::TestURLFetcher fetcher(0, GURL("http://bar.com"), nullptr);
  fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  fetcher.set_response_code(200);
  fetcher.SetResponseString("invalid response");

  InitializeAndStartRequest(true /* extended_reporting */,
                            false /* incognito */, false /* match whitelist */,
                            10000 /* timeout in ms*/);
  request_->OnURLFetchComplete(&fetcher);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kRequestOutcomeHistogramName),
      testing::ElementsAre(base::Bucket(10 /* RESPONSE_MALFORMED */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestRequestTimedout) {
  histograms_.ExpectTotalCount(kRequestOutcomeHistogramName, 0);
  InitializeAndStartRequest(true /* extended_reporting */,
                            false /* incognito */, false /* match whitelist */,
                            0 /* timeout immediately */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(histograms_.GetAllSamples(kRequestOutcomeHistogramName),
              testing::ElementsAre(base::Bucket(3 /* TIMEDOUT */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestRequestAndResponseSuccessfull) {
  histograms_.ExpectTotalCount(kRequestOutcomeHistogramName, 0);
  // Set up valid response.
  net::TestURLFetcher fetcher(0, GURL("http://bar.com"), nullptr);
  fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  fetcher.set_response_code(200);
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING, 600,
                         GURL(kTargetUrl).host(), true);
  fetcher.SetResponseString(expected_response.SerializeAsString());

  InitializeAndStartRequest(true /* extended_reporting */,
                            false /* incognito */, false /* match whitelist */,
                            10000 /* timeout in ms*/);
  request_->OnURLFetchComplete(&fetcher);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(histograms_.GetAllSamples(kRequestOutcomeHistogramName),
              testing::ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  LoginReputationClientResponse* actual_response =
      password_protection_service_->latest_response();
  EXPECT_EQ(expected_response.verdict_type(), actual_response->verdict_type());
  EXPECT_EQ(expected_response.cache_expression(),
            actual_response->cache_expression());
  EXPECT_EQ(expected_response.cache_expression_exact_match(),
            actual_response->cache_expression_exact_match());
  EXPECT_EQ(expected_response.cache_duration_sec(),
            actual_response->cache_duration_sec());
}

TEST_F(PasswordProtectionServiceTest, TestTearDownWithPendingRequests) {
  histograms_.ExpectTotalCount(kRequestOutcomeHistogramName, 0);
  GURL target_url(kTargetUrl);
  EXPECT_CALL(*database_manager_.get(), MatchCsdWhitelistUrl(target_url))
      .WillRepeatedly(testing::Return(false));
  password_protection_service_->StartRequest(
      target_url, LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      true /* extended_reporting */, false /* incognito */);

  // Destroy password_protection_service_ while there is one request pending.
  password_protection_service_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(histograms_.GetAllSamples(kRequestOutcomeHistogramName),
              testing::ElementsAre(base::Bucket(2 /* CANCELED */, 1)));
}
}  // namespace safe_browsing
