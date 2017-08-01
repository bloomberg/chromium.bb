// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/password_protection/password_protection_service.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::Return;

namespace {

const char kFormActionUrl[] = "https://form_action.com/";
const char kPasswordFrameUrl[] = "https://password_frame.com/";
const char kSavedDomain[] = "saved_domain.com";
const char kTargetUrl[] = "http://foo.com/";

}  // namespace

namespace safe_browsing {

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD2(CheckCsdWhitelistUrl,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));

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
      : PasswordProtectionService(database_manager,
                                  request_context_getter,
                                  nullptr,
                                  content_setting_map.get()),
        is_extended_reporting_(true),
        is_incognito_(false),
        latest_request_(nullptr) {}

  void RequestFinished(
      PasswordProtectionRequest* request,
      bool already_cached_unused,
      std::unique_ptr<LoginReputationClientResponse> response) override {
    latest_request_ = request;
    latest_response_ = std::move(response);
  }

  // Since referrer chain logic has been thoroughly tested in
  // SBNavigationObserverBrowserTest class, we intentionally leave this function
  // as a no-op here.
  void FillReferrerChain(const GURL& event_url,
                         int event_tab_id,
                         LoginReputationClientRequest::Frame* frame) override {}

  bool IsExtendedReporting() override { return is_extended_reporting_; }

  bool IsIncognito() override { return is_incognito_; }

  void set_extended_reporting(bool enabled) {
    is_extended_reporting_ = enabled;
  }

  void set_incognito(bool enabled) { is_incognito_ = enabled; }

  bool IsPingingEnabled(const base::Feature& feature,
                        RequestOutcome* reason) override {
    checked_feature_name_ = feature.name;
    return true;
  }

  void ShowPhishingInterstitial(const GURL& phishing_url,
                                const std::string& token,
                                content::WebContents* web_contents) override {}

  bool IsHistorySyncEnabled() override { return false; }

  LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
  GetSyncAccountType() override {
    return LoginReputationClientRequest::PasswordReuseEvent::NOT_SIGNED_IN;
  }

  LoginReputationClientResponse* latest_response() {
    return latest_response_.get();
  }

  ~TestPasswordProtectionService() override {}

  size_t GetPendingRequestsCount() { return requests_.size(); }

  const LoginReputationClientRequest* GetLatestRequestProto() {
    return latest_request_ ? latest_request_->request_proto() : nullptr;
  }

  std::string checked_feature_name() { return checked_feature_name_; }

 private:
  bool is_extended_reporting_;
  bool is_incognito_;
  PasswordProtectionRequest* latest_request_;
  std::unique_ptr<LoginReputationClientResponse> latest_response_;
  std::string checked_feature_name_;
  DISALLOW_COPY_AND_ASSIGN(TestPasswordProtectionService);
};

class PasswordProtectionServiceTest : public testing::Test {
 public:
  PasswordProtectionServiceTest(){};

  LoginReputationClientResponse CreateVerdictProto(
      LoginReputationClientResponse::VerdictType verdict,
      int cache_duration_sec,
      const std::string& cache_expression) {
    LoginReputationClientResponse verdict_proto;
    verdict_proto.set_verdict_type(verdict);
    verdict_proto.set_cache_duration_sec(cache_duration_sec);
    verdict_proto.set_cache_expression(cache_expression);
    return verdict_proto;
  }

  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* incognito */, false /* guest_profile */,
        false /* store_last_modified */);
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    dummy_request_context_getter_ = new DummyURLRequestContextGetter();
    password_protection_service_ =
        base::MakeUnique<TestPasswordProtectionService>(
            database_manager_, dummy_request_context_getter_,
            content_setting_map_);
  }

  void TearDown() override { content_setting_map_->ShutdownOnUIThread(); }

  // Sets up |database_manager_| and |requests_| as needed.
  void InitializeAndStartPasswordOnFocusRequest(bool match_whitelist,
                                                int timeout_in_ms) {
    GURL target_url(kTargetUrl);
    EXPECT_CALL(*database_manager_.get(), CheckCsdWhitelistUrl(target_url, _))
        .WillRepeatedly(
            Return(match_whitelist ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH));

    request_ = new PasswordProtectionRequest(
        nullptr, target_url, GURL(kFormActionUrl), GURL(kPasswordFrameUrl),
        std::string(), LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
        true, password_protection_service_.get(), timeout_in_ms);
    request_->Start();
  }

  void InitializeAndStartPasswordEntryRequest(const std::string& saved_domain,
                                              bool match_whitelist,
                                              int timeout_in_ms) {
    GURL target_url(kTargetUrl);
    EXPECT_CALL(*database_manager_.get(), CheckCsdWhitelistUrl(target_url, _))
        .WillRepeatedly(
            Return(match_whitelist ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH));

    request_ = new PasswordProtectionRequest(
        nullptr, target_url, GURL(), GURL(), saved_domain,
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT, true,
        password_protection_service_.get(), timeout_in_ms);
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

  void CacheVerdict(const GURL& url,
                    LoginReputationClientRequest::TriggerType trigger,
                    LoginReputationClientResponse::VerdictType verdict,
                    int cache_duration_sec,
                    const std::string& cache_expression,
                    const base::Time& verdict_received_time) {
    LoginReputationClientResponse response(
        CreateVerdictProto(verdict, cache_duration_sec, cache_expression));
    password_protection_service_->CacheVerdict(url, trigger, &response,
                                               verdict_received_time);
  }

  void CacheInvalidVerdict() {
    GURL invalid_hostname("http://invalid.com");
    std::unique_ptr<base::DictionaryValue> verdict_dictionary =
        base::DictionaryValue::From(content_setting_map_->GetWebsiteSetting(
            invalid_hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
            std::string(), nullptr));

    if (!verdict_dictionary.get())
      verdict_dictionary = base::MakeUnique<base::DictionaryValue>();

    std::unique_ptr<base::DictionaryValue> invalid_verdict_entry =
        base::MakeUnique<base::DictionaryValue>();
    invalid_verdict_entry->SetString("invalid", "invalid_string");

    verdict_dictionary->SetWithoutPathExpansion(
        "invalid_cache_expression", std::move(invalid_verdict_entry));
    content_setting_map_->SetWebsiteSettingDefaultScope(
        invalid_hostname, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
        std::string(), std::move(verdict_dictionary));
  }

  size_t GetStoredVerdictCount(LoginReputationClientRequest::TriggerType type) {
    return password_protection_service_->GetStoredVerdictCount(type);
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
  scoped_refptr<PasswordProtectionRequest> request_;
  base::HistogramTester histograms_;
};

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
      LoginReputationClientResponse::SAFE, 10 * 60, "test.com/foo"));
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

TEST_F(PasswordProtectionServiceTest, TestCachePasswordReuseVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/foo",
               base::Time::Now());

  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Cache another verdict with the some origin and cache_expression should
  // override the cache.
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::PHISHING, 10 * 60, "test.com/foo",
               base::Time::Now());
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  LoginReputationClientResponse out_verdict;
  EXPECT_EQ(
      LoginReputationClientResponse::PHISHING,
      password_protection_service_->GetCachedVerdict(
          GURL("http://www.test.com/foo/index2.html"),
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &out_verdict));

  // Cache another verdict with the same origin but different cache_expression
  // will not increase setting count, but will increase the number of verdicts
  // in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/bar",
               base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Now cache a UNFAMILIAR_LOGIN_PAGE verdict, stored verdict count for
  // PASSWORD_REUSE_EVENT should be the same.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/foobar",
               base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
}

TEST_F(PasswordProtectionServiceTest, TestCacheUnfamiliarLoginVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/foo",
               base::Time::Now());

  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Cache another verdict with the same origin but different cache_expression
  // will not increase setting count, but will increase the number of verdicts
  // in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/bar",
               base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Now cache a PASSWORD_REUSE_EVENT verdict, stored verdict count for
  // UNFAMILIAR_LOGIN_PAGE should be the same.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/foobar",
               base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_F(PasswordProtectionServiceTest, TestGetCachedVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  // Prepare 3 verdicts of the same origin with different cache expressions,
  // one is expired, one is not, the other is of a different type.
  base::Time now = base::Time::Now();
  CacheVerdict(GURL("http://test.com/login.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com", now);
  CacheVerdict(
      GURL("http://test.com/def/index.jsp"),
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      LoginReputationClientResponse::PHISHING, 10 * 60, "test.com/def",
      base::Time::FromDoubleT(now.ToDoubleT() -
                              24.0 * 60.0 * 60.0));  // Yesterday, expired.
  CacheVerdict(GURL("http://test.com/bar/login.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               LoginReputationClientResponse::PHISHING, 10 * 60, "test.com/bar",
               now);

  ASSERT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL with unknown origin.
  LoginReputationClientResponse actual_verdict;
  EXPECT_EQ(
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      password_protection_service_->GetCachedVerdict(
          GURL("http://www.unknown.com/"),
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &actual_verdict));

  // Return SAFE if look up for a URL that matches "test.com" cache expression.
  EXPECT_EQ(
      LoginReputationClientResponse::SAFE,
      password_protection_service_->GetCachedVerdict(
          GURL("http://test.com/xyz/foo.jsp"),
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &actual_verdict));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL whose variants match
  // test.com/def, but the corresponding verdict is expired.
  EXPECT_EQ(
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      password_protection_service_->GetCachedVerdict(
          GURL("http://test.com/def/ghi/index.html"),
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &actual_verdict));
}

TEST_F(PasswordProtectionServiceTest, TestRemoveCachedVerdictOnURLsDeleted) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  // Prepare 2 verdicts. One is for origin "http://foo.com", and the other is
  // for "http://bar.com".
  base::Time now = base::Time::Now();
  CacheVerdict(GURL("http://foo.com/abc/index.jsp"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::LOW_REPUTATION, 10 * 60,
               "foo.com/abc", now);
  CacheVerdict(GURL("http://bar.com/index.jsp"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::PHISHING, 10 * 60, "bar.com",
               now);
  ASSERT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  CacheVerdict(GURL("http://foo.com/abc/index.jsp"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               LoginReputationClientResponse::LOW_REPUTATION, 10 * 60,
               "foo.com/abc", now);
  CacheVerdict(GURL("http://bar.com/index.jsp"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               LoginReputationClientResponse::PHISHING, 10 * 60, "bar.com",
               now);
  ASSERT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Delete a bar.com URL. Corresponding content setting keyed on
  // origin "http://bar.com" should be removed,
  history::URLRows deleted_urls;
  deleted_urls.push_back(history::URLRow(GURL("http://bar.com")));

  // Delete an arbitrary data URL, to ensure the service is robust against
  // filtering only http/s URLs. See crbug.com/709758.
  deleted_urls.push_back(history::URLRow(GURL("data:text/html, <p>hellow")));

  password_protection_service_->RemoveContentSettingsOnURLsDeleted(
      false /* all_history */, deleted_urls);
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  LoginReputationClientResponse actual_verdict;
  EXPECT_EQ(
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      password_protection_service_->GetCachedVerdict(
          GURL("http://bar.com"),
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &actual_verdict));
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                GURL("http://bar.com"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                &actual_verdict));

  // If delete all history. All password protection content settings should be
  // gone.
  password_protection_service_->RemoveContentSettingsOnURLsDeleted(
      true /* all_history */, history::URLRows());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
}

TEST_F(PasswordProtectionServiceTest, VerifyCanGetReputationOfURL) {
  // Invalid main frame URL.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(GURL()));

  // Main frame URL scheme is not HTTP or HTTPS.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("data:text/html, <p>hellow")));

  // Main frame URL is a local host.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://localhost:80")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://127.0.0.1")));

  // Main frame URL is a private IP address or anything in an IANA-reserved
  // range.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://192.168.1.0/")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://10.0.1.0/")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://[FEED::BEEF]")));

  // Main frame URL is a no-yet-assigned y ICANN gTLD.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://intranet")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://host.intranet.example")));

  // Main frame URL is a dotless domain.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://go/example")));

  // Main frame URL is anything else.
  EXPECT_TRUE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://www.chromium.org")));
}

TEST_F(PasswordProtectionServiceTest, TestNoRequestSentForWhitelistedURL) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogramName, 0);
  InitializeAndStartPasswordOnFocusRequest(true /* match whitelist */,
                                           10000 /* timeout in ms*/);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(4 /* MATCHED_WHITELIST */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestNoRequestSentIfVerdictAlreadyCached) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogramName, 0);
  CacheVerdict(GURL(kTargetUrl),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               LoginReputationClientResponse::LOW_REPUTATION, 600,
               GURL(kTargetUrl).host(), base::Time::Now());
  InitializeAndStartPasswordOnFocusRequest(false /* match whitelist */,
                                           10000 /* timeout in ms*/);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(5 /* RESPONSE_ALREADY_CACHED */, 1)));
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            password_protection_service_->latest_response()->verdict_type());
}

TEST_F(PasswordProtectionServiceTest, TestResponseFetchFailed) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogramName, 0);
  net::TestURLFetcher failed_fetcher(0, GURL("http://bar.com"), nullptr);
  // Set up failed response.
  failed_fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));

  InitializeAndStartPasswordOnFocusRequest(false /* match whitelist */,
                                           10000 /* timeout in ms*/);
  request_->OnURLFetchComplete(&failed_fetcher);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(9 /* FETCH_FAILED */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestMalformedResponse) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogramName, 0);
  // Set up malformed response.
  net::TestURLFetcher fetcher(0, GURL("http://bar.com"), nullptr);
  fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  fetcher.set_response_code(200);
  fetcher.SetResponseString("invalid response");

  InitializeAndStartPasswordOnFocusRequest(false /* match whitelist */,
                                           10000 /* timeout in ms*/);
  request_->OnURLFetchComplete(&fetcher);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(10 /* RESPONSE_MALFORMED */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestRequestTimedout) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogramName, 0);
  InitializeAndStartPasswordOnFocusRequest(false /* match whitelist */,
                                           0 /* timeout immediately */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(3 /* TIMEDOUT */, 1)));
}

TEST_F(PasswordProtectionServiceTest,
       TestPasswordOnFocusRequestAndResponseSuccessfull) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogramName, 0);
  // Set up valid response.
  net::TestURLFetcher fetcher(0, GURL("http://bar.com"), nullptr);
  fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  fetcher.set_response_code(200);
  LoginReputationClientResponse expected_response = CreateVerdictProto(
      LoginReputationClientResponse::PHISHING, 600, GURL(kTargetUrl).host());
  fetcher.SetResponseString(expected_response.SerializeAsString());

  InitializeAndStartPasswordOnFocusRequest(false /* match whitelist */,
                                           10000 /* timeout in ms*/);
  request_->OnURLFetchComplete(&fetcher);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(
                  "PasswordProtection.Verdict.PasswordFieldOnFocus"),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  LoginReputationClientResponse* actual_response =
      password_protection_service_->latest_response();
  EXPECT_EQ(expected_response.verdict_type(), actual_response->verdict_type());
  EXPECT_EQ(expected_response.cache_expression(),
            actual_response->cache_expression());
  EXPECT_EQ(expected_response.cache_duration_sec(),
            actual_response->cache_duration_sec());
}

TEST_F(PasswordProtectionServiceTest,
       TestPasswordEntryRequestAndResponseSuccessfull) {
  histograms_.ExpectTotalCount(kPasswordEntryRequestOutcomeHistogramName, 0);
  histograms_.ExpectTotalCount(kSyncPasswordEntryRequestOutcomeHistogramName,
                               0);
  // Set up valid response.
  net::TestURLFetcher fetcher(0, GURL("http://bar.com"), nullptr);
  fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  fetcher.set_response_code(200);
  LoginReputationClientResponse expected_response = CreateVerdictProto(
      LoginReputationClientResponse::PHISHING, 600, GURL(kTargetUrl).host());
  fetcher.SetResponseString(expected_response.SerializeAsString());

  // Initiate a saved password entry request.
  InitializeAndStartPasswordEntryRequest(
      "example.com", false /* match whitelist */, 10000 /* timeout in ms*/);
  request_->OnURLFetchComplete(&fetcher);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordEntryRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(
                  "PasswordProtection.Verdict.ProtectedPasswordEntry"),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  histograms_.ExpectTotalCount(kSyncPasswordEntryRequestOutcomeHistogramName,
                               0);

  // Initiate a sync password entry request.
  InitializeAndStartPasswordEntryRequest(password_manager::kSyncPasswordDomain,
                                         false /* match whitelist */,
                                         10000 /* timeout in ms*/);
  request_->OnURLFetchComplete(&fetcher);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms_.GetAllSamples(kSyncPasswordEntryRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordEntryRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(
                  "PasswordProtection.Verdict.SyncProtectedPasswordEntry"),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(
                  "PasswordProtection.Verdict.ProtectedPasswordEntry"),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestTearDownWithPendingRequests) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogramName, 0);
  GURL target_url(kTargetUrl);
  EXPECT_CALL(*database_manager_.get(), CheckCsdWhitelistUrl(target_url, _))
      .WillRepeatedly(Return(AsyncMatch::NO_MATCH));
  password_protection_service_->StartRequest(
      nullptr, target_url, GURL("http://foo.com/submit"),
      GURL("http://foo.com/frame"), std::string(),
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);

  // Destroy password_protection_service_ while there is one request pending.
  password_protection_service_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogramName),
      ElementsAre(base::Bucket(2 /* CANCELED */, 1)));
}

TEST_F(PasswordProtectionServiceTest, TestCleanUpExpiredVerdict) {
  // Prepare 4 verdicts for PASSWORD_REUSE_EVENT:
  // (1) "foo.com/abc" valid
  // (2) "foo.com/def" expired
  // (3) "bar.com/abc" expired
  // (4) "bar.com/def" expired
  base::Time now = base::Time::Now();
  CacheVerdict(GURL("https://foo.com/abc/index.jsp"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::LOW_REPUTATION, 10 * 60,
               "foo.com/abc", now);
  CacheVerdict(GURL("https://foo.com/def/index.jsp"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::LOW_REPUTATION, 0, "foo.com/def",
               now);
  CacheVerdict(GURL("https://bar.com/abc/index.jsp"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::PHISHING, 0, "bar.com/abc", now);
  CacheVerdict(GURL("https://bar.com/def/index.jsp"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               LoginReputationClientResponse::PHISHING, 0, "bar.com/def", now);
  ASSERT_EQ(4U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Prepare 2 verdicts for UNFAMILIAR_LOGIN_PAGE:
  // (1) "bar.com/def" valid
  // (2) "bar.com/xyz" expired
  CacheVerdict(GURL("https://bar.com/def/index.jsp"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               LoginReputationClientResponse::SAFE, 10 * 60, "bar.com/def",
               now);
  CacheVerdict(GURL("https://bar.com/xyz/index.jsp"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               LoginReputationClientResponse::PHISHING, 0, "bar.com/xyz", now);
  ASSERT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  password_protection_service_->CleanUpExpiredVerdicts();

  ASSERT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  LoginReputationClientResponse actual_verdict;
  // Has cached PASSWORD_REUSE_EVENT verdict for foo.com/abc.
  EXPECT_EQ(
      LoginReputationClientResponse::LOW_REPUTATION,
      password_protection_service_->GetCachedVerdict(
          GURL("https://foo.com/abc/test.jsp"),
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &actual_verdict));
  // No cached PASSWORD_REUSE_EVENT verdict for foo.com/def.
  EXPECT_EQ(
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      password_protection_service_->GetCachedVerdict(
          GURL("https://foo.com/def/index.jsp"),
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &actual_verdict));
  // No cached PASSWORD_REUSE_EVENT verdict for bar.com/abc.
  EXPECT_EQ(
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      password_protection_service_->GetCachedVerdict(
          GURL("https://bar.com/abc/index.jsp"),
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &actual_verdict));
  // No cached PASSWORD_REUSE_EVENT verdict for bar.com/def.
  EXPECT_EQ(
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      password_protection_service_->GetCachedVerdict(
          GURL("https://bar.com/def/index.jsp"),
          LoginReputationClientRequest::PASSWORD_REUSE_EVENT, &actual_verdict));

  // Has cached UNFAMILIAR_LOGIN_PAGE verdict for bar.com/def.
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                GURL("https://bar.com/def/index.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                &actual_verdict));

  // No cached UNFAMILIAR_LOGIN_PAGE verdict for bar.com/xyz.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                GURL("https://bar.com/xyz/index.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                &actual_verdict));
}

TEST_F(PasswordProtectionServiceTest,
       TestCleanUpExpiredVerdictWithInvalidEntry) {
  CacheInvalidVerdict();
  ContentSettingsForOneType password_protection_settings;
  content_setting_map_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
      &password_protection_settings);
  ASSERT_FALSE(password_protection_settings.empty());

  password_protection_service_->CleanUpExpiredVerdicts();

  content_setting_map_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
      &password_protection_settings);
  EXPECT_TRUE(password_protection_settings.empty());
}

TEST_F(PasswordProtectionServiceTest, VerifyPasswordOnFocusRequestProto) {
  // Set up valid response.
  net::TestURLFetcher fetcher(0, GURL("http://bar.com"), nullptr);
  fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  fetcher.set_response_code(200);
  LoginReputationClientResponse expected_response = CreateVerdictProto(
      LoginReputationClientResponse::PHISHING, 600, GURL(kTargetUrl).host());
  fetcher.SetResponseString(expected_response.SerializeAsString());

  InitializeAndStartPasswordOnFocusRequest(false /* match whitelist */,
                                           100000 /* timeout in ms*/);
  base::RunLoop().RunUntilIdle();
  request_->OnURLFetchComplete(&fetcher);
  base::RunLoop().RunUntilIdle();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  EXPECT_EQ(kTargetUrl, actual_request->page_url());
  EXPECT_EQ(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
            actual_request->trigger_type());
  ASSERT_EQ(2, actual_request->frames_size());
  EXPECT_EQ(kTargetUrl, actual_request->frames(0).url());
  EXPECT_EQ(kPasswordFrameUrl, actual_request->frames(1).url());
  EXPECT_EQ(true, actual_request->frames(1).has_password_field());
  ASSERT_EQ(1, actual_request->frames(1).forms_size());
  EXPECT_EQ(kFormActionUrl, actual_request->frames(1).forms(0).action_url());
}

TEST_F(PasswordProtectionServiceTest, VerifyPasswordProtectionRequestProto) {
  // Set up valid response.
  net::TestURLFetcher fetcher(0, GURL("http://bar.com"), nullptr);
  fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  fetcher.set_response_code(200);
  LoginReputationClientResponse expected_response = CreateVerdictProto(
      LoginReputationClientResponse::PHISHING, 600, GURL(kTargetUrl).host());
  fetcher.SetResponseString(expected_response.SerializeAsString());
  // Initialize request triggered by chrome sync password reuse.
  InitializeAndStartPasswordEntryRequest(
      std::string(password_manager::kSyncPasswordDomain),
      false /* match whitelist */, 100000 /* timeout in ms*/);
  base::RunLoop().RunUntilIdle();
  request_->OnURLFetchComplete(&fetcher);
  base::RunLoop().RunUntilIdle();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  EXPECT_EQ(kTargetUrl, actual_request->page_url());
  EXPECT_EQ(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
            actual_request->trigger_type());
  EXPECT_EQ(1, actual_request->frames_size());
  EXPECT_EQ(kTargetUrl, actual_request->frames(0).url());
  EXPECT_TRUE(actual_request->frames(0).has_password_field());
  ASSERT_TRUE(actual_request->has_password_reuse_event());
  ASSERT_TRUE(
      actual_request->password_reuse_event().is_chrome_signin_password());

  // Initialize request triggered by saved password reuse.
  InitializeAndStartPasswordEntryRequest(std::string(kSavedDomain),
                                         false /* match whitelist */,
                                         100000 /* timeout in ms*/);
  base::RunLoop().RunUntilIdle();
  request_->OnURLFetchComplete(&fetcher);
  base::RunLoop().RunUntilIdle();

  actual_request = password_protection_service_->GetLatestRequestProto();
  ASSERT_TRUE(actual_request->has_password_reuse_event());
  ASSERT_FALSE(
      actual_request->password_reuse_event().is_chrome_signin_password());
}

TEST_F(PasswordProtectionServiceTest, VerifyCanSendPing) {
  GURL suspicious_url("http://phishing.com");
  EXPECT_TRUE(password_protection_service_->CanSendPing(
      kProtectedPasswordEntryPinging, suspicious_url,
      true /* is_sync_password */));
  EXPECT_EQ(kProtectedPasswordEntryPinging.name,
            password_protection_service_->checked_feature_name());

  EXPECT_TRUE(password_protection_service_->CanSendPing(
      kPasswordFieldOnFocusPinging, suspicious_url,
      false /* is_sync_password */));
  EXPECT_EQ(kPasswordFieldOnFocusPinging.name,
            password_protection_service_->checked_feature_name());
}

}  // namespace safe_browsing
