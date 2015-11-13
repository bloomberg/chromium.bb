// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/safe_browsing/local_database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using content::TestBrowserThreadBundle;

namespace safe_browsing {

namespace {

class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient() {}
  ~TestClient() override {}

  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const std::string& metadata) override {}

  void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                SBThreatType threat_type) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace

class SafeBrowsingDatabaseManagerTest : public PlatformTest {
 public:
  bool RunSBHashTest(const ListType list_type,
                     const std::vector<SBThreatType>& expected_threats,
                     const std::string& result_list);

 private:
  TestBrowserThreadBundle thread_bundle_;
};

bool SafeBrowsingDatabaseManagerTest::RunSBHashTest(
    const ListType list_type,
    const std::vector<SBThreatType>& expected_threats,
    const std::string& result_list) {
  scoped_refptr<SafeBrowsingService> sb_service_(
      SafeBrowsingService::CreateSafeBrowsingService());
  scoped_refptr<LocalSafeBrowsingDatabaseManager> db_manager_(
      new LocalSafeBrowsingDatabaseManager(sb_service_));
  const SBFullHash same_full_hash = {};

  LocalSafeBrowsingDatabaseManager::SafeBrowsingCheck* check =
      new LocalSafeBrowsingDatabaseManager::SafeBrowsingCheck(
          std::vector<GURL>(), std::vector<SBFullHash>(1, same_full_hash), NULL,
          list_type, expected_threats);
  db_manager_->checks_.insert(check);

  const SBFullHashResult full_hash_result = {same_full_hash,
                                             GetListId(result_list)};

  std::vector<SBFullHashResult> fake_results(1, full_hash_result);
  bool result = db_manager_->HandleOneCheck(check, fake_results);
  db_manager_->checks_.erase(check);
  delete check;
  return result;
}

TEST_F(SafeBrowsingDatabaseManagerTest, CheckCorrespondsListType) {
  std::vector<SBThreatType> malware_threat(1,
                                           SB_THREAT_TYPE_BINARY_MALWARE_URL);
  EXPECT_FALSE(RunSBHashTest(BINURL, malware_threat, kMalwareList));
  EXPECT_TRUE(RunSBHashTest(BINURL, malware_threat, kBinUrlList));

  // Check for multiple threats
  std::vector<SBThreatType> multiple_threats;
  multiple_threats.push_back(SB_THREAT_TYPE_URL_MALWARE);
  multiple_threats.push_back(SB_THREAT_TYPE_URL_PHISHING);
  EXPECT_FALSE(RunSBHashTest(MALWARE, multiple_threats, kBinUrlList));
  EXPECT_TRUE(RunSBHashTest(MALWARE, multiple_threats, kMalwareList));
}

TEST_F(SafeBrowsingDatabaseManagerTest, GetUrlSeverestThreatType) {
  std::vector<SBFullHashResult> full_hashes;

  const GURL kMalwareUrl("http://www.malware.com/page.html");
  const GURL kPhishingUrl("http://www.phishing.com/page.html");
  const GURL kUnwantedUrl("http://www.unwanted.com/page.html");
  const GURL kUnwantedAndMalwareUrl(
      "http://www.unwantedandmalware.com/page.html");
  const GURL kSafeUrl("http://www.safe.com/page.html");

  const SBFullHash kMalwareHostHash = SBFullHashForString("malware.com/");
  const SBFullHash kPhishingHostHash = SBFullHashForString("phishing.com/");
  const SBFullHash kUnwantedHostHash = SBFullHashForString("unwanted.com/");
  const SBFullHash kUnwantedAndMalwareHostHash =
      SBFullHashForString("unwantedandmalware.com/");
  const SBFullHash kSafeHostHash = SBFullHashForString("www.safe.com/");

  {
    SBFullHashResult full_hash;
    full_hash.hash = kMalwareHostHash;
    full_hash.list_id = static_cast<int>(MALWARE);
    full_hashes.push_back(full_hash);
  }

  {
    SBFullHashResult full_hash;
    full_hash.hash = kPhishingHostHash;
    full_hash.list_id = static_cast<int>(PHISH);
    full_hashes.push_back(full_hash);
  }

  {
    SBFullHashResult full_hash;
    full_hash.hash = kUnwantedHostHash;
    full_hash.list_id = static_cast<int>(UNWANTEDURL);
    full_hashes.push_back(full_hash);
  }

  {
    // Add both MALWARE and UNWANTEDURL list IDs for
    // kUnwantedAndMalwareHostHash.
    SBFullHashResult full_hash_malware;
    full_hash_malware.hash = kUnwantedAndMalwareHostHash;
    full_hash_malware.list_id = static_cast<int>(MALWARE);
    full_hashes.push_back(full_hash_malware);

    SBFullHashResult full_hash_unwanted;
    full_hash_unwanted.hash = kUnwantedAndMalwareHostHash;
    full_hash_unwanted.list_id = static_cast<int>(UNWANTEDURL);
    full_hashes.push_back(full_hash_unwanted);
  }

  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            LocalSafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kMalwareHostHash, full_hashes));

  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING,
            LocalSafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kPhishingHostHash, full_hashes));

  EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED,
            LocalSafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kUnwantedHostHash, full_hashes));

  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            LocalSafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kUnwantedAndMalwareHostHash, full_hashes));

  EXPECT_EQ(SB_THREAT_TYPE_SAFE,
            LocalSafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kSafeHostHash, full_hashes));

  const size_t kArbitraryValue = 123456U;
  size_t index = kArbitraryValue;
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            LocalSafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kMalwareUrl, full_hashes, &index));
  EXPECT_EQ(0U, index);

  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING,
            LocalSafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kPhishingUrl, full_hashes, &index));
  EXPECT_EQ(1U, index);

  EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED,
            LocalSafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kUnwantedUrl, full_hashes, &index));
  EXPECT_EQ(2U, index);

  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            LocalSafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kUnwantedAndMalwareUrl, full_hashes, &index));
  EXPECT_EQ(3U, index);

  index = kArbitraryValue;
  EXPECT_EQ(SB_THREAT_TYPE_SAFE,
            LocalSafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kSafeUrl, full_hashes, &index));
  EXPECT_EQ(kArbitraryValue, index);
}

TEST_F(SafeBrowsingDatabaseManagerTest, ServiceStopWithPendingChecks) {
  scoped_refptr<SafeBrowsingService> sb_service(
      SafeBrowsingService::CreateSafeBrowsingService());
  scoped_refptr<LocalSafeBrowsingDatabaseManager> db_manager(
      new LocalSafeBrowsingDatabaseManager(sb_service));
  TestClient client;

  // Start the service and flush tasks to ensure database is made available.
  db_manager->StartOnIOThread();
  content::RunAllBlockingPoolTasksUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(db_manager->DatabaseAvailable());

  // Start an extension check operation, which is done asynchronously.
  std::set<std::string> extension_ids;
  extension_ids.insert("testtesttesttesttesttesttesttest");
  db_manager->CheckExtensionIDs(extension_ids, &client);

  // Stop the service without first flushing above tasks.
  db_manager->StopOnIOThread(false);

  // Now run posted tasks, whish should include the extension check which has
  // been posted to the safe browsing task runner. This should not crash.
  content::RunAllBlockingPoolTasksUntilIdle();
  base::RunLoop().RunUntilIdle();
}

}  // namespace safe_browsing
