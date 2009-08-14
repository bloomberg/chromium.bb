// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/file_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/net/test_url_fetcher_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

const char *valid_xml =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </app>"
"</gupdate>";

const char *valid_xml_with_hash =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' "
"               hash='1234'/>"
" </app>"
"</gupdate>";

const char* missing_appid =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' />"
" </app>"
"</gupdate>";

const char* invalid_codebase =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345' status='ok'>"
"  <updatecheck codebase='example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' />"
" </app>"
"</gupdate>";

const char* missing_version =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345' status='ok'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx' />"
" </app>"
"</gupdate>";

const char* invalid_version =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345' status='ok'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx' "
"               version='1.2.3.a'/>"
" </app>"
"</gupdate>";

const char *uses_namespace_prefix =
"<?xml version='1.0' encoding='UTF-8'?>"
"<g:gupdate xmlns:g='http://www.google.com/update2/response' protocol='2.0'>"
" <g:app appid='12345'>"
"  <g:updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </g:app>"
"</g:gupdate>";

// Includes unrelated <app> tags from other xml namespaces - this should
// not cause problems.
const char *similar_tagnames =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response'"
"         xmlns:a='http://a' protocol='2.0'>"
" <a:app/>"
" <b:app xmlns:b='http://b' />"
" <app appid='12345'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </app>"
"</gupdate>";


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
    FilePath pref_file = temp_dir_.path().AppendASCII(
      StringPrintf("prefs_%lld", base::RandUint64()));
    prefs_.reset(new PrefService(pref_file, NULL));
  }

  ~ScopedTempPrefService() {}

  PrefService* get() {
    return prefs_.get();
  }

 private:
  scoped_ptr<PrefService> prefs_;
  ScopedTempDir temp_dir_;
};

const char* kIdPrefix = "000000000000000000000000000000000000000";

// Creates test extensions and inserts them into list. The name and
// version are all based on their index. If |update_url| is non-null, it
// will be used as the update_url for each extension.
void CreateTestExtensions(int count, ExtensionList *list,
                          const std::string* update_url) {
  for (int i = 1; i <= count; i++) {
    DictionaryValue input;
    Extension* e = new Extension();
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

  const std::string& extension_id() { return extension_id_; }
  const FilePath& install_path() { return install_path_; }

 private:
  std::string extension_id_;
  FilePath install_path_;
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
  static void expectParseFailure(const char *xml) {
    ScopedVector<ExtensionUpdater::ParseResult> result;
    EXPECT_FALSE(ExtensionUpdater::Parse(xml, &result.get()));
  }

  static void SimulateTimerFired(ExtensionUpdater* updater) {
    EXPECT_TRUE(updater->timer_.IsRunning());
    updater->timer_.Stop();
    updater->TimerFired();
  }

  // Make a test ParseResult
  static ExtensionUpdater::ParseResult* MakeParseResult(
      const std::string& id,
      const std::string& version,
      const std::string& url) {
    ExtensionUpdater::ParseResult *result = new ExtensionUpdater::ParseResult;
    result->extension_id = id;
    result->version.reset(Version::GetVersionFromString(version));
    result->crx_url = GURL(url);
    return result;
  }

  static void TestXmlParsing() {
    ExtensionErrorReporter::Init(false);

    // Test parsing of a number of invalid xml cases
    expectParseFailure("");
    expectParseFailure(missing_appid);
    expectParseFailure(invalid_codebase);
    expectParseFailure(missing_version);
    expectParseFailure(invalid_version);

    // Parse some valid XML, and check that all params came out as expected
    ScopedVector<ExtensionUpdater::ParseResult> results;
    EXPECT_TRUE(ExtensionUpdater::Parse(valid_xml, &results.get()));
    EXPECT_FALSE(results->empty());
    ExtensionUpdater::ParseResult* firstResult = results->at(0);
    EXPECT_EQ(GURL("http://example.com/extension_1.2.3.4.crx"),
              firstResult->crx_url);
    EXPECT_TRUE(firstResult->package_hash.empty());
    scoped_ptr<Version> version(Version::GetVersionFromString("1.2.3.4"));
    EXPECT_EQ(0, version->CompareTo(*firstResult->version.get()));
    version.reset(Version::GetVersionFromString("2.0.143.0"));
    EXPECT_EQ(0, version->CompareTo(*firstResult->browser_min_version.get()));

    // Parse some xml that uses namespace prefixes.
    results.reset();
    EXPECT_TRUE(ExtensionUpdater::Parse(uses_namespace_prefix, &results.get()));
    EXPECT_TRUE(ExtensionUpdater::Parse(similar_tagnames, &results.get()));

    // Parse xml with hash value
    results.reset();
    EXPECT_TRUE(ExtensionUpdater::Parse(valid_xml_with_hash, &results.get()));
    EXPECT_FALSE(results->empty());
    firstResult = results->at(0);
    EXPECT_EQ("1234", firstResult->package_hash);
  }

  static void TestExtensionUpdateCheckRequests() {
    // Create an extension with an update_url.
    ServiceForManifestTests service;
    ExtensionList tmp;
    std::string update_url("http://foo.com/bar");
    CreateTestExtensions(1, &tmp, &update_url);
    service.set_extensions(tmp);

    // Setup and start the updater.
    TestURLFetcherFactory factory;
    URLFetcher::set_factory(&factory);
    MessageLoop message_loop;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, prefs.get(), 60*60*24, &message_loop);
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
    TestURLFetcherFactory factory;
    URLFetcher::set_factory(&factory);
    MessageLoop message_loop;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, prefs.get(), 60*60*24, &message_loop);
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
      new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs,
                           &message_loop);

    // Check passing an empty list of parse results to DetermineUpdates
    ExtensionUpdater::ParseResultList updates;
    std::vector<int> updateable = updater->DetermineUpdates(updates);
    EXPECT_TRUE(updateable.empty());

    // Create two updates - expect that DetermineUpdates will return the first
    // one (v1.0 installed, v1.1 available) but not the second one (both
    // installed and available at v2.0).
    scoped_ptr<Version> one(Version::GetVersionFromString("1.0"));
    EXPECT_TRUE(tmp[0]->version()->Equals(*one));
    updates.push_back(MakeParseResult(tmp[0]->id(),
        "1.1", "http://localhost/e1_1.1.crx"));
    updates.push_back(MakeParseResult(tmp[1]->id(),
        tmp[1]->VersionString(), "http://localhost/e2_2.0.crx"));
    updateable = updater->DetermineUpdates(updates);
    EXPECT_EQ(1u, updateable.size());
    EXPECT_EQ(0, updateable[0]);
    STLDeleteElements(&updates);
    STLDeleteElements(&tmp);
  }

  static void TestMultipleManifestDownloading() {
    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForDownloadTests service;
    MessageLoop message_loop;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs,
                           &message_loop);

    GURL url1("http://localhost/manifest1");
    GURL url2("http://localhost/manifest2");

    // Request 2 update checks - the first should begin immediately and the
    // second one should be queued up.
    updater->StartUpdateCheck(url1);
    updater->StartUpdateCheck(url2);

    std::string manifest_data("invalid xml");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kManifestFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url1, URLRequestStatus(), 200, ResponseCookies(),
        manifest_data);

    // Now that the first request is complete, make sure the second one has
    // been started.
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kManifestFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url2, URLRequestStatus(), 200, ResponseCookies(),
        manifest_data);
  }

  static void TestSingleExtensionDownloading() {
    MessageLoop message_loop;
    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForDownloadTests service;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs,
                           &message_loop);

    GURL test_url("http://localhost/extension.crx");

    std::string id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    std::string hash = "";

    std::string version = "0.0.1";

    updater->FetchUpdatedExtension(id, test_url, hash, version);

    // Call back the ExtensionUpdater with a 200 response and some test data
    std::string extension_data("whatever");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kExtensionFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, test_url, URLRequestStatus(), 200, ResponseCookies(),
        extension_data);

    message_loop.RunAllPending();

    // Expect that ExtensionUpdater asked the mock extensions service to install
    // a file with the test data for the right id.
    EXPECT_EQ(id, service.extension_id());
    FilePath tmpfile_path = service.install_path();
    EXPECT_FALSE(tmpfile_path.empty());
    std::string file_contents;
    EXPECT_TRUE(file_util::ReadFileToString(tmpfile_path, &file_contents));
    EXPECT_TRUE(extension_data == file_contents);

    URLFetcher::set_factory(NULL);
  }

  static void TestBlacklistDownloading() {
    MessageLoop message_loop;
    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForBlacklistTests service;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs,
                           &message_loop);
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
    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForDownloadTests service;
    ScopedTempPrefService prefs;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, prefs.get(), kUpdateFrequencySecs,
                           &message_loop);

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
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url1, URLRequestStatus(), 200, ResponseCookies(),
        extension_data1);
    message_loop.RunAllPending();

    // Expect that the service was asked to do an install with the right data.
    FilePath tmpfile_path = service.install_path();
    EXPECT_FALSE(tmpfile_path.empty());
    EXPECT_EQ(id1, service.extension_id());
    message_loop.RunAllPending();

    // Make sure the second fetch finished and asked the service to do an
    // update.
    std::string extension_data2("whatever2");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kExtensionFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
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
  }
};

// Because we test some private methods of ExtensionUpdater, it's easer for the
// actual test code to live in ExtenionUpdaterTest methods instead of TEST_F
// subclasses where friendship with ExtenionUpdater is not inherited.

TEST(ExtensionUpdaterTest, TestXmlParsing) {
  ExtensionUpdaterTest::TestXmlParsing();
}

TEST(ExtensionUpdaterTest, TestExtensionUpdateCheckRequests) {
  ExtensionUpdaterTest::TestExtensionUpdateCheckRequests();
}

TEST(ExtensionUpdaterTest, TestBlacklistUpdateCheckRequests) {
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

TEST(ExtensionUpdaterTest, TestBlacklistDownloading) {
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
