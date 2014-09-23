// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a safebrowsing service using test safebrowsing database
// and a test protocol manager. It is used to test logics in safebrowsing
// service.

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/test/thread_test_helper.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/startup_task_runner_service.h"
#include "chrome/browser/profiles/startup_task_runner_service_factory.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/metadata.pb.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/cookie_store.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using content::BrowserThread;
using content::InterstitialPage;
using content::WebContents;
using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

namespace {

void InvokeFullHashCallback(
    SafeBrowsingProtocolManager::FullHashCallback callback,
    const std::vector<SBFullHashResult>& result) {
  callback.Run(result, base::TimeDelta::FromMinutes(45));
}

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  explicit FakeSafeBrowsingService(const std::string& url_prefix)
      : url_prefix_(url_prefix) {}

  virtual SafeBrowsingProtocolConfig GetProtocolConfig() const OVERRIDE {
    SafeBrowsingProtocolConfig config;
    config.url_prefix = url_prefix_;
    // Makes sure the auto update is not triggered. The tests will force the
    // update when needed.
    config.disable_auto_update = true;
#if defined(OS_ANDROID)
    config.disable_connection_check = true;
#endif
    config.client_name = "browser_tests";
    return config;
  }

 private:
  virtual ~FakeSafeBrowsingService() {}

  std::string url_prefix_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  explicit TestSafeBrowsingServiceFactory(const std::string& url_prefix)
      : url_prefix_(url_prefix) {}

  virtual SafeBrowsingService* CreateSafeBrowsingService() OVERRIDE {
    return new FakeSafeBrowsingService(url_prefix_);
  }

 private:
  std::string url_prefix_;
};

// A SafeBrowingDatabase class that allows us to inject the malicious URLs.
class TestSafeBrowsingDatabase :  public SafeBrowsingDatabase {
 public:
  TestSafeBrowsingDatabase() {}

  virtual ~TestSafeBrowsingDatabase() {}

  // Initializes the database with the given filename.
  virtual void Init(const base::FilePath& filename) OVERRIDE {}

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase() OVERRIDE {
    badurls_.clear();
    return true;
  }

  // Called on the IO thread to check if the given URL is safe or not.  If we
  // can synchronously determine that the URL is safe, CheckUrl returns true,
  // otherwise it returns false.
  virtual bool ContainsBrowseUrl(
      const GURL& url,
      std::vector<SBPrefix>* prefix_hits,
      std::vector<SBFullHashResult>* cache_hits) OVERRIDE {
    cache_hits->clear();
    return ContainsUrl(safe_browsing_util::MALWARE,
                       safe_browsing_util::PHISH,
                       std::vector<GURL>(1, url),
                       prefix_hits);
  }
  virtual bool ContainsDownloadUrl(
      const std::vector<GURL>& urls,
      std::vector<SBPrefix>* prefix_hits) OVERRIDE {
    bool found = ContainsUrl(safe_browsing_util::BINURL,
                             safe_browsing_util::BINURL,
                             urls,
                             prefix_hits);
    if (!found)
      return false;
    DCHECK_LE(1U, prefix_hits->size());
    return true;
  }
  virtual bool ContainsCsdWhitelistedUrl(const GURL& url) OVERRIDE {
    return true;
  }
  virtual bool ContainsDownloadWhitelistedString(
      const std::string& str) OVERRIDE {
    return true;
  }
  virtual bool ContainsDownloadWhitelistedUrl(const GURL& url) OVERRIDE {
    return true;
  }
  virtual bool ContainsExtensionPrefixes(
      const std::vector<SBPrefix>& prefixes,
      std::vector<SBPrefix>* prefix_hits) OVERRIDE {
    return true;
  }
  virtual bool ContainsSideEffectFreeWhitelistUrl(const GURL& url) OVERRIDE {
    return true;
  }
  virtual bool ContainsMalwareIP(const std::string& ip_address) OVERRIDE {
    return true;
  }
  virtual bool UpdateStarted(std::vector<SBListChunkRanges>* lists) OVERRIDE {
    ADD_FAILURE() << "Not implemented.";
    return false;
  }
  virtual void InsertChunks(
      const std::string& list_name,
      const std::vector<SBChunkData*>& chunks) OVERRIDE {
    ADD_FAILURE() << "Not implemented.";
  }
  virtual void DeleteChunks(
      const std::vector<SBChunkDelete>& chunk_deletes) OVERRIDE {
    ADD_FAILURE() << "Not implemented.";
  }
  virtual void UpdateFinished(bool update_succeeded) OVERRIDE {
    ADD_FAILURE() << "Not implemented.";
  }
  virtual void CacheHashResults(
      const std::vector<SBPrefix>& prefixes,
      const std::vector<SBFullHashResult>& cache_hits,
      const base::TimeDelta& cache_lifetime) OVERRIDE {
    // Do nothing for the cache.
  }
  virtual bool IsMalwareIPMatchKillSwitchOn() OVERRIDE {
    return false;
  }
  virtual bool IsCsdWhitelistKillSwitchOn() OVERRIDE {
    return false;
  }

  // Fill up the database with test URL.
  void AddUrl(const GURL& url,
              int list_id,
              const std::vector<SBPrefix>& prefix_hits) {
    badurls_[url.spec()].list_id = list_id;
    badurls_[url.spec()].prefix_hits = prefix_hits;
  }

  // Fill up the database with test hash digest.
  void AddDownloadPrefix(SBPrefix prefix) {
    download_digest_prefix_.insert(prefix);
  }

 private:
  struct Hits {
    int list_id;
    std::vector<SBPrefix> prefix_hits;
  };

  bool ContainsUrl(int list_id0,
                   int list_id1,
                   const std::vector<GURL>& urls,
                   std::vector<SBPrefix>* prefix_hits) {
    bool hit = false;
    for (size_t i = 0; i < urls.size(); ++i) {
      const GURL& url = urls[i];
      base::hash_map<std::string, Hits>::const_iterator
          badurls_it = badurls_.find(url.spec());

      if (badurls_it == badurls_.end())
        continue;

      if (badurls_it->second.list_id == list_id0 ||
          badurls_it->second.list_id == list_id1) {
        prefix_hits->insert(prefix_hits->end(),
                            badurls_it->second.prefix_hits.begin(),
                            badurls_it->second.prefix_hits.end());
        hit = true;
      }
    }
    return hit;
  }

  base::hash_map<std::string, Hits> badurls_;
  base::hash_set<SBPrefix> download_digest_prefix_;
};

// Factory that creates TestSafeBrowsingDatabase instances.
class TestSafeBrowsingDatabaseFactory : public SafeBrowsingDatabaseFactory {
 public:
  TestSafeBrowsingDatabaseFactory() : db_(NULL) {}
  virtual ~TestSafeBrowsingDatabaseFactory() {}

  virtual SafeBrowsingDatabase* CreateSafeBrowsingDatabase(
      bool enable_download_protection,
      bool enable_client_side_whitelist,
      bool enable_download_whitelist,
      bool enable_extension_blacklist,
      bool enable_side_effect_free_whitelist,
      bool enable_ip_blacklist) OVERRIDE {
    db_ = new TestSafeBrowsingDatabase();
    return db_;
  }
  TestSafeBrowsingDatabase* GetDb() {
    return db_;
  }
 private:
  // Owned by the SafebrowsingService.
  TestSafeBrowsingDatabase* db_;
};

// A TestProtocolManager that could return fixed responses from
// safebrowsing server for testing purpose.
class TestProtocolManager :  public SafeBrowsingProtocolManager {
 public:
  TestProtocolManager(SafeBrowsingProtocolManagerDelegate* delegate,
                      net::URLRequestContextGetter* request_context_getter,
                      const SafeBrowsingProtocolConfig& config)
      : SafeBrowsingProtocolManager(delegate, request_context_getter, config) {
    create_count_++;
  }

  virtual ~TestProtocolManager() {
    delete_count_++;
  }

  // This function is called when there is a prefix hit in local safebrowsing
  // database and safebrowsing service issues a get hash request to backends.
  // We return a result from the prefilled full_hashes_ hash_map to simulate
  // server's response. At the same time, latency is added to simulate real
  // life network issues.
  virtual void GetFullHash(
      const std::vector<SBPrefix>& prefixes,
      SafeBrowsingProtocolManager::FullHashCallback callback,
      bool is_download) OVERRIDE {
    BrowserThread::PostDelayedTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(InvokeFullHashCallback, callback, full_hashes_),
        delay_);
  }

  // Prepare the GetFullHash results for the next request.
  void SetGetFullHashResponse(const SBFullHashResult& full_hash_result) {
    full_hashes_.clear();
    full_hashes_.push_back(full_hash_result);
  }

  void IntroduceDelay(const base::TimeDelta& delay) {
    delay_ = delay;
  }

  static int create_count() {
    return create_count_;
  }

  static int delete_count() {
    return delete_count_;
  }

 private:
  std::vector<SBFullHashResult> full_hashes_;
  base::TimeDelta delay_;
  static int create_count_;
  static int delete_count_;
};

// static
int TestProtocolManager::create_count_ = 0;
// static
int TestProtocolManager::delete_count_ = 0;

// Factory that creates TestProtocolManager instances.
class TestSBProtocolManagerFactory : public SBProtocolManagerFactory {
 public:
  TestSBProtocolManagerFactory() : pm_(NULL) {}
  virtual ~TestSBProtocolManagerFactory() {}

  virtual SafeBrowsingProtocolManager* CreateProtocolManager(
      SafeBrowsingProtocolManagerDelegate* delegate,
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config) OVERRIDE {
    pm_ = new TestProtocolManager(delegate, request_context_getter, config);
    return pm_;
  }

  TestProtocolManager* GetProtocolManager() {
    return pm_;
  }

 private:
  // Owned by the SafebrowsingService.
  TestProtocolManager* pm_;
};

class MockObserver : public SafeBrowsingUIManager::Observer {
 public:
  MockObserver() {}
  virtual ~MockObserver() {}
  MOCK_METHOD1(OnSafeBrowsingHit,
               void(const SafeBrowsingUIManager::UnsafeResource&));
  MOCK_METHOD1(OnSafeBrowsingMatch,
               void(const SafeBrowsingUIManager::UnsafeResource&));
};

MATCHER_P(IsUnsafeResourceFor, url, "") {
  return (arg.url.spec() == url.spec() &&
          arg.threat_type != SB_THREAT_TYPE_SAFE);
}

}  // namespace

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingServiceTest() {
  }

  static void GenUrlFullhashResult(const GURL& url,
                                   int list_id,
                                   SBFullHashResult* full_hash) {
    std::string host;
    std::string path;
    safe_browsing_util::CanonicalizeUrl(url, &host, &path, NULL);
    full_hash->hash = SBFullHashForString(host + path);
    full_hash->list_id = list_id;
  }

  virtual void SetUp() {
    // InProcessBrowserTest::SetUp() instantiates SafebrowsingService and
    // RegisterFactory has to be called before SafeBrowsingService is created.
    sb_factory_.reset(new TestSafeBrowsingServiceFactory(
        "https://definatelynotarealdomain/safebrowsing"));
    SafeBrowsingService::RegisterFactory(sb_factory_.get());
    SafeBrowsingDatabase::RegisterFactory(&db_factory_);
    SafeBrowsingProtocolManager::RegisterFactory(&pm_factory_);
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() {
    InProcessBrowserTest::TearDown();

    // Unregister test factories after InProcessBrowserTest::TearDown
    // (which destructs SafeBrowsingService).
    SafeBrowsingDatabase::RegisterFactory(NULL);
    SafeBrowsingProtocolManager::RegisterFactory(NULL);
    SafeBrowsingService::RegisterFactory(NULL);
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Makes sure the auto update is not triggered during the test.
    // This test will fill up the database using testing prefixes
    // and urls.
    command_line->AppendSwitch(switches::kSbDisableAutoUpdate);
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    ASSERT_TRUE(test_server()->Start());
  }

  // This will setup the "url" prefix in database and prepare protocol manager
  // to response with |full_hash| for get full hash request.
  void SetupResponseForUrl(const GURL& url, const SBFullHashResult& full_hash) {
    std::vector<SBPrefix> prefix_hits;
    prefix_hits.push_back(full_hash.hash.prefix);

    // Make sure the full hits is empty unless we need to test the
    // full hash is hit in database's local cache.
    TestSafeBrowsingDatabase* db = db_factory_.GetDb();
    db->AddUrl(url, full_hash.list_id, prefix_hits);

    TestProtocolManager* pm = pm_factory_.GetProtocolManager();
    pm->SetGetFullHashResponse(full_hash);
  }

  bool ShowingInterstitialPage() {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InterstitialPage* interstitial_page = contents->GetInterstitialPage();
    return interstitial_page != NULL;
  }

  void IntroduceGetHashDelay(const base::TimeDelta& delay) {
    pm_factory_.GetProtocolManager()->IntroduceDelay(delay);
  }

  base::TimeDelta GetCheckTimeout(SafeBrowsingService* sb_service) {
    return sb_service->database_manager()->check_timeout_;
  }

  void SetCheckTimeout(SafeBrowsingService* sb_service,
                       const base::TimeDelta& delay) {
    sb_service->database_manager()->check_timeout_ = delay;
  }

  void CreateCSDService() {
    safe_browsing::ClientSideDetectionService* csd_service =
        safe_browsing::ClientSideDetectionService::Create(NULL);
    SafeBrowsingService* sb_service =
        g_browser_process->safe_browsing_service();
    sb_service->csd_service_.reset(csd_service);
    sb_service->RefreshState();
  }

  void ProceedAndWhitelist(
      const SafeBrowsingUIManager::UnsafeResource& resource) {
    std::vector<SafeBrowsingUIManager::UnsafeResource> resources;
    resources.push_back(resource);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingUIManager::OnBlockingPageDone,
                   g_browser_process->safe_browsing_service()->ui_manager(),
                   resources, true));
    WaitForIOThread();
  }

 protected:
  StrictMock<MockObserver> observer_;

  // Temporary profile dir for test cases that create a second profile.  This is
  // owned by the SafeBrowsingServiceTest object so that it will not get
  // destructed until after the test Browser has been torn down, since the
  // ImportantFileWriter may still be modifying it after the Profile object has
  // been destroyed.
  base::ScopedTempDir temp_profile_dir_;

  // Waits for pending tasks on the IO thread to complete. This is useful
  // to wait for the SafeBrowsingService to finish loading/stopping.
  void WaitForIOThread() {
    scoped_refptr<base::ThreadTestHelper> io_helper(new base::ThreadTestHelper(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get()));
    ASSERT_TRUE(io_helper->Run());
  }

 private:
  scoped_ptr<TestSafeBrowsingServiceFactory> sb_factory_;
  TestSafeBrowsingDatabaseFactory db_factory_;
  TestSBProtocolManagerFactory pm_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceTest);
};

enum MalwareMetadataTestType {
  METADATA_NONE,
  METADATA_LANDING,
  METADATA_DISTRIBUTION,
};

class SafeBrowsingServiceMetadataTest
    : public SafeBrowsingServiceTest,
      public ::testing::WithParamInterface<MalwareMetadataTestType> {
 public:
  SafeBrowsingServiceMetadataTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    SafeBrowsingServiceTest::SetUpOnMainThread();
    g_browser_process->safe_browsing_service()->ui_manager()->AddObserver(
        &observer_);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    g_browser_process->safe_browsing_service()->ui_manager()->RemoveObserver(
        &observer_);
    SafeBrowsingServiceTest::TearDownOnMainThread();
  }

  void GenUrlFullhashResultWithMetadata(const GURL& url,
                                        SBFullHashResult* full_hash) {
    GenUrlFullhashResult(url, safe_browsing_util::MALWARE, full_hash);

    safe_browsing::MalwarePatternType proto;
    switch (GetParam()) {
      case METADATA_NONE:
        full_hash->metadata = std::string();
        break;
      case METADATA_LANDING:
        proto.set_pattern_type(safe_browsing::MalwarePatternType::LANDING);
        full_hash->metadata = proto.SerializeAsString();
        break;
      case METADATA_DISTRIBUTION:
        proto.set_pattern_type(safe_browsing::MalwarePatternType::DISTRIBUTION);
        full_hash->metadata = proto.SerializeAsString();
        break;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceMetadataTest);
};

namespace {

const char kEmptyPage[] = "files/empty.html";
const char kMalwareFile[] = "files/downloads/dangerous/dangerous.exe";
const char kMalwarePage[] = "files/safe_browsing/malware.html";
const char kMalwareIFrame[] = "files/safe_browsing/malware_iframe.html";
const char kMalwareImg[] = "files/safe_browsing/malware_image.png";

// This test goes through DownloadResourceHandler.
IN_PROC_BROWSER_TEST_P(SafeBrowsingServiceMetadataTest, MalwareMainFrame) {
  GURL url = test_server()->GetURL(kEmptyPage);

  // After adding the url to safebrowsing database and getfullhash result,
  // we should see the interstitial page.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResultWithMetadata(url, &malware_full_hash);
  EXPECT_CALL(observer_,
              OnSafeBrowsingMatch(IsUnsafeResourceFor(url))).Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(1);
  SetupResponseForUrl(url, malware_full_hash);
  ui_test_utils::NavigateToURL(browser(), url);
  // All types should show the interstitial.
  EXPECT_TRUE(ShowingInterstitialPage());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingServiceMetadataTest, MalwareIFrame) {
  GURL main_url = test_server()->GetURL(kMalwarePage);
  GURL iframe_url = test_server()->GetURL(kMalwareIFrame);

  // Add the iframe url as malware and then load the parent page.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResultWithMetadata(iframe_url, &malware_full_hash);
  EXPECT_CALL(observer_, OnSafeBrowsingMatch(IsUnsafeResourceFor(iframe_url)))
      .Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(iframe_url)))
      .Times(1);
  SetupResponseForUrl(iframe_url, malware_full_hash);
  ui_test_utils::NavigateToURL(browser(), main_url);
  // All types should show the interstitial.
  EXPECT_TRUE(ShowingInterstitialPage());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingServiceMetadataTest, MalwareImg) {
  GURL main_url = test_server()->GetURL(kMalwarePage);
  GURL img_url = test_server()->GetURL(kMalwareImg);

  // Add the img url as malware and then load the parent page.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResultWithMetadata(img_url, &malware_full_hash);
  switch (GetParam()) {
    case METADATA_NONE:
    case METADATA_DISTRIBUTION:
      EXPECT_CALL(observer_, OnSafeBrowsingMatch(IsUnsafeResourceFor(img_url)))
          .Times(1);
      EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(img_url)))
          .Times(1);
      break;
    case METADATA_LANDING:
      // No interstitial shown, so no notifications expected.
      break;
  }
  SetupResponseForUrl(img_url, malware_full_hash);
  ui_test_utils::NavigateToURL(browser(), main_url);
  // Subresource which is tagged as a landing page should not show an
  // interstitial, the other types should.
  switch (GetParam()) {
    case METADATA_NONE:
    case METADATA_DISTRIBUTION:
      EXPECT_TRUE(ShowingInterstitialPage());
      break;
    case METADATA_LANDING:
      EXPECT_FALSE(ShowingInterstitialPage());
      break;
  }
}

INSTANTIATE_TEST_CASE_P(MaybeSetMetadata,
                        SafeBrowsingServiceMetadataTest,
                        testing::Values(METADATA_NONE,
                                        METADATA_LANDING,
                                        METADATA_DISTRIBUTION));

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, DISABLED_MalwareWithWhitelist) {
  GURL url = test_server()->GetURL(kEmptyPage);
  g_browser_process->safe_browsing_service()->
      ui_manager()->AddObserver(&observer_);

  // After adding the url to safebrowsing database and getfullhash result,
  // we should see the interstitial page.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResult(url, safe_browsing_util::MALWARE, &malware_full_hash);
  EXPECT_CALL(observer_,
              OnSafeBrowsingMatch(IsUnsafeResourceFor(url))).Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(1)
      .WillOnce(testing::Invoke(
          this, &SafeBrowsingServiceTest::ProceedAndWhitelist));
  SetupResponseForUrl(url, malware_full_hash);

  ui_test_utils::NavigateToURL(browser(), url);
  // Mock calls OnBlockingPageDone set to proceed, so the interstitial
  // is removed.
  EXPECT_FALSE(ShowingInterstitialPage());
  Mock::VerifyAndClearExpectations(&observer_);

  // Navigate back to kEmptyPage -- should hit the whitelist, and send a match
  // call, but no hit call.
  EXPECT_CALL(observer_,
              OnSafeBrowsingMatch(IsUnsafeResourceFor(url))).Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(0);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(ShowingInterstitialPage());

  g_browser_process->safe_browsing_service()->
      ui_manager()->RemoveObserver(&observer_);
}

const char kPrefetchMalwarePage[] = "files/safe_browsing/prefetch_malware.html";

// This test confirms that prefetches don't themselves get the
// interstitial treatment.
IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, Prefetch) {
  GURL url = test_server()->GetURL(kPrefetchMalwarePage);
  GURL malware_url = test_server()->GetURL(kMalwarePage);
  g_browser_process->safe_browsing_service()->
      ui_manager()->AddObserver(&observer_);

  class SetPrefetchForTest {
   public:
    explicit SetPrefetchForTest(bool prefetch)
        : old_prerender_mode_(prerender::PrerenderManager::GetMode()) {
      std::string exp_group = prefetch ? "ExperimentYes" : "ExperimentNo";
      base::FieldTrialList::CreateFieldTrial("Prefetch", exp_group);

      prerender::PrerenderManager::SetMode(
          prerender::PrerenderManager::PRERENDER_MODE_DISABLED);
    }

    ~SetPrefetchForTest() {
      prerender::PrerenderManager::SetMode(old_prerender_mode_);
    }

   private:
    prerender::PrerenderManager::PrerenderManagerMode old_prerender_mode_;
  } set_prefetch_for_test(true);

  // Even though we have added this uri to the safebrowsing database and
  // getfullhash result, we should not see the interstitial page since the
  // only malware was a prefetch target.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResult(malware_url, safe_browsing_util::MALWARE,
                       &malware_full_hash);
  SetupResponseForUrl(malware_url, malware_full_hash);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(ShowingInterstitialPage());
  Mock::VerifyAndClear(&observer_);

  // However, when we navigate to the malware page, we should still get
  // the interstitial.
  EXPECT_CALL(observer_, OnSafeBrowsingMatch(IsUnsafeResourceFor(malware_url)))
      .Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(malware_url)))
      .Times(1);
  ui_test_utils::NavigateToURL(browser(), malware_url);
  EXPECT_TRUE(ShowingInterstitialPage());
  Mock::VerifyAndClear(&observer_);
  g_browser_process->safe_browsing_service()->
      ui_manager()->RemoveObserver(&observer_);
}

}  // namespace

class TestSBClient
    : public base::RefCountedThreadSafe<TestSBClient>,
      public SafeBrowsingDatabaseManager::Client {
 public:
  TestSBClient()
    : threat_type_(SB_THREAT_TYPE_SAFE),
      safe_browsing_service_(g_browser_process->safe_browsing_service()) {
  }

  SBThreatType GetThreatType() const {
    return threat_type_;
  }

  void CheckDownloadUrl(const std::vector<GURL>& url_chain) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&TestSBClient::CheckDownloadUrlOnIOThread,
                   this, url_chain));
    content::RunMessageLoop();  // Will stop in OnCheckDownloadUrlResult.
  }

 private:
  friend class base::RefCountedThreadSafe<TestSBClient>;
  virtual ~TestSBClient() {}

  void CheckDownloadUrlOnIOThread(const std::vector<GURL>& url_chain) {
    safe_browsing_service_->database_manager()->
        CheckDownloadUrl(url_chain, this);
  }

  // Called when the result of checking a download URL is known.
  virtual void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                        SBThreatType threat_type) OVERRIDE {
    threat_type_ = threat_type;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&TestSBClient::DownloadCheckDone, this));
  }

  void DownloadCheckDone() {
    base::MessageLoopForUI::current()->Quit();
  }

  SBThreatType threat_type_;
  SafeBrowsingService* safe_browsing_service_;

  DISALLOW_COPY_AND_ASSIGN(TestSBClient);
};

// These tests use SafeBrowsingService::Client to directly interact with
// SafeBrowsingService.
namespace {

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, CheckDownloadUrl) {
  GURL badbin_url = test_server()->GetURL(kMalwareFile);
  std::vector<GURL> badbin_urls(1, badbin_url);

  scoped_refptr<TestSBClient> client(new TestSBClient);
  client->CheckDownloadUrl(badbin_urls);

  // Since badbin_url is not in database, it is considered to be safe.
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

  SBFullHashResult full_hash_result;
  GenUrlFullhashResult(badbin_url, safe_browsing_util::BINURL,
                       &full_hash_result);
  SetupResponseForUrl(badbin_url, full_hash_result);

  client->CheckDownloadUrl(badbin_urls);

  // Now, the badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SB_THREAT_TYPE_BINARY_MALWARE_URL, client->GetThreatType());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, CheckDownloadUrlRedirects) {
  GURL original_url = test_server()->GetURL(kEmptyPage);
  GURL badbin_url = test_server()->GetURL(kMalwareFile);
  GURL final_url = test_server()->GetURL(kEmptyPage);
  std::vector<GURL> badbin_urls;
  badbin_urls.push_back(original_url);
  badbin_urls.push_back(badbin_url);
  badbin_urls.push_back(final_url);

  scoped_refptr<TestSBClient> client(new TestSBClient);
  client->CheckDownloadUrl(badbin_urls);

  // Since badbin_url is not in database, it is considered to be safe.
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

  SBFullHashResult full_hash_result;
  GenUrlFullhashResult(badbin_url, safe_browsing_util::BINURL,
                       &full_hash_result);
  SetupResponseForUrl(badbin_url, full_hash_result);

  client->CheckDownloadUrl(badbin_urls);

  // Now, the badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SB_THREAT_TYPE_BINARY_MALWARE_URL, client->GetThreatType());
}

#if defined(OS_WIN)
// http://crbug.com/396409
#define MAYBE_CheckDownloadUrlTimedOut DISABLED_CheckDownloadUrlTimedOut
#else
#define MAYBE_CheckDownloadUrlTimedOut CheckDownloadUrlTimedOut
#endif
IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest,
                       MAYBE_CheckDownloadUrlTimedOut) {
  GURL badbin_url = test_server()->GetURL(kMalwareFile);
  std::vector<GURL> badbin_urls(1, badbin_url);

  scoped_refptr<TestSBClient> client(new TestSBClient);
  SBFullHashResult full_hash_result;
  GenUrlFullhashResult(badbin_url, safe_browsing_util::BINURL,
                       &full_hash_result);
  SetupResponseForUrl(badbin_url, full_hash_result);
  client->CheckDownloadUrl(badbin_urls);

  // badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SB_THREAT_TYPE_BINARY_MALWARE_URL, client->GetThreatType());

  //
  // Now introducing delays and we should hit timeout.
  //
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  base::TimeDelta default_urlcheck_timeout = GetCheckTimeout(sb_service);
  IntroduceGetHashDelay(base::TimeDelta::FromSeconds(1));
  SetCheckTimeout(sb_service, base::TimeDelta::FromMilliseconds(1));
  client->CheckDownloadUrl(badbin_urls);

  // There should be a timeout and the hash would be considered as safe.
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

  // Need to set the timeout back to the default value.
  SetCheckTimeout(sb_service, default_urlcheck_timeout);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, StartAndStop) {
  CreateCSDService();
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  safe_browsing::ClientSideDetectionService* csd_service =
      sb_service->safe_browsing_detection_service();
  PrefService* pref_service = browser()->profile()->GetPrefs();

  ASSERT_TRUE(sb_service != NULL);
  ASSERT_TRUE(csd_service != NULL);
  ASSERT_TRUE(pref_service != NULL);

  EXPECT_TRUE(pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));

  // SBS might still be starting, make sure this doesn't flake.
  WaitForIOThread();
  EXPECT_TRUE(sb_service->enabled());
  EXPECT_TRUE(csd_service->enabled());

  // Add a new Profile. SBS should keep running.
  ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
  scoped_ptr<Profile> profile2(Profile::CreateProfile(
      temp_profile_dir_.path(), NULL, Profile::CREATE_MODE_SYNCHRONOUS));
  ASSERT_TRUE(profile2.get() != NULL);
  StartupTaskRunnerServiceFactory::GetForProfile(profile2.get())->
              StartDeferredTaskRunners();
  PrefService* pref_service2 = profile2->GetPrefs();
  EXPECT_TRUE(pref_service2->GetBoolean(prefs::kSafeBrowsingEnabled));
  // We don't expect the state to have changed, but if it did, wait for it.
  WaitForIOThread();
  EXPECT_TRUE(sb_service->enabled());
  EXPECT_TRUE(csd_service->enabled());

  // Change one of the prefs. SBS should keep running.
  pref_service->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  WaitForIOThread();
  EXPECT_TRUE(sb_service->enabled());
  EXPECT_TRUE(csd_service->enabled());

  // Change the other pref. SBS should stop now.
  pref_service2->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  WaitForIOThread();
  EXPECT_FALSE(sb_service->enabled());
  EXPECT_FALSE(csd_service->enabled());

  // Turn it back on. SBS comes back.
  pref_service2->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  WaitForIOThread();
  EXPECT_TRUE(sb_service->enabled());
  EXPECT_TRUE(csd_service->enabled());

  // Delete the Profile. SBS stops again.
  pref_service2 = NULL;
  profile2.reset();
  WaitForIOThread();
  EXPECT_FALSE(sb_service->enabled());
  EXPECT_FALSE(csd_service->enabled());
}

}  // namespace

class SafeBrowsingServiceShutdownTest : public SafeBrowsingServiceTest {
 public:
  virtual void TearDown() OVERRIDE {
    // Browser should be fully torn down by now, so we can safely check these
    // counters.
    EXPECT_EQ(1, TestProtocolManager::create_count());
    EXPECT_EQ(1, TestProtocolManager::delete_count());

    SafeBrowsingServiceTest::TearDown();
  }

  // An observer that returns back to test code after a new profile is
  // initialized.
  void OnUnblockOnProfileCreation(Profile* profile,
                                  Profile::CreateStatus status) {
    if (status == Profile::CREATE_STATUS_INITIALIZED) {
      profile2_ = profile;
      base::MessageLoop::current()->Quit();
    }
  }

 protected:
  Profile* profile2_;
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceShutdownTest,
                       DontStartAfterShutdown) {
  CreateCSDService();
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  safe_browsing::ClientSideDetectionService* csd_service =
      sb_service->safe_browsing_detection_service();
  PrefService* pref_service = browser()->profile()->GetPrefs();

  ASSERT_TRUE(sb_service != NULL);
  ASSERT_TRUE(csd_service != NULL);
  ASSERT_TRUE(pref_service != NULL);

  EXPECT_TRUE(pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));

  // SBS might still be starting, make sure this doesn't flake.
  WaitForIOThread();
  EXPECT_EQ(1, TestProtocolManager::create_count());
  EXPECT_EQ(0, TestProtocolManager::delete_count());

  // Create an additional profile.  We need to use the ProfileManager so that
  // the profile will get destroyed in the normal browser shutdown process.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
  profile_manager->CreateProfileAsync(
      temp_profile_dir_.path(),
      base::Bind(&SafeBrowsingServiceShutdownTest::OnUnblockOnProfileCreation,
                 this),
      base::string16(), base::string16(), std::string());

  // Spin to allow profile creation to take place, loop is terminated
  // by OnUnblockOnProfileCreation when the profile is created.
  content::RunMessageLoop();

  PrefService* pref_service2 = profile2_->GetPrefs();
  EXPECT_TRUE(pref_service2->GetBoolean(prefs::kSafeBrowsingEnabled));

  // We don't expect the state to have changed, but if it did, wait for it.
  WaitForIOThread();
  EXPECT_EQ(1, TestProtocolManager::create_count());
  EXPECT_EQ(0, TestProtocolManager::delete_count());

  // End the test, shutting down the browser.
  // SafeBrowsingServiceShutdownTest::TearDown will check the create_count and
  // delete_count again.
}

class SafeBrowsingDatabaseManagerCookieTest : public InProcessBrowserTest {
 public:
  SafeBrowsingDatabaseManagerCookieTest() {}

  virtual void SetUp() OVERRIDE {
    // We need to start the test server to get the host&port in the url.
    ASSERT_TRUE(test_server()->Start());

    // Point to the testing server for all SafeBrowsing requests.
    GURL url_prefix = test_server()->GetURL(
        "expect-and-set-cookie?expect=a%3db"
        "&set=c%3dd%3b%20Expires=Fri,%2001%20Jan%202038%2001:01:01%20GMT"
        "&data=foo#");
    sb_factory_.reset(new TestSafeBrowsingServiceFactory(url_prefix.spec()));
    SafeBrowsingService::RegisterFactory(sb_factory_.get());

    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    InProcessBrowserTest::TearDown();

    SafeBrowsingService::RegisterFactory(NULL);
  }

  virtual bool SetUpUserDataDirectory() OVERRIDE {
    base::FilePath cookie_path(
        SafeBrowsingService::GetCookieFilePathForTesting());
    EXPECT_FALSE(base::PathExists(cookie_path));

    base::FilePath test_dir;
    if (!PathService::Get(chrome::DIR_TEST_DATA, &test_dir)) {
      EXPECT_TRUE(false);
      return false;
    }

    // Initialize the SafeBrowsing cookies with a pre-created cookie store.  It
    // contains a single cookie, for domain 127.0.0.1, with value a=b, and
    // expires in 2038.
    base::FilePath initial_cookies = test_dir.AppendASCII("safe_browsing")
        .AppendASCII("Safe Browsing Cookies");
    if (!base::CopyFile(initial_cookies, cookie_path)) {
      EXPECT_TRUE(false);
      return false;
    }

    sql::Connection db;
    if (!db.Open(cookie_path)) {
      EXPECT_TRUE(false);
      return false;
    }
    // Ensure the host value in the cookie file matches the test server we will
    // be connecting to.
    sql::Statement smt(db.GetUniqueStatement(
        "UPDATE cookies SET host_key = ?"));
    if (!smt.is_valid()) {
      EXPECT_TRUE(false);
      return false;
    }
    if (!smt.BindString(0, test_server()->host_port_pair().host())) {
      EXPECT_TRUE(false);
      return false;
    }
    if (!smt.Run()) {
      EXPECT_TRUE(false);
      return false;
    }

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();

    sql::Connection db;
    base::FilePath cookie_path(
        SafeBrowsingService::GetCookieFilePathForTesting());
    ASSERT_TRUE(db.Open(cookie_path));

    sql::Statement smt(db.GetUniqueStatement(
        "SELECT name, value FROM cookies ORDER BY name"));
    ASSERT_TRUE(smt.is_valid());

    ASSERT_TRUE(smt.Step());
    ASSERT_EQ("a", smt.ColumnString(0));
    ASSERT_EQ("b", smt.ColumnString(1));
    ASSERT_TRUE(smt.Step());
    ASSERT_EQ("c", smt.ColumnString(0));
    ASSERT_EQ("d", smt.ColumnString(1));
    EXPECT_FALSE(smt.Step());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    sb_service_ = g_browser_process->safe_browsing_service();
    ASSERT_TRUE(sb_service_.get() != NULL);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    sb_service_ = NULL;
  }

  void ForceUpdate() {
    sb_service_->protocol_manager()->ForceScheduleNextUpdate(
        base::TimeDelta::FromSeconds(0));
  }

  scoped_refptr<SafeBrowsingService> sb_service_;

 private:
  scoped_ptr<TestSafeBrowsingServiceFactory> sb_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseManagerCookieTest);
};

// Test that a Safe Browsing database update request both sends cookies and can
// save cookies.
IN_PROC_BROWSER_TEST_F(SafeBrowsingDatabaseManagerCookieTest,
                       TestSBUpdateCookies) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE,
      content::Source<SafeBrowsingDatabaseManager>(
          sb_service_->database_manager().get()));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManagerCookieTest::ForceUpdate, this));
  observer.Wait();
}
