// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/v4_protocol_manager.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "components/safe_browsing_db/util.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {

const char kClient[] = "unittest";
const char kAppVer[] = "1.0";

}  // namespace

namespace safe_browsing {

class SafeBrowsingV4ProtocolManagerTest : public testing::Test {
 protected:
  std::string key_param_;

  void SetUp() override {
    std::string key = google_apis::GetAPIKey();
    if (!key.empty()) {
      key_param_ = base::StringPrintf(
          "&key=%s", net::EscapeQueryParamValue(key, true).c_str());
    }
  }

  scoped_ptr<V4ProtocolManager> CreateProtocolManager() {
    SafeBrowsingProtocolConfig config;
    config.client_name = kClient;
    config.version = kAppVer;
    return scoped_ptr<V4ProtocolManager>(
        V4ProtocolManager::Create(NULL, config));
  }

  std::string GetStockV4HashResponse() {
    FindFullHashesResponse res;
    res.mutable_negative_cache_duration()->set_seconds(600);
    ThreatMatch* m = res.add_matches();
    m->set_threat_type(API_ABUSE);
    m->set_platform_type(CHROME_PLATFORM);
    m->set_threat_entry_type(URL_EXPRESSION);
    m->mutable_cache_duration()->set_seconds(300);
    m->mutable_threat()->set_hash(
        SBFullHashToString(SBFullHashForString("Everything's shiny, Cap'n.")));
    ThreatEntryMetadata::MetadataEntry* e =
        m->mutable_threat_entry_metadata()->add_entries();
    e->set_key("permission");
    e->set_value("NOTIFICATIONS");

    // Serialize.
    std::string res_data;
    res.SerializeToString(&res_data);

    return res_data;
  }
};

void ValidateGetV4HashResults(
    const std::vector<SBFullHashResult>& expected_full_hashes,
    const base::TimeDelta& expected_cache_duration,
    const std::vector<SBFullHashResult>& full_hashes,
    const base::TimeDelta& cache_duration) {
  EXPECT_EQ(expected_cache_duration, cache_duration);
  ASSERT_EQ(expected_full_hashes.size(), full_hashes.size());

  for (unsigned int i = 0; i < expected_full_hashes.size(); ++i) {
    const SBFullHashResult& expected = expected_full_hashes[i];
    const SBFullHashResult& actual = full_hashes[i];
    EXPECT_TRUE(SBFullHashEqual(expected.hash, actual.hash));
    EXPECT_EQ(expected.metadata, actual.metadata);
    EXPECT_EQ(expected.cache_duration, actual.cache_duration);
  }
}

TEST_F(SafeBrowsingV4ProtocolManagerTest, TestGetHashErrorHandlingNetwork) {
  net::TestURLFetcherFactory factory;
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> expected_full_hashes;
  base::TimeDelta expected_cache_duration;

  pm->GetFullHashesWithApis(
      prefixes, base::Bind(&ValidateGetV4HashResults, expected_full_hashes,
                           expected_cache_duration));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  // Failed request status should result in error.
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                            net::ERR_CONNECTION_RESET));
  fetcher->set_response_code(200);
  fetcher->SetResponseString(GetStockV4HashResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Should have recorded one error, but back off multiplier is unchanged.
  EXPECT_EQ(1ul, pm->gethash_error_count_);
  EXPECT_EQ(1ul, pm->gethash_back_off_mult_);
}

TEST_F(SafeBrowsingV4ProtocolManagerTest,
       TestGetHashErrorHandlingResponseCode) {
  net::TestURLFetcherFactory factory;
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> expected_full_hashes;
  base::TimeDelta expected_cache_duration;

  pm->GetFullHashesWithApis(
      prefixes, base::Bind(&ValidateGetV4HashResults, expected_full_hashes,
                           expected_cache_duration));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  // Response code of anything other than 200 should result in error.
  fetcher->set_response_code(204);
  fetcher->SetResponseString(GetStockV4HashResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Should have recorded one error, but back off multiplier is unchanged.
  EXPECT_EQ(1ul, pm->gethash_error_count_);
  EXPECT_EQ(1ul, pm->gethash_back_off_mult_);
}

TEST_F(SafeBrowsingV4ProtocolManagerTest, TestGetHashErrorHandlingOK) {
  net::TestURLFetcherFactory factory;
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> expected_full_hashes;
  SBFullHashResult hash_result;
  hash_result.hash = SBFullHashForString("Everything's shiny, Cap'n.");
  hash_result.metadata = "NOTIFICATIONS,";
  hash_result.cache_duration = base::TimeDelta::FromSeconds(300);
  expected_full_hashes.push_back(hash_result);
  base::TimeDelta expected_cache_duration = base::TimeDelta::FromSeconds(600);

  pm->GetFullHashesWithApis(
      prefixes, base::Bind(&ValidateGetV4HashResults, expected_full_hashes,
                           expected_cache_duration));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);
  fetcher->SetResponseString(GetStockV4HashResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // No error, back off multiplier is unchanged.
  EXPECT_EQ(0ul, pm->gethash_error_count_);
  EXPECT_EQ(1ul, pm->gethash_back_off_mult_);
}

TEST_F(SafeBrowsingV4ProtocolManagerTest, TestGetHashBackOffTimes) {
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  // No errors or back off time yet.
  EXPECT_EQ(0U, pm->gethash_error_count_);
  EXPECT_EQ(1U, pm->gethash_back_off_mult_);
  Time now = Time::Now();
  EXPECT_TRUE(pm->next_gethash_time_ < now);

  // 1 error.
  pm->HandleGetHashError(now);
  EXPECT_EQ(1U, pm->gethash_error_count_);
  EXPECT_EQ(1U, pm->gethash_back_off_mult_);
  EXPECT_LE(now + TimeDelta::FromMinutes(15), pm->next_gethash_time_);
  EXPECT_GE(now + TimeDelta::FromMinutes(30), pm->next_gethash_time_);

  // 2 errors.
  pm->HandleGetHashError(now);
  EXPECT_EQ(2U, pm->gethash_error_count_);
  EXPECT_EQ(2U, pm->gethash_back_off_mult_);
  EXPECT_LE(now + TimeDelta::FromMinutes(30), pm->next_gethash_time_);
  EXPECT_GE(now + TimeDelta::FromMinutes(60), pm->next_gethash_time_);

  // 3 errors.
  pm->HandleGetHashError(now);
  EXPECT_EQ(3U, pm->gethash_error_count_);
  EXPECT_EQ(4U, pm->gethash_back_off_mult_);
  EXPECT_LE(now + TimeDelta::FromMinutes(60), pm->next_gethash_time_);
  EXPECT_GE(now + TimeDelta::FromMinutes(120), pm->next_gethash_time_);

  // 4 errors.
  pm->HandleGetHashError(now);
  EXPECT_EQ(4U, pm->gethash_error_count_);
  EXPECT_EQ(8U, pm->gethash_back_off_mult_);
  EXPECT_LE(now + TimeDelta::FromMinutes(120), pm->next_gethash_time_);
  EXPECT_GE(now + TimeDelta::FromMinutes(240), pm->next_gethash_time_);

  // 5 errors.
  pm->HandleGetHashError(now);
  EXPECT_EQ(5U, pm->gethash_error_count_);
  EXPECT_EQ(16U, pm->gethash_back_off_mult_);
  EXPECT_LE(now + TimeDelta::FromMinutes(240), pm->next_gethash_time_);
  EXPECT_GE(now + TimeDelta::FromMinutes(480), pm->next_gethash_time_);

  // 6 errors.
  pm->HandleGetHashError(now);
  EXPECT_EQ(6U, pm->gethash_error_count_);
  EXPECT_EQ(32U, pm->gethash_back_off_mult_);
  EXPECT_LE(now + TimeDelta::FromMinutes(480), pm->next_gethash_time_);
  EXPECT_GE(now + TimeDelta::FromMinutes(960), pm->next_gethash_time_);

  // 7 errors.
  pm->HandleGetHashError(now);
  EXPECT_EQ(7U, pm->gethash_error_count_);
  EXPECT_EQ(64U, pm->gethash_back_off_mult_);
  EXPECT_LE(now + TimeDelta::FromMinutes(960), pm->next_gethash_time_);
  EXPECT_GE(now + TimeDelta::FromMinutes(1920), pm->next_gethash_time_);

  // 8 errors, reached max backoff.
  pm->HandleGetHashError(now);
  EXPECT_EQ(8U, pm->gethash_error_count_);
  EXPECT_EQ(128U, pm->gethash_back_off_mult_);
  EXPECT_EQ(now + TimeDelta::FromHours(24), pm->next_gethash_time_);

  // 9 errors, reached max backoff and multiplier capped.
  pm->HandleGetHashError(now);
  EXPECT_EQ(9U, pm->gethash_error_count_);
  EXPECT_EQ(128U, pm->gethash_back_off_mult_);
  EXPECT_EQ(now + TimeDelta::FromHours(24), pm->next_gethash_time_);
}

TEST_F(SafeBrowsingV4ProtocolManagerTest, TestGetHashUrl) {
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  EXPECT_EQ(
      "https://safebrowsing.googleapis.com/v4/encodedFullHashes/request_base64?"
      "alt=proto&client_id=unittest&client_version=1.0" +
          key_param_,
      pm->GetHashUrl("request_base64").spec());
}

TEST_F(SafeBrowsingV4ProtocolManagerTest, TestGetHashRequest) {
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  FindFullHashesRequest req;
  ThreatInfo* info = req.mutable_threat_info();
  info->add_threat_types(API_ABUSE);
  info->add_platform_types(CHROME_PLATFORM);
  info->add_threat_entry_types(URL_EXPRESSION);

  SBPrefix one = 1u;
  SBPrefix two = 2u;
  SBPrefix three = 3u;
  std::string hash(reinterpret_cast<const char*>(&one), sizeof(SBPrefix));
  info->add_threat_entries()->set_hash(hash);
  hash.clear();
  hash.append(reinterpret_cast<const char*>(&two), sizeof(SBPrefix));
  info->add_threat_entries()->set_hash(hash);
  hash.clear();
  hash.append(reinterpret_cast<const char*>(&three), sizeof(SBPrefix));
  info->add_threat_entries()->set_hash(hash);

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  req.SerializeToString(&req_data);
  base::Base64Encode(req_data, &req_base64);

  std::vector<PlatformType> platform;
  platform.push_back(CHROME_PLATFORM);
  std::vector<SBPrefix> prefixes;
  prefixes.push_back(one);
  prefixes.push_back(two);
  prefixes.push_back(three);
  EXPECT_EQ(req_base64, pm->GetHashRequest(prefixes, platform, API_ABUSE));
}

TEST_F(SafeBrowsingV4ProtocolManagerTest, TestParseHashResponse) {
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  res.mutable_minimum_wait_duration()->set_seconds(400);
  ThreatMatch* m = res.add_matches();
  m->set_threat_type(API_ABUSE);
  m->set_platform_type(CHROME_PLATFORM);
  m->set_threat_entry_type(URL_EXPRESSION);
  m->mutable_cache_duration()->set_seconds(300);
  m->mutable_threat()->set_hash(
      SBFullHashToString(SBFullHashForString("Everything's shiny, Cap'n.")));
  ThreatEntryMetadata::MetadataEntry* e =
      m->mutable_threat_entry_metadata()->add_entries();
  e->set_key("permission");
  e->set_value("NOTIFICATIONS");

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  Time now = Time::Now();
  std::vector<SBFullHashResult> full_hashes;
  base::TimeDelta cache_lifetime;
  EXPECT_TRUE(pm->ParseHashResponse(res_data, &full_hashes, &cache_lifetime));

  EXPECT_EQ(base::TimeDelta::FromSeconds(600), cache_lifetime);
  EXPECT_EQ(1ul, full_hashes.size());
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("Everything's shiny, Cap'n."),
                              full_hashes[0].hash));
  EXPECT_EQ("NOTIFICATIONS,", full_hashes[0].metadata);
  EXPECT_EQ(base::TimeDelta::FromSeconds(300), full_hashes[0].cache_duration);
  EXPECT_LE(now + base::TimeDelta::FromSeconds(400), pm->next_gethash_time_);
}

// Adds an entry with an ignored ThreatEntryType.
TEST_F(SafeBrowsingV4ProtocolManagerTest,
       TestParseHashResponseWrongThreatEntryType) {
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  res.add_matches()->set_threat_entry_type(BINARY_DIGEST);

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<SBFullHashResult> full_hashes;
  base::TimeDelta cache_lifetime;
  EXPECT_FALSE(pm->ParseHashResponse(res_data, &full_hashes, &cache_lifetime));

  EXPECT_EQ(base::TimeDelta::FromSeconds(600), cache_lifetime);
  // There should be no hash results.
  EXPECT_EQ(0ul, full_hashes.size());
}

// Adds an entry with a SOCIAL_ENGINEERING threat type.
TEST_F(SafeBrowsingV4ProtocolManagerTest,
       TestParseHashResponseSocialEngineeringThreatType) {
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  ThreatMatch* m = res.add_matches();
  m->set_threat_type(SOCIAL_ENGINEERING);
  m->set_platform_type(CHROME_PLATFORM);
  m->set_threat_entry_type(URL_EXPRESSION);
  m->mutable_threat()->set_hash(
      SBFullHashToString(SBFullHashForString("Not to fret.")));
  ThreatEntryMetadata::MetadataEntry* e =
      m->mutable_threat_entry_metadata()->add_entries();
  e->set_key("permission");
  e->set_value("IGNORED");

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<SBFullHashResult> full_hashes;
  base::TimeDelta cache_lifetime;
  EXPECT_FALSE(pm->ParseHashResponse(res_data, &full_hashes, &cache_lifetime));

  EXPECT_EQ(base::TimeDelta::FromSeconds(600), cache_lifetime);
  EXPECT_EQ(0ul, full_hashes.size());
}

// Adds metadata with a key value that is not "permission".
TEST_F(SafeBrowsingV4ProtocolManagerTest,
       TestParseHashResponseNonPermissionMetadata) {
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  ThreatMatch* m = res.add_matches();
  m->set_threat_type(API_ABUSE);
  m->set_platform_type(CHROME_PLATFORM);
  m->set_threat_entry_type(URL_EXPRESSION);
  m->mutable_threat()->set_hash(
      SBFullHashToString(SBFullHashForString("Not to fret.")));
  ThreatEntryMetadata::MetadataEntry* e =
      m->mutable_threat_entry_metadata()->add_entries();
  e->set_key("notpermission");
  e->set_value("NOTGEOLOCATION");

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<SBFullHashResult> full_hashes;
  base::TimeDelta cache_lifetime;
  EXPECT_TRUE(pm->ParseHashResponse(res_data, &full_hashes, &cache_lifetime));

  EXPECT_EQ(base::TimeDelta::FromSeconds(600), cache_lifetime);
  EXPECT_EQ(1ul, full_hashes.size());

  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("Not to fret."),
                              full_hashes[0].hash));
  // Metadata should be empty.
  EXPECT_EQ("", full_hashes[0].metadata);
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), full_hashes[0].cache_duration);
}

TEST_F(SafeBrowsingV4ProtocolManagerTest,
       TestParseHashResponseInconsistentThreatTypes) {
  scoped_ptr<V4ProtocolManager> pm(CreateProtocolManager());

  FindFullHashesResponse res;
  ThreatMatch* m1 = res.add_matches();
  m1->set_threat_type(API_ABUSE);
  m1->set_platform_type(CHROME_PLATFORM);
  m1->set_threat_entry_type(URL_EXPRESSION);
  m1->mutable_threat()->set_hash(
      SBFullHashToString(SBFullHashForString("Everything's shiny, Cap'n.")));
  m1->mutable_threat_entry_metadata()->add_entries();
  ThreatMatch* m2 = res.add_matches();
  m2->set_threat_type(MALWARE_THREAT);
  m2->set_threat_entry_type(URL_EXPRESSION);
  m2->mutable_threat()->set_hash(
      SBFullHashToString(SBFullHashForString("Not to fret.")));

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<SBFullHashResult> full_hashes;
  base::TimeDelta cache_lifetime;
  EXPECT_FALSE(pm->ParseHashResponse(res_data, &full_hashes, &cache_lifetime));
}

}  // namespace safe_browsing
