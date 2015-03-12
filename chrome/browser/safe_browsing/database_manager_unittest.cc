// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using content::TestBrowserThreadBundle;

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
  bool RunSBHashTest(const safe_browsing_util::ListType list_type,
                     const std::vector<SBThreatType>& expected_threats,
                     const std::string& result_list);

 private:
  TestBrowserThreadBundle thread_bundle_;
};

bool SafeBrowsingDatabaseManagerTest::RunSBHashTest(
    const safe_browsing_util::ListType list_type,
    const std::vector<SBThreatType>& expected_threats,
    const std::string& result_list) {
  scoped_refptr<SafeBrowsingService> sb_service_(
      SafeBrowsingService::CreateSafeBrowsingService());
  scoped_refptr<SafeBrowsingDatabaseManager> db_manager_(
      new SafeBrowsingDatabaseManager(sb_service_));
  const SBFullHash same_full_hash = {};

  SafeBrowsingDatabaseManager::SafeBrowsingCheck* check =
      new SafeBrowsingDatabaseManager::SafeBrowsingCheck(
          std::vector<GURL>(),
          std::vector<SBFullHash>(1, same_full_hash),
          NULL,
          list_type,
          expected_threats);
  db_manager_->checks_.insert(check);

  const SBFullHashResult full_hash_result = {
      same_full_hash,
      safe_browsing_util::GetListId(result_list)
  };

  std::vector<SBFullHashResult> fake_results(1, full_hash_result);
  bool result = db_manager_->HandleOneCheck(check, fake_results);
  db_manager_->checks_.erase(check);
  delete check;
  return result;
}

TEST_F(SafeBrowsingDatabaseManagerTest, CheckCorrespondsListType) {
  std::vector<SBThreatType> malware_threat(1,
                                           SB_THREAT_TYPE_BINARY_MALWARE_URL);
  EXPECT_FALSE(RunSBHashTest(safe_browsing_util::BINURL,
                             malware_threat,
                             safe_browsing_util::kMalwareList));
  EXPECT_TRUE(RunSBHashTest(safe_browsing_util::BINURL,
                            malware_threat,
                            safe_browsing_util::kBinUrlList));

  // Check for multiple threats
  std::vector<SBThreatType> multiple_threats;
  multiple_threats.push_back(SB_THREAT_TYPE_URL_MALWARE);
  multiple_threats.push_back(SB_THREAT_TYPE_URL_PHISHING);
  EXPECT_FALSE(RunSBHashTest(safe_browsing_util::MALWARE,
                             multiple_threats,
                             safe_browsing_util::kBinUrlList));
  EXPECT_TRUE(RunSBHashTest(safe_browsing_util::MALWARE,
                            multiple_threats,
                            safe_browsing_util::kMalwareList));
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
    full_hash.list_id = static_cast<int>(safe_browsing_util::MALWARE);
    full_hashes.push_back(full_hash);
  }

  {
    SBFullHashResult full_hash;
    full_hash.hash = kPhishingHostHash;
    full_hash.list_id = static_cast<int>(safe_browsing_util::PHISH);
    full_hashes.push_back(full_hash);
  }

  {
    SBFullHashResult full_hash;
    full_hash.hash = kUnwantedHostHash;
    full_hash.list_id = static_cast<int>(safe_browsing_util::UNWANTEDURL);
    full_hashes.push_back(full_hash);
  }

  {
    // Add both MALWARE and UNWANTEDURL list IDs for
    // kUnwantedAndMalwareHostHash.
    SBFullHashResult full_hash_malware;
    full_hash_malware.hash = kUnwantedAndMalwareHostHash;
    full_hash_malware.list_id = static_cast<int>(safe_browsing_util::MALWARE);
    full_hashes.push_back(full_hash_malware);

    SBFullHashResult full_hash_unwanted;
    full_hash_unwanted.hash = kUnwantedAndMalwareHostHash;
    full_hash_unwanted.list_id =
        static_cast<int>(safe_browsing_util::UNWANTEDURL);
    full_hashes.push_back(full_hash_unwanted);
  }

  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            SafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kMalwareHostHash, full_hashes));

  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING,
            SafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kPhishingHostHash, full_hashes));

  EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED,
            SafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kUnwantedHostHash, full_hashes));

  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            SafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kUnwantedAndMalwareHostHash, full_hashes));

  EXPECT_EQ(SB_THREAT_TYPE_SAFE,
            SafeBrowsingDatabaseManager::GetHashSeverestThreatType(
                kSafeHostHash, full_hashes));

  const size_t kArbitraryValue = 123456U;
  size_t index = kArbitraryValue;
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            SafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kMalwareUrl, full_hashes, &index));
  EXPECT_EQ(0U, index);

  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING,
            SafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kPhishingUrl, full_hashes, &index));
  EXPECT_EQ(1U, index);

  EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED,
            SafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kUnwantedUrl, full_hashes, &index));
  EXPECT_EQ(2U, index);

  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            SafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kUnwantedAndMalwareUrl, full_hashes, &index));
  EXPECT_EQ(3U, index);

  index = kArbitraryValue;
  EXPECT_EQ(SB_THREAT_TYPE_SAFE,
            SafeBrowsingDatabaseManager::GetUrlSeverestThreatType(
                kSafeUrl, full_hashes, &index));
  EXPECT_EQ(kArbitraryValue, index);
}

TEST_F(SafeBrowsingDatabaseManagerTest, ServiceStopWithPendingChecks) {
  // Force the "blocking pool" mode for the test. This allows test coverage
  // for this behavior while that mode is still not the default. Additionally,
  // it is currently required for this test to work - as RunUntilIdle() will
  // not run tasks of the special spawned thread, but will for the worker pool.
  // TODO(asvitkine): Clean up, when blocking pool mode is made the default.
  base::FieldTrialList list(nullptr);
  std::map<std::string, std::string> params;
  params["SBThreadingMode"] = "BlockingPool2";
  variations::AssociateVariationParams("LightSpeed", "X", params);
  base::FieldTrialList::CreateFieldTrial("LightSpeed", "X");

  scoped_refptr<SafeBrowsingService> sb_service(
      SafeBrowsingService::CreateSafeBrowsingService());
  scoped_refptr<SafeBrowsingDatabaseManager> db_manager(
      new SafeBrowsingDatabaseManager(sb_service));
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

  variations::testing::ClearAllVariationParams();
}
