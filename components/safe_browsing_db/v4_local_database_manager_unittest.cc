// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing_db/v4_database.h"
#include "components/safe_browsing_db/v4_local_database_manager.h"
#include "components/safe_browsing_db/v4_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class FakeV4Database : public V4Database {
 public:
  static void Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<StoreMap> store_map,
      const StoreAndHashPrefixes& store_and_hash_prefixes,
      NewDatabaseReadyCallback new_db_callback) {
    // Mimics V4Database::Create
    const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner =
        base::MessageLoop::current()->task_runner();
    db_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&FakeV4Database::CreateOnTaskRunner, db_task_runner,
                   base::Passed(&store_map), store_and_hash_prefixes,
                   callback_task_runner, new_db_callback));
  }

  void GetStoresMatchingFullHash(
      const FullHash& full_hash,
      const StoresToCheck& stores_to_check,
      StoreAndHashPrefixes* store_and_hash_prefixes) override {
    store_and_hash_prefixes->clear();
    for (const StoreAndHashPrefix& stored_sahp : store_and_hash_prefixes_) {
      const PrefixSize& prefix_size = stored_sahp.hash_prefix.size();
      if (!full_hash.compare(0, prefix_size, stored_sahp.hash_prefix)) {
        store_and_hash_prefixes->push_back(stored_sahp);
      }
    }
  }

 private:
  static void CreateOnTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<StoreMap> store_map,
      const StoreAndHashPrefixes& store_and_hash_prefixes,
      const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
      NewDatabaseReadyCallback new_db_callback) {
    // Mimics the semantics of V4Database::CreateOnTaskRunner
    std::unique_ptr<FakeV4Database> fake_v4_database(new FakeV4Database(
        db_task_runner, std::move(store_map), store_and_hash_prefixes));
    callback_task_runner->PostTask(
        FROM_HERE,
        base::Bind(new_db_callback, base::Passed(&fake_v4_database)));
  }

  FakeV4Database(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
                 std::unique_ptr<StoreMap> store_map,
                 const StoreAndHashPrefixes& store_and_hash_prefixes)
      : V4Database(db_task_runner, std::move(store_map)),
        store_and_hash_prefixes_(store_and_hash_prefixes) {}

  const StoreAndHashPrefixes store_and_hash_prefixes_;
};

class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient(SBThreatType sb_threat_type, const GURL& url)
      : expected_sb_threat_type(sb_threat_type), expected_url(url) {}

  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override {
    DCHECK_EQ(expected_url, url);
    DCHECK_EQ(expected_sb_threat_type, threat_type);
  }

  SBThreatType expected_sb_threat_type;
  GURL expected_url;
};

class FakeV4LocalDatabaseManager : public V4LocalDatabaseManager {
 public:
  void PerformFullHashCheck(std::unique_ptr<PendingCheck> check,
                            const FullHashToStoreAndHashPrefixesMap&
                                full_hash_to_store_and_hash_prefixes) override {
    perform_full_hash_check_called_ = true;
  }

  FakeV4LocalDatabaseManager(const base::FilePath& base_path)
      : V4LocalDatabaseManager(base_path),
        perform_full_hash_check_called_(false) {}

  static bool PerformFullHashCheckCalled(
      scoped_refptr<safe_browsing::V4LocalDatabaseManager>& v4_ldbm) {
    FakeV4LocalDatabaseManager* fake =
        static_cast<FakeV4LocalDatabaseManager*>(v4_ldbm.get());
    return fake->perform_full_hash_check_called_;
  }

 private:
  ~FakeV4LocalDatabaseManager() override {}

  bool perform_full_hash_check_called_;
};

class V4LocalDatabaseManagerTest : public PlatformTest {
 public:
  V4LocalDatabaseManagerTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    DVLOG(1) << "base_dir_: " << base_dir_.GetPath().value();

    v4_local_database_manager_ =
        make_scoped_refptr(new V4LocalDatabaseManager(base_dir_.GetPath()));
    SetTaskRunnerForTest();

    StartLocalDatabaseManager();
  }

  void TearDown() override {
    StopLocalDatabaseManager();

    PlatformTest::TearDown();
  }

  void ForceDisableLocalDatabaseManager() {
    v4_local_database_manager_->enabled_ = false;
  }

  void ForceEnableLocalDatabaseManager() {
    v4_local_database_manager_->enabled_ = true;
  }

  const V4LocalDatabaseManager::QueuedChecks& GetQueuedChecks() {
    return v4_local_database_manager_->queued_checks_;
  }

  void ReplaceV4Database(const StoreAndHashPrefixes& store_and_hash_prefixes) {
    // Disable the V4LocalDatabaseManager first so that if the callback to
    // verify checksum has been scheduled, then it doesn't do anything when it
    // is called back.
    ForceDisableLocalDatabaseManager();
    // Wait to make sure that the callback gets executed if it has already been
    // scheduled.
    WaitForTasksOnTaskRunner();
    // Re-enable the V4LocalDatabaseManager otherwise the checks won't work and
    // the fake database won't be set either.
    ForceEnableLocalDatabaseManager();

    NewDatabaseReadyCallback db_ready_callback =
        base::Bind(&V4LocalDatabaseManager::DatabaseReadyForChecks,
                   base::Unretained(v4_local_database_manager_.get()));
    FakeV4Database::Create(task_runner_, base::MakeUnique<StoreMap>(),
                           store_and_hash_prefixes, db_ready_callback);
    WaitForTasksOnTaskRunner();
  }

  void ResetV4Database() {
    V4Database::Destroy(std::move(v4_local_database_manager_->v4_database_));
  }

  void SetTaskRunnerForTest() {
    v4_local_database_manager_->SetTaskRunnerForTest(task_runner_);
  }

  void StartLocalDatabaseManager() {
    v4_local_database_manager_->StartOnIOThread(NULL,
                                                GetTestV4ProtocolConfig());
  }

  void StopLocalDatabaseManager() {
    if (v4_local_database_manager_) {
      v4_local_database_manager_->StopOnIOThread(true);
    }

    // Force destruction of the database.
    WaitForTasksOnTaskRunner();
  }

  void WaitForTasksOnTaskRunner() {
    // Wait for tasks on the task runner so we're sure that the
    // V4LocalDatabaseManager has read the data from disk.
    task_runner_->RunPendingTasks();
    base::RunLoop().RunUntilIdle();
  }

  base::ScopedTempDir base_dir_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<V4LocalDatabaseManager> v4_local_database_manager_;
};

TEST_F(V4LocalDatabaseManagerTest, TestGetThreatSource) {
  WaitForTasksOnTaskRunner();
  EXPECT_EQ(ThreatSource::LOCAL_PVER4,
            v4_local_database_manager_->GetThreatSource());
}

TEST_F(V4LocalDatabaseManagerTest, TestIsSupported) {
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(v4_local_database_manager_->IsSupported());
}

TEST_F(V4LocalDatabaseManagerTest, TestCanCheckUrl) {
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(
      v4_local_database_manager_->CanCheckUrl(GURL("http://example.com/a/")));
  EXPECT_TRUE(
      v4_local_database_manager_->CanCheckUrl(GURL("https://example.com/a/")));
  EXPECT_TRUE(
      v4_local_database_manager_->CanCheckUrl(GURL("ftp://example.com/a/")));
  EXPECT_FALSE(
      v4_local_database_manager_->CanCheckUrl(GURL("adp://example.com/a/")));
}

TEST_F(V4LocalDatabaseManagerTest,
       TestCheckBrowseUrlWithEmptyStoresReturnsNoMatch) {
  WaitForTasksOnTaskRunner();
  // Both the stores are empty right now so CheckBrowseUrl should return true.
  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), nullptr));
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckBrowseUrlWithFakeDbReturnsMatch) {
  WaitForTasksOnTaskRunner();
  net::TestURLFetcherFactory factory;

  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(),
                                       HashPrefix("eW\x1A\xF\xA9"));
  ReplaceV4Database(store_and_hash_prefixes);

  // The fake database returns a matched hash prefix.
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), nullptr));

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();
}

TEST_F(V4LocalDatabaseManagerTest,
       TestCheckBrowseUrlReturnsNoMatchWhenDisabled) {
  WaitForTasksOnTaskRunner();

  // The same URL returns |false| in the previous test because
  // v4_local_database_manager_ is enabled.
  ForceDisableLocalDatabaseManager();

  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), nullptr));
}

TEST_F(V4LocalDatabaseManagerTest, TestGetSeverestThreatTypeAndMetadata) {
  WaitForTasksOnTaskRunner();

  FullHashInfo fhi_malware(FullHash("Malware"), GetUrlMalwareId(),
                           base::Time::Now());
  fhi_malware.metadata.population_id = "malware_popid";

  FullHashInfo fhi_api(FullHash("api"), GetChromeUrlApiId(), base::Time::Now());
  fhi_api.metadata.population_id = "api_popid";

  std::vector<FullHashInfo> fhis({fhi_malware, fhi_api});

  SBThreatType result_threat_type;
  ThreatMetadata metadata;

  v4_local_database_manager_->GetSeverestThreatTypeAndMetadata(
      &result_threat_type, &metadata, fhis);
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, result_threat_type);
  EXPECT_EQ("malware_popid", metadata.population_id);

  // Reversing the list has no effect.
  std::reverse(std::begin(fhis), std::end(fhis));
  v4_local_database_manager_->GetSeverestThreatTypeAndMetadata(
      &result_threat_type, &metadata, fhis);
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, result_threat_type);
  EXPECT_EQ("malware_popid", metadata.population_id);
}

TEST_F(V4LocalDatabaseManagerTest, TestChecksAreQueued) {
  const GURL url("https://www.example.com/");
  TestClient client(SB_THREAT_TYPE_SAFE, url);
  EXPECT_TRUE(GetQueuedChecks().empty());
  v4_local_database_manager_->CheckBrowseUrl(url, &client);
  // The database is unavailable so the check should get queued.
  EXPECT_EQ(1ul, GetQueuedChecks().size());

  // The following function waits for the DB to load.
  WaitForTasksOnTaskRunner();
  EXPECT_TRUE(GetQueuedChecks().empty());

  ResetV4Database();
  v4_local_database_manager_->CheckBrowseUrl(url, &client);
  // The database is unavailable so the check should get queued.
  EXPECT_EQ(1ul, GetQueuedChecks().size());

  StopLocalDatabaseManager();
  EXPECT_TRUE(GetQueuedChecks().empty());
}

// This test is somewhat similar to TestCheckBrowseUrlWithFakeDbReturnsMatch but
// it uses a fake V4LocalDatabaseManager to assert that PerformFullHashCheck is
// called async.
TEST_F(V4LocalDatabaseManagerTest, PerformFullHashCheckCalledAsync) {
  // StopLocalDatabaseManager before resetting it because that's what
  // ~V4LocalDatabaseManager expects.
  StopLocalDatabaseManager();
  v4_local_database_manager_ =
      make_scoped_refptr(new FakeV4LocalDatabaseManager(base_dir_.GetPath()));
  SetTaskRunnerForTest();
  StartLocalDatabaseManager();
  WaitForTasksOnTaskRunner();
  net::TestURLFetcherFactory factory;

  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(),
                                       HashPrefix("eW\x1A\xF\xA9"));
  ReplaceV4Database(store_and_hash_prefixes);

  // The fake database returns a matched hash prefix.
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), nullptr));

  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));

  // Wait for PerformFullHashCheck to complete.
  WaitForTasksOnTaskRunner();

  EXPECT_TRUE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));
}

TEST_F(V4LocalDatabaseManagerTest, UsingWeakPtrDropsCallback) {
  // StopLocalDatabaseManager before resetting it because that's what
  // ~V4LocalDatabaseManager expects.
  StopLocalDatabaseManager();
  v4_local_database_manager_ =
      make_scoped_refptr(new FakeV4LocalDatabaseManager(base_dir_.GetPath()));
  SetTaskRunnerForTest();
  StartLocalDatabaseManager();
  WaitForTasksOnTaskRunner();
  net::TestURLFetcherFactory factory;

  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetUrlMalwareId(),
                                       HashPrefix("eW\x1A\xF\xA9"));
  ReplaceV4Database(store_and_hash_prefixes);

  // The fake database returns a matched hash prefix.
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), nullptr));
  v4_local_database_manager_->StopOnIOThread(true);

  // Release the V4LocalDatabaseManager object right away before the callback
  // gets called. When the callback gets called, without using a weak-ptr
  // factory, this leads to a use after free. However, using the weak-ptr means
  // that the callback is simply dropped.
  v4_local_database_manager_ = nullptr;

  // Wait for the tasks scheduled by StopOnIOThread to complete.
  WaitForTasksOnTaskRunner();
}

TEST_F(V4LocalDatabaseManagerTest, TestMatchMalwareIP) {
  StopLocalDatabaseManager();
  v4_local_database_manager_ =
      make_scoped_refptr(new FakeV4LocalDatabaseManager(base_dir_.GetPath()));
  SetTaskRunnerForTest();
  StartLocalDatabaseManager();
  WaitForTasksOnTaskRunner();

  // >>> hashlib.sha1(socket.inet_pton(socket.AF_INET6,
  // '::ffff:192.168.1.2')).digest() + chr(128)
  // '\xb3\xe0z\xafAv#h\x9a\xcf<\xf3ee\x94\xda\xf6y\xb1\xad\x80'
  StoreAndHashPrefixes store_and_hash_prefixes;
  store_and_hash_prefixes.emplace_back(GetIpMalwareId(),
                                       FullHash("\xB3\xE0z\xAF"
                                                "Av#h\x9A\xCF<\xF3"
                                                "ee\x94\xDA\xF6y\xB1\xAD\x80"));
  ReplaceV4Database(store_and_hash_prefixes);

  EXPECT_FALSE(v4_local_database_manager_->MatchMalwareIP(""));
  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));

  // The fake database returns no match.
  EXPECT_FALSE(v4_local_database_manager_->MatchMalwareIP("192.168.1.1"));
  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));

  // The fake database returns a matched hash prefix.
  EXPECT_TRUE(v4_local_database_manager_->MatchMalwareIP("192.168.1.2"));
  EXPECT_FALSE(FakeV4LocalDatabaseManager::PerformFullHashCheckCalled(
      v4_local_database_manager_));
}

}  // namespace safe_browsing
