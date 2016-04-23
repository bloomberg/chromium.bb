// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "components/safe_browsing_db/testing_util.h"
#include "components/safe_browsing_db/util.h"
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
const char kKeyParam[] = "test_key_param";

}  // namespace

namespace safe_browsing {

class SafeBrowsingV4GetHashProtocolManagerTest : public testing::Test {
 protected:
  std::unique_ptr<V4GetHashProtocolManager> CreateProtocolManager() {
    V4ProtocolConfig config;
    config.client_name = kClient;
    config.version = kAppVer;
    config.key_param = kKeyParam;
    return std::unique_ptr<V4GetHashProtocolManager>(
        V4GetHashProtocolManager::Create(NULL, config));
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

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestGetHashErrorHandlingNetwork) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

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

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestGetHashErrorHandlingResponseCode) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

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

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest, TestGetHashErrorHandlingOK) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> expected_full_hashes;
  SBFullHashResult hash_result;
  hash_result.hash = SBFullHashForString("Everything's shiny, Cap'n.");
  hash_result.metadata.api_permissions.push_back("NOTIFICATIONS");
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

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest, TestGetHashRequest) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

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

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest, TestParseHashResponse) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

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
  EXPECT_EQ(1ul, full_hashes[0].metadata.api_permissions.size());
  EXPECT_EQ("NOTIFICATIONS", full_hashes[0].metadata.api_permissions[0]);
  EXPECT_EQ(base::TimeDelta::FromSeconds(300), full_hashes[0].cache_duration);
  EXPECT_LE(now + base::TimeDelta::FromSeconds(400), pm->next_gethash_time_);
}

// Adds an entry with an ignored ThreatEntryType.
TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestParseHashResponseWrongThreatEntryType) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

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
TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestParseHashResponseSocialEngineeringThreatType) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

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
TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestParseHashResponseNonPermissionMetadata) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

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
  EXPECT_EQ(0ul, full_hashes[0].metadata.api_permissions.size());
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), full_hashes[0].cache_duration);
}

TEST_F(SafeBrowsingV4GetHashProtocolManagerTest,
       TestParseHashResponseInconsistentThreatTypes) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

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
