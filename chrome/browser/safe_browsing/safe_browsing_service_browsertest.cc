// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a safebrowsing service using test safebrowsing database
// and a test protocol manager. It is used to test logics in safebrowsing
// service.

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/ref_counted.h"
#include "base/sha2.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

using base::Histogram;
using base::StatisticsRecorder;

namespace {

// A SafeBrowingDatabase class that allows us to inject the malicious URLs.
class TestSafeBrowsingDatabase :  public SafeBrowsingDatabase {
 public:
  TestSafeBrowsingDatabase() {}

  virtual ~TestSafeBrowsingDatabase() {}

  // Initializes the database with the given filename.
  virtual void Init(const FilePath& filename) {}

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase() {
    badurls_.clear();
    return true;
  }

  // Called on the IO thread to check if the given URL is safe or not.  If we
  // can synchronously determine that the URL is safe, CheckUrl returns true,
  // otherwise it returns false.
  virtual bool ContainsBrowseUrl(const GURL& url,
                                 std::string* matching_list,
                                 std::vector<SBPrefix>* prefix_hits,
                                 std::vector<SBFullHashResult>* full_hits,
                                 base::Time last_update) {
    return ContainsUrl(safe_browsing_util::kMalwareList,
                       safe_browsing_util::kPhishingList,
                       url, prefix_hits, full_hits);
  }
  virtual bool ContainsDownloadUrl(const GURL& url,
                                   SBPrefix* prefix_hit) {
    std::vector<SBFullHashResult> full_hits;
    std::vector<SBPrefix> prefix_hits;
    bool found = ContainsUrl(safe_browsing_util::kBinUrlList,
                             safe_browsing_util::kBinHashList,
                             url, &prefix_hits, &full_hits);
    if (!found)
      return false;
    DCHECK_EQ(1U, prefix_hits.size());
    *prefix_hit = prefix_hits[0];
    return true;
  }
  virtual bool ContainsDownloadHashPrefix(const SBPrefix& prefix) {
    return download_digest_prefix_.count(prefix) > 0;
  }
  virtual bool UpdateStarted(std::vector<SBListChunkRanges>* lists) {
    ADD_FAILURE() << "Not implemented.";
    return false;
  }
  virtual void InsertChunks(const std::string& list_name,
                            const SBChunkList& chunks) {
    ADD_FAILURE() << "Not implemented.";
  }
  virtual void DeleteChunks(const std::vector<SBChunkDelete>& chunk_deletes) {
    ADD_FAILURE() << "Not implemented.";
  }
  virtual void UpdateFinished(bool update_succeeded) {
    ADD_FAILURE() << "Not implemented.";
  }
  virtual void CacheHashResults(const std::vector<SBPrefix>& prefixes,
      const std::vector<SBFullHashResult>& full_hits) {
    // Do nothing for the cache.
    return;
  }

  // Fill up the database with test URL.
  void AddUrl(const GURL& url,
              const std::string& list_name,
              const std::vector<SBPrefix>& prefix_hits,
              const std::vector<SBFullHashResult>& full_hits) {
    badurls_[url.spec()].list_name = list_name;
    badurls_[url.spec()].prefix_hits = prefix_hits;
    badurls_[url.spec()].full_hits = full_hits;
  }

  // Fill up the database with test hash digest.
  void AddDownloadPrefix(SBPrefix prefix) {
    download_digest_prefix_.insert(prefix);
  }

 private:
  struct Hits {
    std::string list_name;
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> full_hits;
  };

  bool ContainsUrl(const std::string& list_name0,
                   const std::string& list_name1,
                   const GURL& url,
                   std::vector<SBPrefix>* prefix_hits,
                   std::vector<SBFullHashResult>* full_hits) {
    base::hash_map<std::string, Hits>::const_iterator
        badurls_it = badurls_.find(url.spec());

    if (badurls_it == badurls_.end())
      return false;

    if (badurls_it->second.list_name == list_name0 ||
        badurls_it->second.list_name == list_name1) {
      *prefix_hits = badurls_it->second.prefix_hits;
      *full_hits = badurls_it->second.full_hits;
      return true;
    }

    return false;
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
      bool enable_download_protection) {
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
  TestProtocolManager(SafeBrowsingService* sb_service,
                      const std::string& client_name,
                      const std::string& client_key,
                      const std::string& wrapped_key,
                      URLRequestContextGetter* request_context_getter,
                      const std::string& info_url_prefix,
                      const std::string& mackey_url_prefix,
                      bool disable_auto_update)
      : SafeBrowsingProtocolManager(sb_service, client_name, client_key,
                                    wrapped_key, request_context_getter,
                                    info_url_prefix, mackey_url_prefix,
                                    disable_auto_update),
        sb_service_(sb_service) {
  }

  // This function is called when there is a prefix hit in local safebrowsing
  // database and safebrowsing service issues a get hash request to backends.
  // We return a result from the prefilled full_hashes_ hash_map to simulate
  // server's response.
  virtual void GetFullHash(SafeBrowsingService::SafeBrowsingCheck* check,
                           const std::vector<SBPrefix>& prefixes) {
    // When we get a valid response, always cache the result.
    bool cancache = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            sb_service_, &SafeBrowsingService::HandleGetHashResults,
            check, full_hashes_, cancache));
  }

  // Prepare the GetFullHash results for the next request.
  void SetGetFullHashResponse(const SBFullHashResult& full_hash_result) {
    full_hashes_.clear();
    full_hashes_.push_back(full_hash_result);
  }

 private:
  std::vector<SBFullHashResult> full_hashes_;
  SafeBrowsingService* sb_service_;
};

// Factory that creates TestProtocolManager instances.
class TestSBProtocolManagerFactory : public SBProtocolManagerFactory {
 public:
  TestSBProtocolManagerFactory() : pm_(NULL) {}
  virtual ~TestSBProtocolManagerFactory() {}

  virtual SafeBrowsingProtocolManager* CreateProtocolManager(
      SafeBrowsingService* sb_service,
      const std::string& client_name,
      const std::string& client_key,
      const std::string& wrapped_key,
      URLRequestContextGetter* request_context_getter,
      const std::string& info_url_prefix,
      const std::string& mackey_url_prefix,
      bool disable_auto_update) {
    pm_ = new TestProtocolManager(
        sb_service, client_name, client_key, wrapped_key,
        request_context_getter, info_url_prefix, mackey_url_prefix,
        disable_auto_update);
    return pm_;
  }
  TestProtocolManager* GetProtocolManager() {
    return pm_;
  }
 private:
  // Owned by the SafebrowsingService.
  TestProtocolManager* pm_;
};

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingServiceTest() {
  }

  static void GenUrlFullhashResult(const GURL& url,
                                   const std::string& list_name,
                                   int add_chunk_id,
                                   SBFullHashResult* full_hash) {
    std::string host;
    std::string path;
    safe_browsing_util::CanonicalizeUrl(url, &host, &path, NULL);
    base::SHA256HashString(host + path, &full_hash->hash,
                           sizeof(SBFullHash));
    full_hash->list_name = list_name;
    full_hash->add_chunk_id = add_chunk_id;
  }

  static void GenDigestFullhashResult(const std::string& full_digest,
                                      const std::string& list_name,
                                      int add_chunk_id,
                                      SBFullHashResult* full_hash) {
    safe_browsing_util::StringToSBFullHash(full_digest, &full_hash->hash);
    full_hash->list_name = list_name;
    full_hash->add_chunk_id = add_chunk_id;
  }

  virtual void SetUp() {
    // InProcessBrowserTest::SetUp() intantiates SafebrowsingService and
    // RegisterFactory has to be called before SafeBrowsingService is created.
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
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    // Makes sure the auto update is not triggered during the test.
    // This test will fill up the database using testing prefixes
    // and urls.
    command_line->AppendSwitch(switches::kSbDisableAutoUpdate);

    command_line->AppendSwitch(switches::kSbEnableDownloadProtection);
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
    std::vector<SBFullHashResult> empty_full_hits;
    TestSafeBrowsingDatabase* db = db_factory_.GetDb();
    db->AddUrl(url, full_hash.list_name, prefix_hits, empty_full_hits);

    TestProtocolManager* pm = pm_factory_.GetProtocolManager();
    pm->SetGetFullHashResponse(full_hash);
  }

  // This will setup the binary digest prefix in database and prepare protocol
  // manager to response with |full_hash| for get full hash request.
  void SetupResponseForDigest(const std::string& digest,
                              const SBFullHashResult& hash_result) {
    TestSafeBrowsingDatabase* db = db_factory_.GetDb();
    SBFullHash full_hash;
    safe_browsing_util::StringToSBFullHash(digest, &full_hash);
    db->AddDownloadPrefix(full_hash.prefix);

    TestProtocolManager* pm = pm_factory_.GetProtocolManager();
    pm->SetGetFullHashResponse(hash_result);
  }

  bool ShowingInterstitialPage() {
    TabContents* contents = browser()->GetSelectedTabContents();
    InterstitialPage* interstitial_page = contents->interstitial_page();
    return interstitial_page != NULL;
  }

 private:
  TestSafeBrowsingDatabaseFactory db_factory_;
  TestSBProtocolManagerFactory pm_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceTest);
};

const char kEmptyPage[] = "files/empty.html";
const char kMalwareFile[] = "files/downloads/dangerous/dangerous.exe";
const char kMalwareIframe[] = "files/safe_browsing/malware_iframe.html";
const char kMalwarePage[] = "files/safe_browsing/malware.html";

// This test goes through DownloadResourceHandler.
IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, Malware) {
  GURL url = test_server()->GetURL(kEmptyPage);

  // After adding the url to safebrowsing database and getfullhash result,
  // we should see the interstitial page.
  SBFullHashResult malware_full_hash;
  int chunk_id = 0;
  GenUrlFullhashResult(url, safe_browsing_util::kMalwareList, chunk_id,
                       &malware_full_hash);
  SetupResponseForUrl(url, malware_full_hash);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(ShowingInterstitialPage());
}

// This test uses SafeBrowsingService::Client to directly interact with
// SafeBrowsingService.
class TestSBClient
    : public base::RefCountedThreadSafe<TestSBClient>,
      public SafeBrowsingService::Client {
 public:
  TestSBClient() : result_(SafeBrowsingService::SAFE),
                   safe_browsing_service_(g_browser_process->
                                          resource_dispatcher_host()->
                                          safe_browsing_service()) {
  }

  int GetResult() {
    return result_;
  }

  void CheckDownloadUrl(const GURL& url) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &TestSBClient::CheckDownloadUrlOnIOThread,
                          url));
    ui_test_utils::RunMessageLoop();  // Will stop in OnDownloadUrlCheckResult.
  }

  void CheckDownloadHash(const std::string& full_hash) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &TestSBClient::CheckDownloadHashOnIOThread,
                          full_hash));
    ui_test_utils::RunMessageLoop();  // Will stop in OnDownloadHashCheckResult.
  }

 private:
  void CheckDownloadUrlOnIOThread(const GURL& url) {
    safe_browsing_service_->CheckDownloadUrl(url, this);
  }

  void CheckDownloadHashOnIOThread(const std::string& full_hash) {
    safe_browsing_service_->CheckDownloadHash(full_hash, this);
  }

  // Called when the result of checking a download URL is known.
  void OnDownloadUrlCheckResult(const GURL& url,
                                SafeBrowsingService::UrlCheckResult result) {
    result_ = result;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &TestSBClient::DownloadCheckDone));
  }

  // Called when the result of checking a download hash is known.
  void OnDownloadHashCheckResult(const SBFullHash& hash,
                                 SafeBrowsingService::UrlCheckResult result) {
    result_ = result;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &TestSBClient::DownloadCheckDone));
  }

  void DownloadCheckDone() {
    MessageLoopForUI::current()->Quit();
  }

  SafeBrowsingService::UrlCheckResult result_;
  SafeBrowsingService* safe_browsing_service_;

  DISALLOW_COPY_AND_ASSIGN(TestSBClient);
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, CheckDownloadUrl) {
  GURL badbin_url = test_server()->GetURL(kMalwareFile);

  scoped_refptr<TestSBClient> client(new TestSBClient);
  client->CheckDownloadUrl(badbin_url);

  // Since badbin_url is not in database, it is considered to be safe.
  EXPECT_EQ(SafeBrowsingService::SAFE, client->GetResult());

  SBFullHashResult full_hash_result;
  int chunk_id = 0;
  GenUrlFullhashResult(badbin_url, safe_browsing_util::kBinUrlList,
                       chunk_id, &full_hash_result);
  SetupResponseForUrl(badbin_url, full_hash_result);

  client->CheckDownloadUrl(badbin_url);

  // Now, the badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SafeBrowsingService::BINARY_MALWARE_URL, client->GetResult());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, CheckDownloadHash) {
  const std::string full_hash = "12345678902234567890323456789012";

  scoped_refptr<TestSBClient> client(new TestSBClient);
  client->CheckDownloadHash(full_hash);

  // Since badbin_url is not in database, it is considered to be safe.
  EXPECT_EQ(SafeBrowsingService::SAFE, client->GetResult());

  SBFullHashResult full_hash_result;
  int chunk_id = 0;
  GenDigestFullhashResult(full_hash, safe_browsing_util::kBinHashList,
                          chunk_id, &full_hash_result);
  SetupResponseForDigest(full_hash, full_hash_result);

  client->CheckDownloadHash(full_hash);

  // Now, the badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SafeBrowsingService::BINARY_MALWARE_HASH, client->GetResult());
}
}  // namespace
