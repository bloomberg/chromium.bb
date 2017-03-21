// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/password_protection/password_protection_service.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPasswordReuseMatchWhitelistHistogramName[] =
    "PasswordManager.PasswordReuse.MainFrameMatchCsdWhitelist";
const char kWhitelistedUrl[] = "http://inwhitelist.com";
const char kNoneWhitelistedUrl[] = "http://notinwhitelist.com";

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

class PasswordProtectionServiceTest : public testing::Test {
 public:
  PasswordProtectionServiceTest(){};

  ~PasswordProtectionServiceTest() override {
    content_setting_map_->ShutdownOnUIThread();
  }

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
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    password_protection_service_ =
        base::MakeUnique<PasswordProtectionService>(database_manager_);
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* incognito */, false /* guest_profile */);
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

  size_t GetPasswordProtectionSettingCount() {
    ContentSettingsForOneType password_protection_settings;
    content_setting_map_->GetSettingsForOneType(
        CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, std::string(),
        &password_protection_settings);
    return password_protection_settings.size();
  }

  size_t GetVerdictCountForOrigin(const GURL& origin_url) {
    std::unique_ptr<base::DictionaryValue> verdict_dictionary =
        base::DictionaryValue::From(content_setting_map_->GetWebsiteSetting(
            origin_url, GURL(), CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,
            std::string(), nullptr));
    return verdict_dictionary->size();
  }

 protected:
  // |thread_bundle_| is needed here because this test involves both UI and IO
  // threads.
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  std::unique_ptr<PasswordProtectionService> password_protection_service_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
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
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kPasswordReuseMatchWhitelistHistogramName, 0);

  // Empty url should not increment metric.
  password_protection_service_->RecordPasswordReuse(GURL());
  base::RunLoop().RunUntilIdle();
  histograms.ExpectTotalCount(kPasswordReuseMatchWhitelistHistogramName, 0);

  // Whitelisted url should increase "True" bucket by 1.
  password_protection_service_->RecordPasswordReuse(whitelisted_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms.GetAllSamples(kPasswordReuseMatchWhitelistHistogramName),
      testing::ElementsAre(base::Bucket(1, 1)));

  // Non-whitelisted url should increase "False" bucket by 1.
  password_protection_service_->RecordPasswordReuse(not_whitelisted_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms.GetAllSamples(kPasswordReuseMatchWhitelistHistogramName),
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
  ASSERT_EQ(0U, GetPasswordProtectionSettingCount());
  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/foo",
               false, base::Time::Now());

  EXPECT_EQ(1U, GetPasswordProtectionSettingCount());
  EXPECT_EQ(1U, GetVerdictCountForOrigin(
                    GURL("http://www.test.com/foo/index.html").GetOrigin()));

  // Cache another verdict with the some origin and cache_expression should
  // override the cache.
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientResponse::PHISHING, 10 * 60, "test.com/foo",
               false, base::Time::Now());
  EXPECT_EQ(1U, GetPasswordProtectionSettingCount());
  EXPECT_EQ(1U,
            GetVerdictCountForOrigin(GURL("http://www.test.com/").GetOrigin()));
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(),
                GURL("http://www.test.com/foo/index2.html")));

  // Cache another verdict with the same origin but different cache_expression
  // will not increase setting count, but will increase the number of verdicts
  // in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientResponse::SAFE, 10 * 60, "test.com/bar",
               false, base::Time::Now());
  EXPECT_EQ(1U, GetPasswordProtectionSettingCount());
  EXPECT_EQ(2U,
            GetVerdictCountForOrigin(GURL("http://www.test.com/").GetOrigin()));
}

TEST_F(PasswordProtectionServiceTest, TestGetCachedVerdicts) {
  ASSERT_EQ(0U, GetPasswordProtectionSettingCount());
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
  ASSERT_EQ(1U, GetPasswordProtectionSettingCount());

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL with unknown origin.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(), GURL("http://www.unknown.com/")));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL with http://test.com
  // origin, but doesn't match any known cache_expression.
  EXPECT_EQ(
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      password_protection_service_->GetCachedVerdict(
          content_setting_map_.get(), GURL("http://test.com/xyz/foo.jsp")));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL whose variants match
  // test.com/def, since corresponding entry is expired.
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(),
                GURL("http://test.com/def/ghi/index.html")));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL whose variants match
  // test.com, but not match it exactly. Return SAFE if it is a exact match of
  // test.com.
  EXPECT_EQ(
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      password_protection_service_->GetCachedVerdict(
          content_setting_map_.get(), GURL("http://test.com/ghi/index.html")));
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(),
                GURL("http://test.com/term_of_service.html")));

  // Return LOW_REPUTATION if look up for a URL whose variants match
  // test.com/abc.
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(), GURL("http://test.com/abc/")));
  EXPECT_EQ(
      LoginReputationClientResponse::LOW_REPUTATION,
      password_protection_service_->GetCachedVerdict(
          content_setting_map_.get(), GURL("http://test.com/abc/bar.jsp")));
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(),
                GURL("http://test.com/abc/foo/bar.html")));
}

TEST_F(PasswordProtectionServiceTest, TestCleanUpCachedVerdicts) {
  ASSERT_EQ(0U, GetPasswordProtectionSettingCount());
  // Prepare 2 verdicts. One is for origin "http://foo.com", and the other is
  // for "http://bar.com".
  base::Time now = base::Time::Now();
  CacheVerdict(GURL("http://foo.com/abc/index.jsp"),
               LoginReputationClientResponse::LOW_REPUTATION, 10 * 60,
               "foo.com/abc", false, now);
  CacheVerdict(GURL("http://bar.com/index.jsp"),
               LoginReputationClientResponse::PHISHING, 10 * 60, "bar.com",
               false, now);
  ASSERT_EQ(2U, GetPasswordProtectionSettingCount());

  // Delete a bar.com URL. Corresponding content setting keyed on
  // origin "http://bar.com" should be removed,
  history::URLRows deleted_urls;
  deleted_urls.push_back(history::URLRow(GURL("http://bar.com")));
  password_protection_service_->RemoveContentSettingsOnURLsDeleted(
      false /* all_history */, deleted_urls, content_setting_map_.get());
  EXPECT_EQ(1U, GetPasswordProtectionSettingCount());
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                content_setting_map_.get(), GURL("http://bar.com")));

  // If delete all history. All password protection content settings should be
  // gone.
  password_protection_service_->RemoveContentSettingsOnURLsDeleted(
      true /* all_history */, history::URLRows(), content_setting_map_.get());
  EXPECT_EQ(0U, GetPasswordProtectionSettingCount());
}

}  // namespace safe_browsing
