// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::TestBrowserThread;

namespace {

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  FakeSafeBrowsingService() {}
 private:
  virtual ~FakeSafeBrowsingService() {}

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
  virtual SafeBrowsingService* CreateSafeBrowsingService() OVERRIDE {
    return new FakeSafeBrowsingService();
  }
};

}  // namespace

class SafeBrowsingDatabaseManagerTest : public PlatformTest {
 public:

  virtual void SetUp() {
    PlatformTest::SetUp();
    io_thread_.reset(new TestBrowserThread(BrowserThread::IO));
    SafeBrowsingService::RegisterFactory(&factory_);
  }
  virtual void TearDown() {
    io_thread_.reset();
    PlatformTest::TearDown();
  }
  bool RunSBHashTest(const safe_browsing_util::ListType list_type,
                     const std::vector<SBThreatType>& expected_threats,
                     const std::string& result_list);

 private:
  scoped_ptr<TestBrowserThread> io_thread_;
  TestSafeBrowsingServiceFactory factory_;
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
      result_list,
      0
  };

  std::vector<SBFullHashResult> fake_results(1, full_hash_result);
  return db_manager_->HandleOneCheck(check, fake_results);
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
