// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_cache.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestExtensionId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kTestExtensionId3[] = "cccccccccccccccccccccccccccccccc";
const char kTestExtensionId4[] = "dddddddddddddddddddddddddddddddd";
const char kNonWebstoreUpdateUrl[] = "https://localhost/service/update2/crx";

}  // namespace

namespace chromeos {

class ExternalCacheTest : public testing::Test,
                          public ExternalCache::Delegate {
 public:
  ExternalCacheTest()
    : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD) {
  }
  ~ExternalCacheTest() override {}

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() {
    return background_task_runner_;
  }

  net::URLRequestContextGetter* request_context_getter() {
    return request_context_getter_.get();
  }

  const base::DictionaryValue* provided_prefs() {
    return prefs_.get();
  }

  // testing::Test overrides:
  void SetUp() override {
    request_context_getter_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO));
    fetcher_factory_.reset(new net::TestURLFetcherFactory());

    pool_owner_.reset(
        new base::SequencedWorkerPoolOwner(3, "Background Pool"));
    background_task_runner_ = pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("background"));
  }

  // ExternalCache::Delegate:
  void OnExtensionListsUpdated(const base::DictionaryValue* prefs) override {
    prefs_.reset(prefs->DeepCopy());
  }

  std::string GetInstalledExtensionVersion(const std::string& id) override {
    std::map<std::string, std::string>::iterator it =
        installed_extensions_.find(id);
    return it != installed_extensions_.end() ? it->second : std::string();
  }

  base::FilePath CreateCacheDir(bool initialized) {
    EXPECT_TRUE(cache_dir_.CreateUniqueTempDir());
    if (initialized)
      CreateFlagFile(cache_dir_.GetPath());
    return cache_dir_.GetPath();
  }

  base::FilePath CreateTempDir() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    return temp_dir_.GetPath();
  }

  void CreateFlagFile(const base::FilePath& dir) {
    CreateFile(dir.Append(
        extensions::LocalExtensionCache::kCacheReadyFlagFileName));
  }

  void CreateExtensionFile(const base::FilePath& dir,
                           const std::string& id,
                           const std::string& version) {
    CreateFile(GetExtensionFile(dir, id, version));
  }

  void CreateFile(const base::FilePath& file) {
    EXPECT_EQ(base::WriteFile(file, NULL, 0), 0);
  }

  base::FilePath GetExtensionFile(const base::FilePath& dir,
                                  const std::string& id,
                                  const std::string& version) {
    return dir.Append(id + "-" + version + ".crx");
  }

  base::DictionaryValue* CreateEntryWithUpdateUrl(bool from_webstore) {
    base::DictionaryValue* entry = new base::DictionaryValue;
    entry->SetString(extensions::ExternalProviderImpl::kExternalUpdateUrl,
        from_webstore ? extension_urls::GetWebstoreUpdateUrl().spec()
                      : kNonWebstoreUpdateUrl);
    return entry;
  }

  void WaitForCompletion() {
    // Wait for background task completion that sends replay to UI thread.
    pool_owner_->pool()->FlushForTesting();
    // Wait for UI thread task completion.
    base::RunLoop().RunUntilIdle();
  }

  void AddInstalledExtension(const std::string& id,
                             const std::string& version) {
    installed_extensions_[id] = version;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::unique_ptr<net::TestURLFetcherFactory> fetcher_factory_;

  std::unique_ptr<base::SequencedWorkerPoolOwner> pool_owner_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::ScopedTempDir cache_dir_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::DictionaryValue> prefs_;
  std::map<std::string, std::string> installed_extensions_;

  ScopedTestDeviceSettingsService test_device_settings_service_;
  ScopedTestCrosSettings test_cros_settings_;

  DISALLOW_COPY_AND_ASSIGN(ExternalCacheTest);
};

TEST_F(ExternalCacheTest, Basic) {
  base::FilePath cache_dir(CreateCacheDir(false));
  ExternalCache external_cache(cache_dir, request_context_getter(),
      background_task_runner(), this, true, false);

  std::unique_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);
  base::DictionaryValue* dict = CreateEntryWithUpdateUrl(true);
  prefs->Set(kTestExtensionId1, dict);
  CreateExtensionFile(cache_dir, kTestExtensionId1, "1");
  dict = CreateEntryWithUpdateUrl(true);
  prefs->Set(kTestExtensionId2, dict);
  prefs->Set(kTestExtensionId3, CreateEntryWithUpdateUrl(false));
  CreateExtensionFile(cache_dir, kTestExtensionId3, "3");
  prefs->Set(kTestExtensionId4, CreateEntryWithUpdateUrl(false));

  external_cache.UpdateExtensionsList(std::move(prefs));
  WaitForCompletion();

  ASSERT_TRUE(provided_prefs());
  EXPECT_EQ(provided_prefs()->size(), 2ul);

  // File in cache from Webstore.
  const base::DictionaryValue* entry1 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId1, &entry1));
  EXPECT_FALSE(entry1->HasKey(
      extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_TRUE(entry1->HasKey(
      extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_TRUE(entry1->HasKey(
      extensions::ExternalProviderImpl::kExternalVersion));
  bool from_webstore = false;
  EXPECT_TRUE(entry1->GetBoolean(
      extensions::ExternalProviderImpl::kIsFromWebstore, &from_webstore));
  EXPECT_TRUE(from_webstore);

  // File in cache not from Webstore.
  const base::DictionaryValue* entry3 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId3, &entry3));
  EXPECT_FALSE(entry3->HasKey(
      extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_TRUE(entry3->HasKey(
      extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_TRUE(entry3->HasKey(
      extensions::ExternalProviderImpl::kExternalVersion));
  EXPECT_FALSE(entry3->HasKey(
      extensions::ExternalProviderImpl::kIsFromWebstore));

  // Update from Webstore.
  base::FilePath temp_dir(CreateTempDir());
  base::FilePath temp_file2 = temp_dir.Append("b.crx");
  CreateFile(temp_file2);
  external_cache.OnExtensionDownloadFinished(
      extensions::CRXFileInfo(kTestExtensionId2, temp_file2), true, GURL(), "2",
      extensions::ExtensionDownloaderDelegate::PingResult(), std::set<int>(),
      extensions::ExtensionDownloaderDelegate::InstallCallback());

  WaitForCompletion();
  EXPECT_EQ(provided_prefs()->size(), 3ul);

  const base::DictionaryValue* entry2 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId2, &entry2));
  EXPECT_FALSE(entry2->HasKey(
      extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_TRUE(entry2->HasKey(
      extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_TRUE(entry2->HasKey(
      extensions::ExternalProviderImpl::kExternalVersion));
  from_webstore = false;
  EXPECT_TRUE(entry2->GetBoolean(
      extensions::ExternalProviderImpl::kIsFromWebstore, &from_webstore));
  EXPECT_TRUE(from_webstore);
  EXPECT_TRUE(base::PathExists(
      GetExtensionFile(cache_dir, kTestExtensionId2, "2")));

  // Update not from Webstore.
  base::FilePath temp_file4 = temp_dir.Append("d.crx");
  CreateFile(temp_file4);
  external_cache.OnExtensionDownloadFinished(
      extensions::CRXFileInfo(kTestExtensionId4, temp_file4), true, GURL(), "4",
      extensions::ExtensionDownloaderDelegate::PingResult(), std::set<int>(),
      extensions::ExtensionDownloaderDelegate::InstallCallback());

  WaitForCompletion();
  EXPECT_EQ(provided_prefs()->size(), 4ul);

  const base::DictionaryValue* entry4 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId4, &entry4));
  EXPECT_FALSE(entry4->HasKey(
      extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_TRUE(entry4->HasKey(
      extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_TRUE(entry4->HasKey(
      extensions::ExternalProviderImpl::kExternalVersion));
  EXPECT_FALSE(entry4->HasKey(
      extensions::ExternalProviderImpl::kIsFromWebstore));
  EXPECT_TRUE(base::PathExists(
      GetExtensionFile(cache_dir, kTestExtensionId4, "4")));

  // Damaged file should be removed from disk.
  external_cache.OnDamagedFileDetected(
      GetExtensionFile(cache_dir, kTestExtensionId2, "2"));
  WaitForCompletion();
  EXPECT_EQ(provided_prefs()->size(), 3ul);
  EXPECT_FALSE(base::PathExists(
      GetExtensionFile(cache_dir, kTestExtensionId2, "2")));

  // Shutdown with callback OnExtensionListsUpdated that clears prefs.
  std::unique_ptr<base::DictionaryValue> empty(new base::DictionaryValue);
  external_cache.Shutdown(
        base::Bind(&ExternalCacheTest::OnExtensionListsUpdated,
                   base::Unretained(this),
                   base::Unretained(empty.get())));
  WaitForCompletion();
  EXPECT_EQ(provided_prefs()->size(), 0ul);

  // After Shutdown directory shouldn't be touched.
  external_cache.OnDamagedFileDetected(
      GetExtensionFile(cache_dir, kTestExtensionId4, "4"));
  WaitForCompletion();
  EXPECT_TRUE(base::PathExists(
      GetExtensionFile(cache_dir, kTestExtensionId4, "4")));
}

TEST_F(ExternalCacheTest, PreserveInstalled) {
  base::FilePath cache_dir(CreateCacheDir(false));
  ExternalCache external_cache(cache_dir, request_context_getter(),
      background_task_runner(), this, true, false);

  std::unique_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);
  prefs->Set(kTestExtensionId1, CreateEntryWithUpdateUrl(true));
  prefs->Set(kTestExtensionId2, CreateEntryWithUpdateUrl(true));

  AddInstalledExtension(kTestExtensionId1, "1");

  external_cache.UpdateExtensionsList(std::move(prefs));
  WaitForCompletion();

  ASSERT_TRUE(provided_prefs());
  EXPECT_EQ(provided_prefs()->size(), 1ul);

  // File not in cache but extension installed.
  const base::DictionaryValue* entry1 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId1, &entry1));
  EXPECT_TRUE(entry1->HasKey(
      extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_FALSE(entry1->HasKey(
      extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_FALSE(entry1->HasKey(
      extensions::ExternalProviderImpl::kExternalVersion));
}

}  // namespace chromeos
