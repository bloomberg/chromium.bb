// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/database_manager.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

void InvokeFullHashCallback(
    V4GetHashProtocolManager::FullHashCallback callback,
    const std::vector<SBFullHashResult>& full_hashes,
    base::Time negative_cache_expire) {
  callback.Run(full_hashes, negative_cache_expire);
}

// A TestV4GetHashProtocolManager that returns fixed responses from the
// Safe Browsing server for testing purpose.
class TestV4GetHashProtocolManager : public V4GetHashProtocolManager {
 public:
  TestV4GetHashProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config)
      : V4GetHashProtocolManager(request_context_getter, config),
        negative_cache_expire_(base::Time()), delay_seconds_(0) {}

  ~TestV4GetHashProtocolManager() override {}

  void GetFullHashesWithApis(const std::vector<SBPrefix>& prefixes,
                             FullHashCallback callback) override {
    prefixes_ = prefixes;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(InvokeFullHashCallback, callback, full_hashes_,
                              negative_cache_expire_),
        base::TimeDelta::FromSeconds(delay_seconds_));
  }

  void SetDelaySeconds(int delay) {
    delay_seconds_ = delay;
  }

  void SetNegativeCacheDurationMins(base::Time now,
      int negative_cache_duration_mins) {
    negative_cache_expire_ = now +
        base::TimeDelta::FromMinutes(negative_cache_duration_mins);
  }

  // Prepare the GetFullHash results for the next request.
  void AddGetFullHashResponse(const SBFullHashResult& full_hash_result) {
    full_hashes_.push_back(full_hash_result);
  }

  // Returns the prefixes that were sent in the last request.
  const std::vector<SBPrefix>& GetRequestPrefixes() { return prefixes_; }

 private:
  std::vector<SBPrefix> prefixes_;
  std::vector<SBFullHashResult> full_hashes_;
  base::Time negative_cache_expire_;
  int delay_seconds_;
};

// Factory that creates test protocol manager instances.
class TestV4GetHashProtocolManagerFactory :
    public V4GetHashProtocolManagerFactory {
 public:
  TestV4GetHashProtocolManagerFactory() {}
  ~TestV4GetHashProtocolManagerFactory() override {}

  V4GetHashProtocolManager* CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config) override {
    return new TestV4GetHashProtocolManager(request_context_getter, config);
  }
};

class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient() : callback_invoked_(false) {}
  ~TestClient() override {}

  void OnCheckApiBlacklistUrlResult(const GURL& url,
                                    const ThreatMetadata& metadata) override {
    blocked_permissions_ = metadata.api_permissions;
    callback_invoked_ = true;
  }

  const std::vector<std::string>& GetBlockedPermissions() {
    return blocked_permissions_;
  }

  bool callback_invoked() {return callback_invoked_;}

 private:
  std::vector<std::string> blocked_permissions_;
  bool callback_invoked_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace

class SafeBrowsingDatabaseManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    V4GetHashProtocolManager::RegisterFactory(
        base::WrapUnique(new TestV4GetHashProtocolManagerFactory()));

    db_manager_ = new TestSafeBrowsingDatabaseManager();
    db_manager_->StartOnIOThread(NULL, V4ProtocolConfig());
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    db_manager_->StopOnIOThread(false);
    V4GetHashProtocolManager::RegisterFactory(nullptr);
  }

  scoped_refptr<SafeBrowsingDatabaseManager> db_manager_;

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
};

TEST_F(SafeBrowsingDatabaseManagerTest, CheckApiBlacklistUrlWrongScheme) {
  TestClient client;
  const GURL url("file://example.txt");
  EXPECT_TRUE(db_manager_->CheckApiBlacklistUrl(url, &client));
}

TEST_F(SafeBrowsingDatabaseManagerTest, CheckApiBlacklistUrlPrefixes) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  // Generated from the sorted output of UrlToFullHashes in util.h.
  std::vector<SBPrefix> expected_prefixes =
      {1237562338, 2871045197, 3553205461, 3766933875};

  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();
  std::vector<SBPrefix> prefixes = pm->GetRequestPrefixes();
  EXPECT_EQ(expected_prefixes.size(), prefixes.size());
  for (unsigned int i = 0; i < prefixes.size(); ++i) {
    EXPECT_EQ(expected_prefixes[i], prefixes[i]);
  }
}

TEST_F(SafeBrowsingDatabaseManagerTest, HandleGetHashesWithApisResults) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.push_back("GEOLOCATION");
  pm->AddGetFullHashResponse(full_hash_result);

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  const std::vector<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(1ul, permissions.size());
  EXPECT_EQ("GEOLOCATION", permissions[0]);
}

TEST_F(SafeBrowsingDatabaseManagerTest, HandleGetHashesWithApisResultsNoMatch) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("wrongexample.com/");
  full_hash_result.metadata.api_permissions.push_back("GEOLOCATION");
  pm->AddGetFullHashResponse(full_hash_result);

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  const std::vector<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(0ul, permissions.size());
}

TEST_F(SafeBrowsingDatabaseManagerTest, HandleGetHashesWithApisResultsMatches) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.push_back("GEOLOCATION");
  pm->AddGetFullHashResponse(full_hash_result);
  SBFullHashResult full_hash_result2;
  full_hash_result2.hash = SBFullHashForString("example.com/more");
  full_hash_result2.metadata.api_permissions.push_back("NOTIFICATIONS");
  pm->AddGetFullHashResponse(full_hash_result2);
  SBFullHashResult full_hash_result3;
  full_hash_result3.hash = SBFullHashForString("wrongexample.com/");
  full_hash_result3.metadata.api_permissions.push_back("AUDIO_CAPTURE");
  pm->AddGetFullHashResponse(full_hash_result3);

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  const std::vector<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(2ul, permissions.size());
  EXPECT_EQ("GEOLOCATION", permissions[0]);
  EXPECT_EQ("NOTIFICATIONS", permissions[1]);
}

TEST_F(SafeBrowsingDatabaseManagerTest, CancelApiCheck) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.push_back("GEOLOCATION");
  pm->AddGetFullHashResponse(full_hash_result);
  pm->SetDelaySeconds(100);

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  EXPECT_TRUE(db_manager_->CancelApiCheck(&client));
  base::RunLoop().RunUntilIdle();

  const std::vector<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(0ul, permissions.size());
  EXPECT_FALSE(client.callback_invoked());
}

TEST_F(SafeBrowsingDatabaseManagerTest, ResultsAreCached) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  base::Time now = base::Time::UnixEpoch();
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.push_back("GEOLOCATION");
  full_hash_result.cache_expire_after = now + base::TimeDelta::FromMinutes(3);
  pm->AddGetFullHashResponse(full_hash_result);
  pm->SetNegativeCacheDurationMins(now, 5);

  EXPECT_TRUE(db_manager_->api_cache_.empty());
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  const std::vector<std::string>& permissions = client.GetBlockedPermissions();
  EXPECT_EQ(1ul, permissions.size());
  EXPECT_EQ("GEOLOCATION", permissions[0]);

  // Check the cache.
  // Generated from the sorted output of UrlToFullHashes in util.h.
  std::vector<SBPrefix> expected_prefixes =
      {1237562338, 2871045197, 3553205461, 3766933875};
  EXPECT_EQ(expected_prefixes.size(), db_manager_->api_cache_.size());

  auto entry = db_manager_->api_cache_.find(expected_prefixes[0]);
  EXPECT_NE(db_manager_->api_cache_.end(), entry);
  EXPECT_EQ(now + base::TimeDelta::FromMinutes(5), entry->second.expire_after);
  EXPECT_EQ(0ul, entry->second.full_hashes.size());

  entry = db_manager_->api_cache_.find(expected_prefixes[1]);
  EXPECT_NE(db_manager_->api_cache_.end(), entry);
  EXPECT_EQ(now + base::TimeDelta::FromMinutes(5), entry->second.expire_after);
  EXPECT_EQ(0ul, entry->second.full_hashes.size());

  entry = db_manager_->api_cache_.find(expected_prefixes[2]);
  EXPECT_NE(db_manager_->api_cache_.end(), entry);
  EXPECT_EQ(now + base::TimeDelta::FromMinutes(5), entry->second.expire_after);
  EXPECT_EQ(0ul, entry->second.full_hashes.size());

  entry = db_manager_->api_cache_.find(expected_prefixes[3]);
  EXPECT_NE(db_manager_->api_cache_.end(), entry);
  EXPECT_EQ(now + base::TimeDelta::FromMinutes(5), entry->second.expire_after);
  EXPECT_EQ(1ul, entry->second.full_hashes.size());
  EXPECT_TRUE(SBFullHashEqual(full_hash_result.hash,
                              entry->second.full_hashes[0].hash));
  EXPECT_EQ(1ul, entry->second.full_hashes[0].metadata.api_permissions.size());
  EXPECT_EQ("GEOLOCATION",
            entry->second.full_hashes[0].metadata.api_permissions[0]);
  EXPECT_EQ(full_hash_result.cache_expire_after,
            entry->second.full_hashes[0].cache_expire_after);
}

// An uninitialized value for negative cache expire does not cache results.
TEST_F(SafeBrowsingDatabaseManagerTest, ResultsAreNotCachedOnNull) {
  TestClient client;
  const GURL url("https://www.example.com/more");
  TestV4GetHashProtocolManager* pm = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_);
  base::Time now = base::Time::UnixEpoch();
  SBFullHashResult full_hash_result;
  full_hash_result.hash = SBFullHashForString("example.com/");
  full_hash_result.metadata.api_permissions.push_back("GEOLOCATION");
  full_hash_result.cache_expire_after = now + base::TimeDelta::FromMinutes(3);
  pm->AddGetFullHashResponse(full_hash_result);

  EXPECT_TRUE(db_manager_->api_cache_.empty());
  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client.callback_invoked());
  EXPECT_TRUE(db_manager_->api_cache_.empty());
}

}  // namespace safe_browsing
