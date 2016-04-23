// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/database_manager.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
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
    const std::vector<SBFullHashResult>& full_hashes) {
  callback.Run(full_hashes, base::TimeDelta::FromMinutes(0));
}

// A TestV4GetHashProtocolManager that returns fixed responses from the
// Safe Browsing server for testing purpose.
class TestV4GetHashProtocolManager : public V4GetHashProtocolManager {
 public:
  TestV4GetHashProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config)
      : V4GetHashProtocolManager(request_context_getter, config),
        delay_seconds_(0) {}

  ~TestV4GetHashProtocolManager() override {}

  void GetFullHashesWithApis(const std::vector<SBPrefix>& prefixes,
                             FullHashCallback callback) override {
    prefixes_ = prefixes;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(InvokeFullHashCallback, callback, full_hashes_),
        base::TimeDelta::FromSeconds(delay_seconds_));
  }

  void SetDelaySeconds(int delay) {
    delay_seconds_ = delay;
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
  int delay_seconds_;
};

// Factory that creates test protocol manager instances.
class TestV4GetHashProtocolManagerFactory :
    public V4GetHashProtocolManagerFactory {
 public:
  TestV4GetHashProtocolManagerFactory() : pm_(NULL) {}
  ~TestV4GetHashProtocolManagerFactory() override {}

  V4GetHashProtocolManager* CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config) override {
    pm_ = new TestV4GetHashProtocolManager(request_context_getter, config);
    return pm_;
  }

 private:
  // Owned by the SafeBrowsingDatabaseManager.
  TestV4GetHashProtocolManager* pm_;
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
    TestV4GetHashProtocolManagerFactory get_hash_pm_factory;
    V4GetHashProtocolManager::RegisterFactory(&get_hash_pm_factory);

    db_manager_ = new TestSafeBrowsingDatabaseManager();
    db_manager_->StartOnIOThread(NULL, V4ProtocolConfig());
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    db_manager_->StopOnIOThread(false);
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

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  base::RunLoop().RunUntilIdle();
  std::vector<SBPrefix> prefixes = static_cast<TestV4GetHashProtocolManager*>(
      db_manager_->v4_get_hash_protocol_manager_)->GetRequestPrefixes();
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

}  // namespace safe_browsing
