// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/v4_get_hash_protocol_manager.h"

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/safe_browsing/db/safebrowsing.pb.h"
#include "components/safe_browsing/db/util.h"
#include "components/safe_browsing/db/v4_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/platform_test.h"

using base::Time;
using base::TimeDelta;

namespace safe_browsing {

namespace {

struct KeyValue {
  std::string key;
  std::string value;

  explicit KeyValue(const std::string key, const std::string value)
      : key(key), value(value){};
  explicit KeyValue(const KeyValue& other) = default;

 private:
  KeyValue();
};

struct ResponseInfo {
  FullHash full_hash;
  ListIdentifier list_id;
  std::vector<KeyValue> key_values;

  explicit ResponseInfo(FullHash full_hash, ListIdentifier list_id)
      : full_hash(full_hash), list_id(list_id){};
  explicit ResponseInfo(const ResponseInfo& other)
      : full_hash(other.full_hash),
        list_id(other.list_id),
        key_values(other.key_values){};

 private:
  ResponseInfo();
};

}  // namespace

class V4GetHashProtocolManagerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    callback_called_ = false;
  }

  std::unique_ptr<V4GetHashProtocolManager> CreateProtocolManager() {
    StoresToCheck stores_to_check(
        {GetUrlMalwareId(), GetChromeUrlApiId(),
         ListIdentifier(CHROME_PLATFORM, URL, SOCIAL_ENGINEERING),
         ListIdentifier(CHROME_PLATFORM, URL, POTENTIALLY_HARMFUL_APPLICATION),
         ListIdentifier(CHROME_PLATFORM, URL, SUBRESOURCE_FILTER)});
    return V4GetHashProtocolManager::Create(NULL, stores_to_check,
                                            GetTestV4ProtocolConfig());
  }

  static void SetupFetcherToReturnOKResponse(
      const net::TestURLFetcherFactory& factory,
      const std::vector<ResponseInfo>& infos) {
    net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
    DCHECK(fetcher);
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(200);
    fetcher->SetResponseString(GetV4HashResponse(infos));
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  static std::vector<ResponseInfo> GetStockV4HashResponseInfos() {
    ResponseInfo info(FullHash("Everything's shiny, Cap'n."),
                      GetChromeUrlApiId());
    info.key_values.emplace_back("permission", "NOTIFICATIONS");
    std::vector<ResponseInfo> infos;
    infos.push_back(info);
    return infos;
  }

  static std::string GetStockV4HashResponse() {
    return GetV4HashResponse(GetStockV4HashResponseInfos());
  }

  void SetTestClock(base::Time now, V4GetHashProtocolManager* pm) {
    base::SimpleTestClock* clock = new base::SimpleTestClock();
    clock->SetNow(now);
    pm->SetClockForTests(base::WrapUnique(clock));
  }

  void ValidateGetV4ApiResults(const ThreatMetadata& expected_md,
                               const ThreatMetadata& actual_md) {
    EXPECT_EQ(expected_md, actual_md);
    callback_called_ = true;
  }

  void ValidateGetV4HashResults(
      const std::vector<FullHashInfo>& expected_results,
      const std::vector<FullHashInfo>& actual_results) {
    EXPECT_EQ(expected_results.size(), actual_results.size());
    for (size_t i = 0; i < actual_results.size(); i++) {
      EXPECT_TRUE(expected_results[i] == actual_results[i]);
    }
    callback_called_ = true;
  }

  bool callback_called() { return callback_called_; }

 private:
  static std::string GetV4HashResponse(
      std::vector<ResponseInfo> response_infos) {
    FindFullHashesResponse res;
    res.mutable_negative_cache_duration()->set_seconds(600);
    for (const ResponseInfo& info : response_infos) {
      ThreatMatch* m = res.add_matches();
      m->set_platform_type(info.list_id.platform_type());
      m->set_threat_entry_type(info.list_id.threat_entry_type());
      m->set_threat_type(info.list_id.threat_type());
      m->mutable_cache_duration()->set_seconds(300);
      m->mutable_threat()->set_hash(info.full_hash);

      for (const KeyValue& key_value : info.key_values) {
        ThreatEntryMetadata::MetadataEntry* e =
            m->mutable_threat_entry_metadata()->add_entries();
        e->set_key(key_value.key);
        e->set_value(key_value.value);
      }
    }

    // Serialize.
    std::string res_data;
    res.SerializeToString(&res_data);

    return res_data;
  }

  bool callback_called_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(V4GetHashProtocolManagerTest, TestGetHashErrorHandlingNetwork) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  FullHashToStoreAndHashPrefixesMap matched_locally;
  matched_locally[FullHash("AHashFull")].emplace_back(GetUrlSocEngId(),
                                                      HashPrefix("AHash"));
  std::vector<FullHashInfo> expected_results;
  pm->GetFullHashes(
      matched_locally, {},
      base::Bind(&V4GetHashProtocolManagerTest::ValidateGetV4HashResults,
                 base::Unretained(this), expected_results));

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
  EXPECT_TRUE(callback_called());
}

TEST_F(V4GetHashProtocolManagerTest, TestGetHashErrorHandlingResponseCode) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  FullHashToStoreAndHashPrefixesMap matched_locally;
  matched_locally[FullHash("AHashFull")].emplace_back(GetUrlSocEngId(),
                                                      HashPrefix("AHash"));
  std::vector<FullHashInfo> expected_results;
  pm->GetFullHashes(
      matched_locally, {},
      base::Bind(&V4GetHashProtocolManagerTest::ValidateGetV4HashResults,
                 base::Unretained(this), expected_results));

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
  EXPECT_TRUE(callback_called());
}

TEST_F(V4GetHashProtocolManagerTest, TestGetHashErrorHandlingOK) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  HashPrefix prefix("Everything");
  FullHash full_hash("Everything's shiny, Cap'n.");
  FullHashToStoreAndHashPrefixesMap matched_locally;
  matched_locally[full_hash].push_back(
      StoreAndHashPrefix(GetChromeUrlApiId(), prefix));
  std::vector<FullHashInfo> expected_results;
  FullHashInfo fhi(full_hash, GetChromeUrlApiId(),
                   now + base::TimeDelta::FromSeconds(300));
  fhi.metadata.api_permissions.insert("NOTIFICATIONS");
  expected_results.push_back(fhi);

  pm->GetFullHashes(
      matched_locally, {},
      base::Bind(&V4GetHashProtocolManagerTest::ValidateGetV4HashResults,
                 base::Unretained(this), expected_results));

  SetupFetcherToReturnOKResponse(factory, GetStockV4HashResponseInfos());

  // No error, back off multiplier is unchanged.
  EXPECT_EQ(0ul, pm->gethash_error_count_);
  EXPECT_EQ(1ul, pm->gethash_back_off_mult_);

  // Verify the state of the cache.
  const FullHashCache* cache = pm->full_hash_cache_for_tests();
  // Check the cache.
  ASSERT_EQ(1u, cache->size());
  EXPECT_EQ(1u, cache->count(prefix));
  const CachedHashPrefixInfo& cached_result = cache->at(prefix);
  EXPECT_EQ(cached_result.negative_expiry,
            now + base::TimeDelta::FromSeconds(600));
  ASSERT_EQ(1u, cached_result.full_hash_infos.size());
  EXPECT_EQ(FullHash("Everything's shiny, Cap'n."),
            cached_result.full_hash_infos[0].full_hash);
  EXPECT_TRUE(callback_called());
}

TEST_F(V4GetHashProtocolManagerTest,
       TestResultsNotCachedForNegativeCacheDuration) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  HashPrefix prefix("Everything");
  std::vector<HashPrefix> prefixes_requested({prefix});
  base::Time negative_cache_expire;
  FullHash full_hash("Everything's shiny, Cap'n.");
  std::vector<FullHashInfo> fhis;
  fhis.emplace_back(full_hash, GetChromeUrlApiId(), base::Time::UnixEpoch());

  pm->UpdateCache(prefixes_requested, fhis, negative_cache_expire);

  // Verify the state of the cache.
  const FullHashCache* cache = pm->full_hash_cache_for_tests();
  // Check the cache.
  EXPECT_EQ(0u, cache->size());
}

TEST_F(V4GetHashProtocolManagerTest, TestGetHashRequest) {
  FindFullHashesRequest req;
  ThreatInfo* info = req.mutable_threat_info();
  for (const PlatformType& p :
       std::set<PlatformType>{GetCurrentPlatformType(), CHROME_PLATFORM}) {
    info->add_platform_types(p);
  }

  info->add_threat_entry_types(URL);

  for (const ThreatType& tt : std::set<ThreatType>{
           MALWARE_THREAT, SOCIAL_ENGINEERING, POTENTIALLY_HARMFUL_APPLICATION,
           API_ABUSE, SUBRESOURCE_FILTER}) {
    info->add_threat_types(tt);
  }

  HashPrefix one = "hashone";
  HashPrefix two = "hashtwo";
  info->add_threat_entries()->set_hash(one);
  info->add_threat_entries()->set_hash(two);

  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());
  req.mutable_client()->set_client_id(pm->config_.client_name);
  req.mutable_client()->set_client_version(pm->config_.version);

  std::vector<std::string> client_states = {"client_state_1", "client_state_2"};
  for (const auto& client_state : client_states) {
    req.add_client_states(client_state);
  }

  // Serialize and Base64 encode.
  std::string req_data, req_base64;
  req.SerializeToString(&req_data);
  base::Base64Encode(req_data, &req_base64);

  std::vector<HashPrefix> prefixes_to_request = {one, two};
  EXPECT_EQ(req_base64, pm->GetHashRequest(prefixes_to_request, client_states));
}

TEST_F(V4GetHashProtocolManagerTest, TestParseHashResponse) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  FullHash full_hash("Everything's shiny, Cap'n.");
  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  res.mutable_minimum_wait_duration()->set_seconds(400);
  ThreatMatch* m = res.add_matches();
  m->set_threat_type(API_ABUSE);
  m->set_platform_type(CHROME_PLATFORM);
  m->set_threat_entry_type(URL);
  m->mutable_cache_duration()->set_seconds(300);
  m->mutable_threat()->set_hash(full_hash);
  ThreatEntryMetadata::MetadataEntry* e =
      m->mutable_threat_entry_metadata()->add_entries();
  e->set_key("permission");
  e->set_value("NOTIFICATIONS");
  // Add another ThreatMatch for a list we don't track. This response should
  // get dropped.
  m = res.add_matches();
  m->set_threat_type(THREAT_TYPE_UNSPECIFIED);
  m->set_platform_type(CHROME_PLATFORM);
  m->set_threat_entry_type(URL);
  m->mutable_cache_duration()->set_seconds(300);
  m->mutable_threat()->set_hash(full_hash);

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<FullHashInfo> full_hash_infos;
  base::Time cache_expire;
  EXPECT_TRUE(pm->ParseHashResponse(res_data, &full_hash_infos, &cache_expire));

  EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);
  // Even though the server responded with two ThreatMatch responses, one
  // should have been dropped.
  ASSERT_EQ(1ul, full_hash_infos.size());
  const FullHashInfo& fhi = full_hash_infos[0];
  EXPECT_EQ(full_hash, fhi.full_hash);
  EXPECT_EQ(GetChromeUrlApiId(), fhi.list_id);
  EXPECT_EQ(1ul, fhi.metadata.api_permissions.size());
  EXPECT_EQ(1ul, fhi.metadata.api_permissions.count("NOTIFICATIONS"));
  EXPECT_EQ(now + base::TimeDelta::FromSeconds(300), fhi.positive_expiry);
  EXPECT_EQ(now + base::TimeDelta::FromSeconds(400), pm->next_gethash_time_);
}

// Adds an entry with an ignored ThreatEntryType.
TEST_F(V4GetHashProtocolManagerTest,
       TestParseHashResponseWrongThreatEntryType) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  res.add_matches()->set_threat_entry_type(EXECUTABLE);

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<FullHashInfo> full_hash_infos;
  base::Time cache_expire;
  EXPECT_FALSE(
      pm->ParseHashResponse(res_data, &full_hash_infos, &cache_expire));

  EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);
  // There should be no hash results.
  EXPECT_EQ(0ul, full_hash_infos.size());
}

// Adds entries with a ThreatPatternType metadata.
TEST_F(V4GetHashProtocolManagerTest, TestParseHashThreatPatternType) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  {
    // Test social engineering pattern type.
    FindFullHashesResponse se_res;
    se_res.mutable_negative_cache_duration()->set_seconds(600);
    ThreatMatch* se = se_res.add_matches();
    se->set_threat_type(SOCIAL_ENGINEERING);
    se->set_platform_type(CHROME_PLATFORM);
    se->set_threat_entry_type(URL);
    FullHash full_hash("Everything's shiny, Cap'n.");
    se->mutable_threat()->set_hash(full_hash);
    ThreatEntryMetadata::MetadataEntry* se_meta =
        se->mutable_threat_entry_metadata()->add_entries();
    se_meta->set_key("se_pattern_type");
    se_meta->set_value("SOCIAL_ENGINEERING_LANDING");

    std::string se_data;
    se_res.SerializeToString(&se_data);

    std::vector<FullHashInfo> full_hash_infos;
    base::Time cache_expire;
    EXPECT_TRUE(
        pm->ParseHashResponse(se_data, &full_hash_infos, &cache_expire));
    EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);

    // Ensure that the threat remains valid since we found a full hash match,
    // even though the metadata information could not be parsed correctly.
    ASSERT_EQ(1ul, full_hash_infos.size());
    const FullHashInfo& fhi = full_hash_infos[0];
    EXPECT_EQ(full_hash, fhi.full_hash);
    const ListIdentifier list_id(CHROME_PLATFORM, URL, SOCIAL_ENGINEERING);
    EXPECT_EQ(list_id, fhi.list_id);
    EXPECT_EQ(ThreatPatternType::SOCIAL_ENGINEERING_LANDING,
              fhi.metadata.threat_pattern_type);
  }

  {
    // Test potentially harmful application pattern type.
    FindFullHashesResponse pha_res;
    pha_res.mutable_negative_cache_duration()->set_seconds(600);
    ThreatMatch* pha = pha_res.add_matches();
    pha->set_threat_type(POTENTIALLY_HARMFUL_APPLICATION);
    pha->set_threat_entry_type(URL);
    pha->set_platform_type(CHROME_PLATFORM);
    FullHash full_hash("Not to fret.");
    pha->mutable_threat()->set_hash(full_hash);
    ThreatEntryMetadata::MetadataEntry* pha_meta =
        pha->mutable_threat_entry_metadata()->add_entries();
    pha_meta->set_key("pha_pattern_type");
    pha_meta->set_value("LANDING");

    std::string pha_data;
    pha_res.SerializeToString(&pha_data);
    std::vector<FullHashInfo> full_hash_infos;
    base::Time cache_expire;
    EXPECT_TRUE(
        pm->ParseHashResponse(pha_data, &full_hash_infos, &cache_expire));
    EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);

    ASSERT_EQ(1ul, full_hash_infos.size());
    const FullHashInfo& fhi = full_hash_infos[0];
    EXPECT_EQ(full_hash, fhi.full_hash);
    const ListIdentifier list_id(CHROME_PLATFORM, URL,
                                 POTENTIALLY_HARMFUL_APPLICATION);
    EXPECT_EQ(list_id, fhi.list_id);
    EXPECT_EQ(ThreatPatternType::MALWARE_LANDING,
              fhi.metadata.threat_pattern_type);
  }

  {
    // Test invalid pattern type.
    FullHash full_hash("Not to fret.");
    FindFullHashesResponse invalid_res;
    invalid_res.mutable_negative_cache_duration()->set_seconds(600);
    ThreatMatch* invalid = invalid_res.add_matches();
    invalid->set_threat_type(POTENTIALLY_HARMFUL_APPLICATION);
    invalid->set_threat_entry_type(URL);
    invalid->set_platform_type(CHROME_PLATFORM);
    invalid->mutable_threat()->set_hash(full_hash);
    ThreatEntryMetadata::MetadataEntry* invalid_meta =
        invalid->mutable_threat_entry_metadata()->add_entries();
    invalid_meta->set_key("pha_pattern_type");
    invalid_meta->set_value("INVALIDE_VALUE");

    std::string invalid_data;
    invalid_res.SerializeToString(&invalid_data);
    std::vector<FullHashInfo> full_hash_infos;
    base::Time cache_expire;
    EXPECT_TRUE(
        pm->ParseHashResponse(invalid_data, &full_hash_infos, &cache_expire));

    // Ensure that the threat remains valid since we found a full hash match,
    // even though the metadata information could not be parsed correctly.
    ASSERT_EQ(1ul, full_hash_infos.size());
    const auto& fhi = full_hash_infos[0];
    EXPECT_EQ(full_hash, fhi.full_hash);
    EXPECT_EQ(
        ListIdentifier(CHROME_PLATFORM, URL, POTENTIALLY_HARMFUL_APPLICATION),
        fhi.list_id);
    EXPECT_EQ(ThreatPatternType::NONE, fhi.metadata.threat_pattern_type);
  }
}

TEST_F(V4GetHashProtocolManagerTest, TestParseSubresourceFilterMetadata) {
  typedef SubresourceFilterLevel Level;
  typedef SubresourceFilterType Type;
  const struct {
    const char* abusive_type;
    const char* bas_type;
    SubresourceFilterMatch expected_match;
  } test_cases[] = {
      {"warn",
       "enforce",
       {{{Type::ABUSIVE, Level::WARN}, {Type::BETTER_ADS, Level::ENFORCE}},
        base::KEEP_FIRST_OF_DUPES}},
      {nullptr,
       "warn",
       {{{Type::BETTER_ADS, Level::WARN}}, base::KEEP_FIRST_OF_DUPES}},
      {"asdf",
       "",
       {{{Type::ABUSIVE, Level::ENFORCE}, {Type::BETTER_ADS, Level::ENFORCE}},
        base::KEEP_FIRST_OF_DUPES}},
      {"warn",
       nullptr,
       {{{Type::ABUSIVE, Level::WARN}}, base::KEEP_FIRST_OF_DUPES}},
      {nullptr, nullptr, {}},
      {"",
       "",
       {{{Type::ABUSIVE, Level::ENFORCE}, {Type::BETTER_ADS, Level::ENFORCE}},
        base::KEEP_FIRST_OF_DUPES}},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "abusive: " << test_case.abusive_type
                                    << " better ads: " << test_case.bas_type);
    std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

    base::Time now = base::Time::UnixEpoch();
    SetTestClock(now, pm.get());
    FindFullHashesResponse sf_res;
    sf_res.mutable_negative_cache_duration()->set_seconds(600);
    ThreatMatch* sf = sf_res.add_matches();
    sf->set_threat_type(SUBRESOURCE_FILTER);
    sf->set_platform_type(CHROME_PLATFORM);
    sf->set_threat_entry_type(URL);
    FullHash full_hash("Everything's shiny, Cap'n.");
    sf->mutable_threat()->set_hash(full_hash);

    // sf_absv.
    if (test_case.abusive_type != nullptr) {
      ThreatEntryMetadata::MetadataEntry* sf_absv =
          sf->mutable_threat_entry_metadata()->add_entries();
      sf_absv->set_key("sf_absv");
      sf_absv->set_value(test_case.abusive_type);
    }

    // sf_bas
    if (test_case.bas_type != nullptr) {
      ThreatEntryMetadata::MetadataEntry* sf_bas =
          sf->mutable_threat_entry_metadata()->add_entries();
      sf_bas->set_key("sf_bas");
      sf_bas->set_value(test_case.bas_type);
    }

    std::string sf_data;
    sf_res.SerializeToString(&sf_data);

    std::vector<FullHashInfo> full_hash_infos;
    base::Time cache_expire;
    EXPECT_TRUE(
        pm->ParseHashResponse(sf_data, &full_hash_infos, &cache_expire));
    EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);

    ASSERT_EQ(1ul, full_hash_infos.size());
    const FullHashInfo& fhi = full_hash_infos[0];
    EXPECT_EQ(full_hash, fhi.full_hash);
    const ListIdentifier list_id(CHROME_PLATFORM, URL, SUBRESOURCE_FILTER);
    EXPECT_EQ(list_id, fhi.list_id);
    EXPECT_EQ(test_case.expected_match, fhi.metadata.subresource_filter_match);
  }
}

// Adds metadata with a key value that is not "permission".
TEST_F(V4GetHashProtocolManagerTest,
       TestParseHashResponseNonPermissionMetadata) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  base::Time now = base::Time::UnixEpoch();
  SetTestClock(now, pm.get());

  FullHash full_hash("Not to fret.");
  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  ThreatMatch* m = res.add_matches();
  m->set_threat_type(API_ABUSE);
  m->set_platform_type(CHROME_PLATFORM);
  m->set_threat_entry_type(URL);
  m->mutable_threat()->set_hash(full_hash);
  ThreatEntryMetadata::MetadataEntry* e =
      m->mutable_threat_entry_metadata()->add_entries();
  e->set_key("notpermission");
  e->set_value("NOTGEOLOCATION");

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<FullHashInfo> full_hash_infos;
  base::Time cache_expire;
  EXPECT_TRUE(pm->ParseHashResponse(res_data, &full_hash_infos, &cache_expire));

  EXPECT_EQ(now + base::TimeDelta::FromSeconds(600), cache_expire);
  ASSERT_EQ(1ul, full_hash_infos.size());
  const auto& fhi = full_hash_infos[0];
  EXPECT_EQ(full_hash, fhi.full_hash);
  EXPECT_EQ(GetChromeUrlApiId(), fhi.list_id);
  EXPECT_TRUE(fhi.metadata.api_permissions.empty());
}

TEST_F(V4GetHashProtocolManagerTest,
       TestParseHashResponseInconsistentThreatTypes) {
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());

  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  ThreatMatch* m1 = res.add_matches();
  m1->set_threat_type(API_ABUSE);
  m1->set_platform_type(CHROME_PLATFORM);
  m1->set_threat_entry_type(URL);
  m1->mutable_threat()->set_hash(FullHash("Everything's shiny, Cap'n."));
  m1->mutable_threat_entry_metadata()->add_entries();
  ThreatMatch* m2 = res.add_matches();
  m2->set_threat_type(MALWARE_THREAT);
  m2->set_threat_entry_type(URL);
  m2->mutable_threat()->set_hash(FullHash("Not to fret."));

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  std::vector<FullHashInfo> full_hash_infos;
  base::Time cache_expire;
  EXPECT_FALSE(
      pm->ParseHashResponse(res_data, &full_hash_infos, &cache_expire));
}

// Checks that results are looked up correctly in the cache.
TEST_F(V4GetHashProtocolManagerTest, GetCachedResults) {
  base::Time now = base::Time::UnixEpoch();
  FullHash full_hash("example");
  HashPrefix prefix("exam");
  FullHashToStoreAndHashPrefixesMap matched_locally;
  matched_locally[full_hash].emplace_back(GetUrlMalwareId(), prefix);
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());
  FullHashCache* cache = pm->full_hash_cache_for_tests();

  {
    std::vector<HashPrefix> prefixes_to_request;
    std::vector<FullHashInfo> cached_full_hash_infos;
    cache->clear();

    // Test with an empty cache. (Case: 2)
    pm->GetFullHashCachedResults(matched_locally, now, &prefixes_to_request,
                                 &cached_full_hash_infos);
    EXPECT_TRUE(cache->empty());
    ASSERT_EQ(1ul, prefixes_to_request.size());
    EXPECT_EQ(prefix, prefixes_to_request[0]);
    EXPECT_TRUE(cached_full_hash_infos.empty());
  }

  {
    std::vector<HashPrefix> prefixes_to_request;
    std::vector<FullHashInfo> cached_full_hash_infos;
    cache->clear();

    // Prefix has a cache entry but full hash is not there. (Case: 1-b-i)
    CachedHashPrefixInfo* entry = &(*cache)[prefix];
    entry->negative_expiry = now + base::TimeDelta::FromMinutes(5);
    pm->GetFullHashCachedResults(matched_locally, now, &prefixes_to_request,
                                 &cached_full_hash_infos);
    EXPECT_TRUE(prefixes_to_request.empty());
    EXPECT_TRUE(cached_full_hash_infos.empty());
  }

  {
    std::vector<HashPrefix> prefixes_to_request;
    std::vector<FullHashInfo> cached_full_hash_infos;
    cache->clear();

    // Expired negative cache entry. (Case: 1-b-ii)
    CachedHashPrefixInfo* entry = &(*cache)[prefix];
    entry->negative_expiry = now - base::TimeDelta::FromMinutes(5);
    pm->GetFullHashCachedResults(matched_locally, now, &prefixes_to_request,
                                 &cached_full_hash_infos);
    ASSERT_EQ(1ul, prefixes_to_request.size());
    EXPECT_EQ(prefix, prefixes_to_request[0]);
    EXPECT_TRUE(cached_full_hash_infos.empty());
  }

  {
    std::vector<HashPrefix> prefixes_to_request;
    std::vector<FullHashInfo> cached_full_hash_infos;
    cache->clear();

    // Now put unexpired full hash in the cache. (Case: 1-a-i)
    CachedHashPrefixInfo* entry = &(*cache)[prefix];
    entry->negative_expiry = now + base::TimeDelta::FromMinutes(5);
    entry->full_hash_infos.emplace_back(full_hash, GetUrlMalwareId(),
                                        now + base::TimeDelta::FromMinutes(3));
    pm->GetFullHashCachedResults(matched_locally, now, &prefixes_to_request,
                                 &cached_full_hash_infos);
    EXPECT_TRUE(prefixes_to_request.empty());
    ASSERT_EQ(1ul, cached_full_hash_infos.size());
    EXPECT_EQ(full_hash, cached_full_hash_infos[0].full_hash);
  }

  {
    std::vector<HashPrefix> prefixes_to_request;
    std::vector<FullHashInfo> cached_full_hash_infos;
    cache->clear();

    // Expire the full hash in the cache. (Case: 1-a-ii)
    CachedHashPrefixInfo* entry = &(*cache)[prefix];
    entry->negative_expiry = now + base::TimeDelta::FromMinutes(5);
    entry->full_hash_infos.emplace_back(full_hash, GetUrlMalwareId(),
                                        now - base::TimeDelta::FromMinutes(3));
    pm->GetFullHashCachedResults(matched_locally, now, &prefixes_to_request,
                                 &cached_full_hash_infos);
    ASSERT_EQ(1ul, prefixes_to_request.size());
    EXPECT_EQ(prefix, prefixes_to_request[0]);
    EXPECT_TRUE(cached_full_hash_infos.empty());
  }
}

TEST_F(V4GetHashProtocolManagerTest, TestUpdatesAreMerged) {
  // We'll add one of the requested FullHashInfo objects into the cache, and
  // inject the other one as a response from the server. The result should
  // include both FullHashInfo objects.

  net::TestURLFetcherFactory factory;
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());
  HashPrefix prefix_1("exam");
  FullHash full_hash_1("example");
  HashPrefix prefix_2("Everything");
  FullHash full_hash_2("Everything's shiny, Cap'n.");

  base::Time now = Time::Now();
  SetTestClock(now, pm.get());

  FullHashCache* cache = pm->full_hash_cache_for_tests();
  CachedHashPrefixInfo* entry = &(*cache)[prefix_1];
  entry->negative_expiry = now + base::TimeDelta::FromMinutes(100);
  // Put one unexpired full hash in the cache for a store we'll look in.
  entry->full_hash_infos.emplace_back(full_hash_1, GetUrlMalwareId(),
                                      now + base::TimeDelta::FromSeconds(200));
  // Put one unexpired full hash in the cache for a store we'll not look in.
  entry->full_hash_infos.emplace_back(full_hash_1, GetUrlSocEngId(),
                                      now + base::TimeDelta::FromSeconds(200));

  // Request full hash information from two stores.
  FullHashToStoreAndHashPrefixesMap matched_locally;
  matched_locally[full_hash_1].push_back(
      StoreAndHashPrefix(GetUrlMalwareId(), prefix_1));
  matched_locally[full_hash_2].push_back(
      StoreAndHashPrefix(GetChromeUrlApiId(), prefix_2));

  // Expect full hash information from both stores.
  std::vector<FullHashInfo> expected_results;
  expected_results.emplace_back(full_hash_1, GetUrlMalwareId(),
                                now + base::TimeDelta::FromSeconds(200));
  expected_results.emplace_back(full_hash_2, GetChromeUrlApiId(),
                                now + base::TimeDelta::FromSeconds(300));
  expected_results[1].metadata.api_permissions.insert("NOTIFICATIONS");

  pm->GetFullHashes(
      matched_locally, {},
      base::Bind(&V4GetHashProtocolManagerTest::ValidateGetV4HashResults,
                 base::Unretained(this), expected_results));

  SetupFetcherToReturnOKResponse(factory, GetStockV4HashResponseInfos());

  // No error, back off multiplier is unchanged.
  EXPECT_EQ(0ul, pm->gethash_error_count_);
  EXPECT_EQ(1ul, pm->gethash_back_off_mult_);

  // Verify the state of the cache.
  ASSERT_EQ(2u, cache->size());
  const CachedHashPrefixInfo& cached_result_1 = cache->at(prefix_1);
  EXPECT_EQ(cached_result_1.negative_expiry,
            now + base::TimeDelta::FromMinutes(100));
  ASSERT_EQ(2u, cached_result_1.full_hash_infos.size());
  EXPECT_EQ(full_hash_1, cached_result_1.full_hash_infos[0].full_hash);
  EXPECT_EQ(GetUrlMalwareId(), cached_result_1.full_hash_infos[0].list_id);

  const CachedHashPrefixInfo& cached_result_2 = cache->at(prefix_2);
  EXPECT_EQ(cached_result_2.negative_expiry,
            now + base::TimeDelta::FromSeconds(600));
  ASSERT_EQ(1u, cached_result_2.full_hash_infos.size());
  EXPECT_EQ(full_hash_2, cached_result_2.full_hash_infos[0].full_hash);
  EXPECT_EQ(GetChromeUrlApiId(), cached_result_2.full_hash_infos[0].list_id);
  EXPECT_TRUE(callback_called());
}

// The server responds back with full hash information containing metadata
// information for one of the full hashes for the URL in test.
TEST_F(V4GetHashProtocolManagerTest, TestGetFullHashesWithApisMergesMetadata) {
  net::TestURLFetcherFactory factory;
  const GURL url("https://www.example.com/more");
  ThreatMetadata expected_md;
  expected_md.api_permissions.insert("NOTIFICATIONS");
  expected_md.api_permissions.insert("AUDIO_CAPTURE");
  std::unique_ptr<V4GetHashProtocolManager> pm(CreateProtocolManager());
  pm->GetFullHashesWithApis(
      url, base::Bind(&V4GetHashProtocolManagerTest::ValidateGetV4ApiResults,
                      base::Unretained(this), expected_md));

  // The following two random looking strings value are two of the full hashes
  // produced by UrlToFullHashes in v4_protocol_manager_util.h for the URL:
  // "https://www.example.com"
  std::vector<ResponseInfo> infos;
  FullHash full_hash;
  base::Base64Decode("1ZzJ0/7NjPkg6t0DAS8L5Jf7jA48Pn7opQcP4UXYeXc=",
                     &full_hash);
  ResponseInfo info(full_hash, GetChromeUrlApiId());
  info.key_values.emplace_back("permission", "NOTIFICATIONS");
  infos.push_back(info);

  base::Base64Decode("c9mG4AkGXxgsELy2pF2z1u2pSY-JMGVK8mU_ipOM2AE=",
                     &full_hash);
  info = ResponseInfo(full_hash, GetChromeUrlApiId());
  info.key_values.emplace_back("permission", "AUDIO_CAPTURE");
  infos.push_back(info);

  full_hash = FullHash("Everything's shiny, Cap'n.");
  info = ResponseInfo(full_hash, GetChromeUrlApiId());
  info.key_values.emplace_back("permission", "GEOLOCATION");
  infos.push_back(info);
  SetupFetcherToReturnOKResponse(factory, infos);

  EXPECT_TRUE(callback_called());
}

}  // namespace safe_browsing
