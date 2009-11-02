// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/file_util.h"
#include "base/rand_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/net/test_url_fetcher_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "libxml/globals.h"

#if defined(OS_MACOSX)
// These tests crash frequently on Mac 10.5 Tests debug. http://crbug.com/26035.
#define MAYBE_TestBlacklistDownloading DISABLED_TestBlacklistDownloading
#define MAYBE_TestBlacklistUpdateCheckRequests DISABLED_TestBlacklistUpdateCheckRequests
#else
#define MAYBE_TestBlacklistDownloading TestBlacklistDownloading
#define MAYBE_TestBlacklistUpdateCheckRequests TestBlacklistUpdateCheckRequests
#endif

static int expected_load_flags =
    net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;

// Do-nothing base class for further specialized test classes.
class MockService : public ExtensionUpdateService {
 public:
  MockService() {}
  virtual ~MockService() {}

  virtual const ExtensionList* extensions() const {
    EXPECT_TRUE(false);
    return NULL;
  }

  virtual void UpdateExtension(const std::string& id,
                               const FilePath& extension_path) {
    EXPECT_TRUE(false);
  }

  virtual Extension* GetExtensionById(const std::string& id) {
    EXPECT_TRUE(false);
    return NULL;
  }

  virtual void UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist) {
    EXPECT_TRUE(false);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(MockService);
};

// Class that contains a PrefService and handles cleanup of a temporary file
// backing it.
class ScopedTempPrefService {
 public:
  ScopedTempPrefService() {
    // Make sure different tests won't use the same prefs file. It will cause
    // problem when different tests are running in parallel.
    temp_dir_.CreateUniqueTempDir();
    FilePath pref_file = temp_dir_.path().AppendASCII("prefs");
    prefs_.reset(new PrefService(pref_file));
  }

  ~ScopedTempPrefService() {}

  PrefService* get() {
    return prefs_.get();
  }

 private:
  // Ordering matters, we want |prefs_| to be destroyed before |temp_dir_|.
  ScopedTempDir temp_dir_;
  scoped_ptr<PrefService> prefs_;
};

// Creates test extensions and inserts them into list. The name and
// version are all based on their index. If |update_url| is non-null, it
// will be used as the update_url for each extension.
void CreateTestExtensions(int count, ExtensionList *list,
                          const std::string* update_url) {
  for (int i = 1; i <= count; i++) {
    DictionaryValue input;
#if defined(OS_WIN)
    FilePath path(StringPrintf(L"c:\\extension%i", i));
#else
    FilePath path(StringPrintf("/extension%i", i));
#endif
    Extension* e = new Extension(path);
    input.SetString(extension_manifest_keys::kVersion,
                    StringPrintf("%d.0.0.0", i));
    input.SetString(extension_manifest_keys::kName,
                    StringPrintf("Extension %d", i));
    if (update_url)
      input.SetString(extension_manifest_keys::kUpdateURL, *update_url);
    std::string error;
    EXPECT_TRUE(e->InitFromValue(input, false, &error));
    list->push_back(e);
  }
}

class ServiceForManifestTests : public MockService {
 public:
  ServiceForManifestTests() {}

  virtual ~ServiceForManifestTests() {}

  virtual Extension* GetExtensionById(const std::string& id) {
    for (ExtensionList::iterator iter = extensions_.begin();
        iter != extensions_.end(); ++iter) {
     if ((*iter)->id() == id) {
       return *iter;
     }
    }
    return NULL;
  }

  virtual const ExtensionList* extensions() const { return &extensions_; }

  void set_extensions(ExtensionList extensions) {
    extensions_ = extensions;
  }

 private:
  ExtensionList extensions_;
};

class ServiceForDownloadTests : public MockService {
 public:
  virtual void UpdateExtension(const std::string& id,
                               const FilePath& extension_path) {
    extension_id_ = id;
    install_path_ = extension_path;
  }

  virtual Extension* GetExtensionById(const std::string& id) {
    last_inquired_extension_id_ = id;
    return NULL;
  }

  const std::string& extension_id() { return extension_id_; }
  const FilePath& install_path() { return install_path_; }
  const std::string& last_inquired_extension_id() {
    return last_inquired_extension_id_;
  }

 private:
  std::string extension_id_;
  FilePath install_path_;
  // The last extension_id that GetExtensionById was called with.
  std::string last_inquired_extension_id_;
};

class ServiceForBlacklistTests : public MockService {
 public:
  ServiceForBlacklistTests()
     : MockService(),
       processed_blacklist_(false) {
  }
  virtual void UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist) {
    processed_blacklist_ = true;
    return;
  }
  bool processed_blacklist() { return processed_blacklist_; }
  const std::string& extension_id() { return extension_id_; }

 private:
  bool processed_blacklist_;
  std::string extension_id_;
  FilePath install_path_;
};


static const int kUpdateFrequencySecs = 15;

// Takes a string with KEY=VALUE parameters separated by '&' in |params| and
// puts the key/value pairs into |result|. For keys with no value, the empty
// string is used. So for "a=1&b=foo&c", result would map "a" to "1", "b" to
// "foo", and "c" to "".
static void ExtractParameters(const std::string& params,
                              std::map<std::string, std::string>* result) {
  std::vector<std::string> pairs;
  SplitString(params, '&', &pairs);
  for (size_t i = 0; i < pairs.size(); i++) {
    std::vector<std::string> key_val;
    SplitString(pairs[i], '=', &key_val);
    if (key_val.size() > 0) {
      std::string key = key_val[0];
      EXPECT_TRUE(result->find(key) == result->end());
      (*result)[key] = (key_val.size() == 2) ? key_val[1] : "";
    } else {
      NOTREACHED();
    }
  }
}

// All of our tests that need to use private APIs of ExtensionUpdater live
// inside this class (which is a friend to ExtensionUpdater).
class ExtensionUpdaterTest : public testing::Test {
 public:


  static void SimulateTimerFired(ExtensionUpdater* updater) {
    EXPECT_TRUE(updater->timer_.IsRunning());
    updater->timer_.Stop();
    updater->TimerFired();
  }

  // Adds a Result with the given data to results.
  static void AddParseResult(
      const std::string& id,
      const std::string& version,
      const std::string& url,
      std::vector<UpdateManifest::Result>* results) {
    UpdateManifest::Result result;
    result.extension_id = id;
    result.version = version;
    result.crx_url = GURL(url);
    results->push_back(result);
  }

  static void TestExtensionUpdateCheckRequests() {
    // Create an extension with an update_url.
    ServiceForManifestTests service;
    ExtensionList tmp;
    std::string update_url("http://foo.com/bar");
    CreateTestExtensions(1, &tmp, &update_url);
    service.set_extensions(tmp);

    // Setup and start the updater.
    MessageLoop message_loop;
    ChromeThread io_thread(ChromeThread::IO);
    io_thread.Start();

    TestURLFetcherFactory factory;
    URLFetcher::set_factory(&factory);
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
        new ExtensionUpdater(&service, prefs.get(), 60*60*24);
    updater->Start();

    // Tell the update that it's time to do update checks.
    SimulateTimerFired(updater.get());

    // Get the url our mock fetcher was asked to fetch.
    TestURLFetcher* fetcher =
        factory.GetFetcherByID(ExtensionUpdater::kManifestFetcherId);
    const GURL& url = fetcher->original_url();
    EXPECT_FALSE(url.is_empty());
    EXPECT_TRUE(url.is_valid());
    EXPECT_TRUE(url.SchemeIs("http"));
    EXPECT_EQ("foo.com", url.host());
    EXPECT_EQ("/bar", url.path());

    // Validate the extension request parameters in the query. It should
    // look something like "?x=id%3D<id>%26v%3D<version>%26uc".
    EXPECT_TRUE(url.has_query());
    std::vector<std::string> parts;
    SplitString(url.query(), '=', &parts);
    EXPECT_EQ(2u, parts.size());
    EXPECT_EQ("x", parts[0]);
    std::string decoded = UnescapeURLComponent(parts[1],
                                               UnescapeRule::URL_SPECIAL_CHARS);
    std::map<std::string, std::string> params;
    ExtractParameters(decoded, &params);
    EXPECT_EQ(tmp[0]->id(), params["id"]);
    EXPECT_EQ(tmp[0]->VersionString(), params["v"]);
    EXPECT_EQ("", params["uc"]);

    STLDeleteElements(&tmp);
  }

  static void TestBlacklistUpdateCheckRequests() {
    ServiceForManifestTests service;

    // Setup and start the updater.
    MessageLoop message_loop;
    ChromeThread io_thread(ChromeThread::IO);
    io_thread.Start();

    TestURLFetcherFactory factory;
    URLFetcher::set_factory(&factory);
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
        new ExtensionUpdater(&service, prefs.get(), 60*60*24);
    updater->Start();

    // Tell the updater that it's time to do update checks.
    SimulateTimerFired(updater.get());

    // Get the url our mock fetcher was asked to fetch.
    TestURLFetcher* fetcher =
        factory.GetFetcherByID(ExtensionUpdater::kManifestFetcherId);
    const GURL& url = fetcher->original_url();

    EXPECT_FALSE(url.is_empty());
    EXPECT_TRUE(url.is_valid());
    EXPECT_TRUE(url.SchemeIs("https"));
    EXPECT_EQ("clients2.google.com", url.host());
    EXPECT_EQ("/service/update2/crx", url.path());

    // Validate the extension request parameters in the query. It should
    // look something like "?x=id%3D<id>%26v%3D<version>%26uc".
    EXPECT_TRUE(url.has_query());
    std::vector<std::string> parts;
    SplitString(url.query(), '=', &parts);
    EXPECT_EQ(2u, parts.size());
    EXPECT_EQ("x", parts[0]);
    std::string decoded = UnescapeURLComponent(parts[1],
                                               UnescapeRule::URL_SPECIAL_CHARS);
    std::map<std::string, std::string> params;
    ExtractParameters(decoded, &params);
    EXPECT_EQ("com.google.crx.blacklist", params["id"]);
    EXPECT_EQ("0", params["v"]);
    EXPECT_EQ("", params["uc"]);
  }

  static void TestDetermineUpdates() {
    // Create a set of test extensions
    ServiceForManifestTests service;
    ExtensionList tmp;
    CreateTestExtensions(3, &tmp, NULL);
    service.set_extensions(tmp);

    MessageLoop message_loop;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
        new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs);

    // Check passing an empty list of parse results to DetermineUpdates
    std::vector<UpdateManifest::Result> updates;
    std::vector<int> updateable = updater->DetermineUpdates(updates);
    EXPECT_TRUE(updateable.empty());

    // Create two updates - expect that DetermineUpdates will return the first
    // one (v1.0 installed, v1.1 available) but not the second one (both
    // installed and available at v2.0).
    scoped_ptr<Version> one(Version::GetVersionFromString("1.0"));
    EXPECT_TRUE(tmp[0]->version()->Equals(*one));
    AddParseResult(tmp[0]->id(),
        "1.1", "http://localhost/e1_1.1.crx", &updates);
    AddParseResult(tmp[1]->id(),
        tmp[1]->VersionString(), "http://localhost/e2_2.0.crx", &updates);
    updateable = updater->DetermineUpdates(updates);
    EXPECT_EQ(1u, updateable.size());
    EXPECT_EQ(0, updateable[0]);
    STLDeleteElements(&tmp);
  }

  static void TestMultipleManifestDownloading() {
    MessageLoop ui_loop;
    ChromeThread ui_thread(ChromeThread::UI, &ui_loop);
    ChromeThread file_thread(ChromeThread::FILE);
    file_thread.Start();
    ChromeThread io_thread(ChromeThread::IO);
    io_thread.Start();

    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForDownloadTests service;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
        new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs);

    GURL url1("http://localhost/manifest1");
    GURL url2("http://localhost/manifest2");

    // Request 2 update checks - the first should begin immediately and the
    // second one should be queued up.
    updater->StartUpdateCheck(url1);
    updater->StartUpdateCheck(url2);

    std::string invalid_xml = "invalid xml";
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kManifestFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    EXPECT_TRUE(fetcher->load_flags() == expected_load_flags);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url1, URLRequestStatus(), 200, ResponseCookies(),
        invalid_xml);

    // Now that the first request is complete, make sure the second one has
    // been started.
    const std::string kValidXml =
        "<?xml version='1.0' encoding='UTF-8'?>"
        "<gupdate xmlns='http://www.google.com/update2/response'"
        "                protocol='2.0'>"
        " <app appid='12345'>"
        "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
        "               version='1.2.3.4' prodversionmin='2.0.143.0' />"
        " </app>"
        "</gupdate>";
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kManifestFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    EXPECT_TRUE(fetcher->load_flags() == expected_load_flags);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url2, URLRequestStatus(), 200, ResponseCookies(),
        kValidXml);

    // This should run the manifest parsing, then we want to make sure that our
    // service was called with GetExtensionById with the matching id from
    // kValidXml.
    file_thread.Stop();
    io_thread.Stop();
    ui_loop.RunAllPending();
    EXPECT_EQ("12345", service.last_inquired_extension_id());
    xmlCleanupGlobals();
  }

  static void TestSingleExtensionDownloading() {
    MessageLoop ui_loop;
    ChromeThread ui_thread(ChromeThread::UI, &ui_loop);
    ChromeThread file_thread(ChromeThread::FILE);
    file_thread.Start();
    ChromeThread io_thread(ChromeThread::IO);
    io_thread.Start();

    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForDownloadTests service;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
        new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs);

    GURL test_url("http://localhost/extension.crx");

    std::string id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    std::string hash = "";
    std::string version = "0.0.1";

    updater->FetchUpdatedExtension(id, test_url, hash, version);

    // Call back the ExtensionUpdater with a 200 response and some test data
    std::string extension_data("whatever");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kExtensionFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    EXPECT_TRUE(fetcher->load_flags() == expected_load_flags);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, test_url, URLRequestStatus(), 200, ResponseCookies(),
        extension_data);

    file_thread.Stop();
    ui_loop.RunAllPending();

    // Expect that ExtensionUpdater asked the mock extensions service to install
    // a file with the test data for the right id.
    EXPECT_EQ(id, service.extension_id());
    FilePath tmpfile_path = service.install_path();
    EXPECT_FALSE(tmpfile_path.empty());
    std::string file_contents;
    EXPECT_TRUE(file_util::ReadFileToString(tmpfile_path, &file_contents));
    EXPECT_TRUE(extension_data == file_contents);

    file_util::Delete(tmpfile_path, false);
    URLFetcher::set_factory(NULL);
  }

  static void TestBlacklistDownloading() {
    MessageLoop message_loop;
    ChromeThread ui_thread(ChromeThread::UI, &message_loop);
    ChromeThread io_thread(ChromeThread::IO);
    io_thread.Start();

    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForBlacklistTests service;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
        new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs);
    prefs.get()->
      RegisterStringPref(prefs::kExtensionBlacklistUpdateVersion, L"0");
    GURL test_url("http://localhost/extension.crx");

    std::string id = "com.google.crx.blacklist";

    std::string hash =
      "2CE109E9D0FAF820B2434E166297934E6177B65AB9951DBC3E204CAD4689B39C";

    std::string version = "0.0.1";

    updater->FetchUpdatedExtension(id, test_url, hash, version);

    // Call back the ExtensionUpdater with a 200 response and some test data
    std::string extension_data("aaabbb");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kExtensionFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    EXPECT_TRUE(fetcher->load_flags() == expected_load_flags);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, test_url, URLRequestStatus(), 200, ResponseCookies(),
        extension_data);

    message_loop.RunAllPending();

    // The updater should have called extension service to process the
    // blacklist.
    EXPECT_TRUE(service.processed_blacklist());

    EXPECT_EQ(version, WideToASCII(prefs.get()->
      GetString(prefs::kExtensionBlacklistUpdateVersion)));

    URLFetcher::set_factory(NULL);
  }

  static void TestMultipleExtensionDownloading() {
    MessageLoopForUI message_loop;
    ChromeThread ui_thread(ChromeThread::UI, &message_loop);
    ChromeThread file_thread(ChromeThread::FILE, &message_loop);
    ChromeThread io_thread(ChromeThread::IO);
    io_thread.Start();

    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForDownloadTests service;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
        new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs);

    GURL url1("http://localhost/extension1.crx");
    GURL url2("http://localhost/extension2.crx");

    std::string id1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    std::string id2 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

    std::string hash1 = "";
    std::string hash2 = "";

    std::string version1 = "0.1";
    std::string version2 = "0.1";
    // Start two fetches
    updater->FetchUpdatedExtension(id1, url1, hash1, version1);
    updater->FetchUpdatedExtension(id2, url2, hash2, version2);

    // Make the first fetch complete.
    std::string extension_data1("whatever");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kExtensionFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    EXPECT_TRUE(fetcher->load_flags() == expected_load_flags);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url1, URLRequestStatus(), 200, ResponseCookies(),
        extension_data1);
    message_loop.RunAllPending();

    // Expect that the service was asked to do an install with the right data.
    FilePath tmpfile_path = service.install_path();
    EXPECT_FALSE(tmpfile_path.empty());
    EXPECT_EQ(id1, service.extension_id());
    message_loop.RunAllPending();
    file_util::Delete(tmpfile_path, false);

    // Make sure the second fetch finished and asked the service to do an
    // update.
    std::string extension_data2("whatever2");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kExtensionFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    EXPECT_TRUE(fetcher->load_flags() == expected_load_flags);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url2, URLRequestStatus(), 200, ResponseCookies(),
        extension_data2);
    message_loop.RunAllPending();
    EXPECT_EQ(id2, service.extension_id());
    EXPECT_FALSE(service.install_path().empty());

    // Make sure the correct crx contents were passed for the update call.
    std::string file_contents;
    EXPECT_TRUE(file_util::ReadFileToString(service.install_path(),
                                            &file_contents));
    EXPECT_TRUE(extension_data2 == file_contents);
    file_util::Delete(service.install_path(), false);
  }
};

// Because we test some private methods of ExtensionUpdater, it's easer for the
// actual test code to live in ExtenionUpdaterTest methods instead of TEST_F
// subclasses where friendship with ExtenionUpdater is not inherited.

TEST(ExtensionUpdaterTest, TestExtensionUpdateCheckRequests) {
  ExtensionUpdaterTest::TestExtensionUpdateCheckRequests();
}

// This test is disabled on Mac, see http://crbug.com/26035.
TEST(ExtensionUpdaterTest, MAYBE_TestBlacklistUpdateCheckRequests) {
  ExtensionUpdaterTest::TestBlacklistUpdateCheckRequests();
}

TEST(ExtensionUpdaterTest, TestDetermineUpdates) {
  ExtensionUpdaterTest::TestDetermineUpdates();
}

TEST(ExtensionUpdaterTest, TestMultipleManifestDownloading) {
  ExtensionUpdaterTest::TestMultipleManifestDownloading();
}

TEST(ExtensionUpdaterTest, TestSingleExtensionDownloading) {
  ExtensionUpdaterTest::TestSingleExtensionDownloading();
}

// This test is disabled on Mac, see http://crbug.com/26035.
TEST(ExtensionUpdaterTest, MAYBE_TestBlacklistDownloading) {
  ExtensionUpdaterTest::TestBlacklistDownloading();
}

TEST(ExtensionUpdaterTest, TestMultipleExtensionDownloading) {
  ExtensionUpdaterTest::TestMultipleExtensionDownloading();
}


// TODO(asargent) - (http://crbug.com/12780) add tests for:
// -prodversionmin (shouldn't update if browser version too old)
// -manifests & updates arriving out of order / interleaved
// -Profile::GetDefaultRequestContext() returning null
//  (should not crash, but just do check later)
// -malformed update url (empty, file://, has query, has a # fragment, etc.)
// -An extension gets uninstalled while updates are in progress (so it doesn't
//  "come back from the dead")
// -An extension gets manually updated to v3 while we're downloading v2 (ie
//  you don't get downgraded accidentally)
// -An update manifest mentions multiple updates
