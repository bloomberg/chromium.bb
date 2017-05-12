// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/features/features.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "base/memory/ptr_util.h"
#include "ppapi/shared_impl/ppapi_constants.h"  // nogncheck
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_util.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

using net::CanonicalCookie;

namespace content {
namespace {

const int kDefaultClientId = 42;
const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";

const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:1/";
const char kTestOrigin3[] = "http://host3:1/";
const char kTestOriginDevTools[] = "chrome-devtools://abcdefghijklmnopqrstuvw/";

#if BUILDFLAG(ENABLE_PLUGINS)
const char kWidevineCdmPluginId[] = "application_x-ppapi-widevine-cdm";
const char kClearKeyCdmPluginId[] = "application_x-ppapi-clearkey-cdm";
#endif  // BUILDFLAG(ENABLE_PLUGINS)

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);
const GURL kOriginDevTools(kTestOriginDevTools);

const base::FilePath::CharType kDomStorageOrigin1[] =
    FILE_PATH_LITERAL("http_host1_1.localstorage");

const base::FilePath::CharType kDomStorageOrigin2[] =
    FILE_PATH_LITERAL("http_host2_1.localstorage");

const base::FilePath::CharType kDomStorageOrigin3[] =
    FILE_PATH_LITERAL("http_host3_1.localstorage");

const storage::StorageType kTemporary = storage::kStorageTypeTemporary;
const storage::StorageType kPersistent = storage::kStorageTypePersistent;

const storage::QuotaClient::ID kClientFile = storage::QuotaClient::kFileSystem;

const uint32_t kAllQuotaRemoveMask =
    StoragePartition::REMOVE_DATA_MASK_APPCACHE |
    StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
    StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
    StoragePartition::REMOVE_DATA_MASK_WEBSQL;

bool AlwaysTrueCookiePredicate(const net::CanonicalCookie& cookie) {
  return true;
}

bool AlwaysFalseCookiePredicate(const net::CanonicalCookie& cookie) {
  return false;
}

class AwaitCompletionHelper {
 public:
  AwaitCompletionHelper() : start_(false), already_quit_(false) {}
  virtual ~AwaitCompletionHelper() {}

  void BlockUntilNotified() {
    if (!already_quit_) {
      DCHECK(!start_);
      start_ = true;
      base::RunLoop().Run();
    } else {
      DCHECK(!start_);
      already_quit_ = false;
    }
  }

  void Notify() {
    if (start_) {
      DCHECK(!already_quit_);
      base::MessageLoop::current()->QuitWhenIdle();
      start_ = false;
    } else {
      DCHECK(!already_quit_);
      already_quit_ = true;
    }
  }

 private:
  // Helps prevent from running message_loop, if the callback invoked
  // immediately.
  bool start_;
  bool already_quit_;

  DISALLOW_COPY_AND_ASSIGN(AwaitCompletionHelper);
};

class RemoveCookieTester {
 public:
  explicit RemoveCookieTester(TestBrowserContext* context)
      : get_cookie_success_(false),
        cookie_store_(context->GetRequestContext()
                          ->GetURLRequestContext()
                          ->cookie_store()) {}

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie() {
    get_cookie_success_ = false;
    cookie_store_->GetCookiesWithOptionsAsync(
        kOrigin1, net::CookieOptions(),
        base::Bind(&RemoveCookieTester::GetCookieCallback,
                   base::Unretained(this)));
    await_completion_.BlockUntilNotified();
    return get_cookie_success_;
  }

  void AddCookie() {
    cookie_store_->SetCookieWithOptionsAsync(
        kOrigin1, "A=1", net::CookieOptions(),
        base::Bind(&RemoveCookieTester::SetCookieCallback,
                   base::Unretained(this)));
    await_completion_.BlockUntilNotified();
  }

 private:
  void GetCookieCallback(const std::string& cookies) {
    if (cookies == "A=1") {
      get_cookie_success_ = true;
    } else {
      EXPECT_EQ("", cookies);
      get_cookie_success_ = false;
    }
    await_completion_.Notify();
  }

  void SetCookieCallback(bool result) {
    ASSERT_TRUE(result);
    await_completion_.Notify();
  }

  bool get_cookie_success_;
  AwaitCompletionHelper await_completion_;
  net::CookieStore* cookie_store_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCookieTester);
};

class RemoveLocalStorageTester {
 public:
  explicit RemoveLocalStorageTester(TestBrowserContext* profile)
      : profile_(profile), dom_storage_context_(NULL) {
    dom_storage_context_ =
        content::BrowserContext::GetDefaultStoragePartition(profile)->
            GetDOMStorageContext();
  }

  // Returns true, if the given origin URL exists.
  bool DOMStorageExistsForOrigin(const GURL& origin) {
    GetLocalStorageUsage();
    await_completion_.BlockUntilNotified();
    for (size_t i = 0; i < infos_.size(); ++i) {
      if (origin == infos_[i].origin)
        return true;
    }
    return false;
  }

  void AddDOMStorageTestData() {
    // Note: This test depends on details of how the dom_storage library
    // stores data in the host file system.
    base::FilePath storage_path =
        profile_->GetPath().AppendASCII("Local Storage");
    base::CreateDirectory(storage_path);

    // Write some files.
    base::WriteFile(storage_path.Append(kDomStorageOrigin1), NULL, 0);
    base::WriteFile(storage_path.Append(kDomStorageOrigin2), NULL, 0);
    base::WriteFile(storage_path.Append(kDomStorageOrigin3), NULL, 0);

    // Tweak their dates.
    base::Time now = base::Time::Now();
    base::TouchFile(storage_path.Append(kDomStorageOrigin1), now, now);

    base::Time one_day_ago = now - base::TimeDelta::FromDays(1);
    base::TouchFile(storage_path.Append(kDomStorageOrigin2),
                    one_day_ago, one_day_ago);

    base::Time sixty_days_ago = now - base::TimeDelta::FromDays(60);
    base::TouchFile(storage_path.Append(kDomStorageOrigin3),
                    sixty_days_ago, sixty_days_ago);
  }

 private:
  void GetLocalStorageUsage() {
    dom_storage_context_->GetLocalStorageUsage(
        base::Bind(&RemoveLocalStorageTester::OnGotLocalStorageUsage,
                   base::Unretained(this)));
  }
  void OnGotLocalStorageUsage(
      const std::vector<content::LocalStorageUsageInfo>& infos) {
    infos_ = infos;
    await_completion_.Notify();
  }

  // We don't own these pointers.
  TestBrowserContext* profile_;
  content::DOMStorageContext* dom_storage_context_;

  std::vector<content::LocalStorageUsageInfo> infos_;

  AwaitCompletionHelper await_completion_;

  DISALLOW_COPY_AND_ASSIGN(RemoveLocalStorageTester);
};

#if BUILDFLAG(ENABLE_PLUGINS)
class RemovePluginPrivateDataTester {
 public:
  explicit RemovePluginPrivateDataTester(
      storage::FileSystemContext* filesystem_context)
      : filesystem_context_(filesystem_context) {}

  // Add some files to the PluginPrivateFileSystem. They are created as follows:
  //   kOrigin1 - ClearKey - 1 file - timestamp 10 days ago
  //   kOrigin2 - Widevine - 2 files - timestamps now and 60 days ago
  void AddPluginPrivateTestData() {
    base::Time now = base::Time::Now();
    base::Time ten_days_ago = now - base::TimeDelta::FromDays(10);
    base::Time sixty_days_ago = now - base::TimeDelta::FromDays(60);

    // Create a PluginPrivateFileSystem for ClearKey and add a single file
    // with a timestamp of 1 day ago.
    std::string clearkey_fsid =
        CreateFileSystem(kClearKeyCdmPluginId, kOrigin1);
    clearkey_file_ = CreateFile(kOrigin1, clearkey_fsid, "foo");
    SetFileTimestamp(clearkey_file_, ten_days_ago);

    // Create a second PluginPrivateFileSystem for Widevine and add two files
    // with different times.
    std::string widevine_fsid =
        CreateFileSystem(kWidevineCdmPluginId, kOrigin2);
    storage::FileSystemURL widevine_file1 =
        CreateFile(kOrigin2, widevine_fsid, "bar1");
    storage::FileSystemURL widevine_file2 =
        CreateFile(kOrigin2, widevine_fsid, "bar2");
    SetFileTimestamp(widevine_file1, now);
    SetFileTimestamp(widevine_file2, sixty_days_ago);
  }

  void DeleteClearKeyTestData() { DeleteFile(clearkey_file_); }

  // Returns true, if the given origin exists in a PluginPrivateFileSystem.
  bool DataExistsForOrigin(const GURL& origin) {
    AwaitCompletionHelper await_completion;
    bool data_exists_for_origin = false;
    filesystem_context_->default_file_task_runner()->PostTask(
        FROM_HERE, base::Bind(&RemovePluginPrivateDataTester::
                                  CheckIfDataExistsForOriginOnFileTaskRunner,
                              base::Unretained(this), origin,
                              &data_exists_for_origin, &await_completion));
    await_completion.BlockUntilNotified();
    return data_exists_for_origin;
  }

  // Opens the file created for ClearKey (in kOrigin1) for writing. Caller
  // needs to verify if the file was opened or not.
  base::File OpenClearKeyFileForWrite() {
    AwaitCompletionHelper await_completion;
    base::File file;
    storage::AsyncFileUtil* async_file_util =
        filesystem_context_->GetAsyncFileUtil(
            storage::kFileSystemTypePluginPrivate);
    std::unique_ptr<storage::FileSystemOperationContext> operation_context =
        base::MakeUnique<storage::FileSystemOperationContext>(
            filesystem_context_);
    async_file_util->CreateOrOpen(
        std::move(operation_context), clearkey_file_,
        base::File::FLAG_OPEN | base::File::FLAG_WRITE,
        base::Bind(&RemovePluginPrivateDataTester::OnFileOpened,
                   base::Unretained(this), &file, &await_completion));
    await_completion.BlockUntilNotified();
    return file;
  }

 private:
  // Creates a PluginPrivateFileSystem for the |plugin_name| and |origin|
  // provided. Returns the file system ID for the created
  // PluginPrivateFileSystem.
  std::string CreateFileSystem(const std::string& plugin_name,
                               const GURL& origin) {
    AwaitCompletionHelper await_completion;
    std::string fsid = storage::IsolatedContext::GetInstance()
                           ->RegisterFileSystemForVirtualPath(
                               storage::kFileSystemTypePluginPrivate,
                               ppapi::kPluginPrivateRootName, base::FilePath());
    EXPECT_TRUE(storage::ValidateIsolatedFileSystemId(fsid));
    filesystem_context_->OpenPluginPrivateFileSystem(
        origin, storage::kFileSystemTypePluginPrivate, fsid, plugin_name,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::Bind(&RemovePluginPrivateDataTester::OnFileSystemOpened,
                   base::Unretained(this), &await_completion));
    await_completion.BlockUntilNotified();
    return fsid;
  }

  // Creates a file named |file_name| in the PluginPrivateFileSystem identified
  // by |origin| and |fsid|. Returns the URL for the created file. The file
  // must not already exist or the test will fail.
  storage::FileSystemURL CreateFile(const GURL& origin,
                                    const std::string& fsid,
                                    const std::string& file_name) {
    AwaitCompletionHelper await_completion;
    std::string root = storage::GetIsolatedFileSystemRootURIString(
        origin, fsid, ppapi::kPluginPrivateRootName);
    storage::FileSystemURL file_url =
        filesystem_context_->CrackURL(GURL(root + file_name));
    storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
        storage::kFileSystemTypePluginPrivate);
    std::unique_ptr<storage::FileSystemOperationContext> operation_context =
        base::MakeUnique<storage::FileSystemOperationContext>(
            filesystem_context_);
    operation_context->set_allowed_bytes_growth(
        storage::QuotaManager::kNoLimit);
    file_util->EnsureFileExists(
        std::move(operation_context), file_url,
        base::Bind(&RemovePluginPrivateDataTester::OnFileCreated,
                   base::Unretained(this), &await_completion));
    await_completion.BlockUntilNotified();
    return file_url;
  }

  void DeleteFile(storage::FileSystemURL file_url) {
    AwaitCompletionHelper await_completion;
    storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
        storage::kFileSystemTypePluginPrivate);
    std::unique_ptr<storage::FileSystemOperationContext> operation_context =
        base::MakeUnique<storage::FileSystemOperationContext>(
            filesystem_context_);
    file_util->DeleteFile(
        std::move(operation_context), file_url,
        base::Bind(&RemovePluginPrivateDataTester::OnFileDeleted,
                   base::Unretained(this), &await_completion));
    await_completion.BlockUntilNotified();
  }

  // Sets the last_access_time and last_modified_time to |time_stamp| on the
  // file specified by |file_url|. The file must already exist.
  void SetFileTimestamp(const storage::FileSystemURL& file_url,
                        const base::Time& time_stamp) {
    AwaitCompletionHelper await_completion;
    storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
        storage::kFileSystemTypePluginPrivate);
    std::unique_ptr<storage::FileSystemOperationContext> operation_context =
        base::MakeUnique<storage::FileSystemOperationContext>(
            filesystem_context_);
    file_util->Touch(std::move(operation_context), file_url, time_stamp,
                     time_stamp,
                     base::Bind(&RemovePluginPrivateDataTester::OnFileTouched,
                                base::Unretained(this), &await_completion));
    await_completion.BlockUntilNotified();
  }

  void OnFileSystemOpened(AwaitCompletionHelper* await_completion,
                          base::File::Error result) {
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    await_completion->Notify();
  }

  void OnFileCreated(AwaitCompletionHelper* await_completion,
                     base::File::Error result,
                     bool created) {
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    EXPECT_TRUE(created);
    await_completion->Notify();
  }

  void OnFileDeleted(AwaitCompletionHelper* await_completion,
                     base::File::Error result) {
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    await_completion->Notify();
  }

  void OnFileTouched(AwaitCompletionHelper* await_completion,
                     base::File::Error result) {
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    await_completion->Notify();
  }

  void OnFileOpened(base::File* file_result,
                    AwaitCompletionHelper* await_completion,
                    base::File file,
                    const base::Closure& on_close_callback) {
    *file_result = std::move(file);
    await_completion->Notify();
  }

  // If |origin| exists in the PluginPrivateFileSystem, set
  // |data_exists_for_origin| to true, false otherwise.
  void CheckIfDataExistsForOriginOnFileTaskRunner(
      const GURL& origin,
      bool* data_exists_for_origin,
      AwaitCompletionHelper* await_completion) {
    storage::FileSystemBackend* backend =
        filesystem_context_->GetFileSystemBackend(
            storage::kFileSystemTypePluginPrivate);
    storage::FileSystemQuotaUtil* quota_util = backend->GetQuotaUtil();

    // Determine the set of origins used.
    std::set<GURL> origins;
    quota_util->GetOriginsForTypeOnFileTaskRunner(
        storage::kFileSystemTypePluginPrivate, &origins);
    *data_exists_for_origin = origins.find(origin) != origins.end();

    // AwaitCompletionHelper and MessageLoop don't work on a
    // SequencedTaskRunner, so post a task on the IO thread.
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&AwaitCompletionHelper::Notify,
                                       base::Unretained(await_completion)));
  }

  // We don't own this pointer.
  storage::FileSystemContext* filesystem_context_;

  // Keep track of the URL for the ClearKey file so that it can be written to
  // or deleted.
  storage::FileSystemURL clearkey_file_;

  DISALLOW_COPY_AND_ASSIGN(RemovePluginPrivateDataTester);
};
#endif  // BUILDFLAG(ENABLE_PLUGINS)

bool IsWebSafeSchemeForTest(const std::string& scheme) {
  return scheme == "http";
}

bool DoesOriginMatchForUnprotectedWeb(
    const GURL& origin,
    storage::SpecialStoragePolicy* special_storage_policy) {
  if (IsWebSafeSchemeForTest(origin.scheme()))
    return !special_storage_policy->IsStorageProtected(origin.GetOrigin());

  return false;
}

bool DoesOriginMatchForBothProtectedAndUnprotectedWeb(
    const GURL& origin,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return true;
}

bool DoesOriginMatchUnprotected(
    const GURL& origin,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return origin.GetOrigin().scheme() != kOriginDevTools.scheme();
}

void ClearQuotaData(content::StoragePartition* partition,
                    base::RunLoop* loop_to_quit) {
  partition->ClearData(kAllQuotaRemoveMask,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, GURL(),
                       StoragePartition::OriginMatcherFunction(), base::Time(),
                       base::Time::Max(), loop_to_quit->QuitClosure());
}

void ClearQuotaDataWithOriginMatcher(
    content::StoragePartition* partition,
    const GURL& remove_origin,
    const StoragePartition::OriginMatcherFunction& origin_matcher,
    const base::Time delete_begin,
    base::RunLoop* loop_to_quit) {
  partition->ClearData(kAllQuotaRemoveMask,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       remove_origin, origin_matcher, delete_begin,
                       base::Time::Max(), loop_to_quit->QuitClosure());
}

void ClearQuotaDataForOrigin(
    content::StoragePartition* partition,
    const GURL& remove_origin,
    const base::Time delete_begin,
    base::RunLoop* loop_to_quit) {
  ClearQuotaDataWithOriginMatcher(
      partition, remove_origin,
      StoragePartition::OriginMatcherFunction(), delete_begin,
      loop_to_quit);
}

void ClearQuotaDataForNonPersistent(
    content::StoragePartition* partition,
    const base::Time delete_begin,
    base::RunLoop* loop_to_quit) {
  partition->ClearData(
      kAllQuotaRemoveMask,
      ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT,
      GURL(), StoragePartition::OriginMatcherFunction(), delete_begin,
      base::Time::Max(), loop_to_quit->QuitClosure());
}

void ClearCookies(content::StoragePartition* partition,
                  const base::Time delete_begin,
                  const base::Time delete_end,
                  base::RunLoop* run_loop) {
  partition->ClearData(
      StoragePartition::REMOVE_DATA_MASK_COOKIES,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      GURL(), StoragePartition::OriginMatcherFunction(),
      delete_begin, delete_end, run_loop->QuitClosure());
}

void ClearCookiesWithMatcher(
    content::StoragePartition* partition,
    const base::Time delete_begin,
    const base::Time delete_end,
    const StoragePartition::CookieMatcherFunction& cookie_matcher,
    base::RunLoop* run_loop) {
  partition->ClearData(StoragePartition::REMOVE_DATA_MASK_COOKIES,
                       StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                       StoragePartition::OriginMatcherFunction(),
                       cookie_matcher, delete_begin, delete_end,
                       run_loop->QuitClosure());
}

void ClearStuff(uint32_t remove_mask,
                content::StoragePartition* partition,
                const base::Time delete_begin,
                const base::Time delete_end,
                const StoragePartition::OriginMatcherFunction& origin_matcher,
                base::RunLoop* run_loop) {
  partition->ClearData(
      remove_mask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      GURL(), origin_matcher, delete_begin, delete_end,
      run_loop->QuitClosure());
}

void ClearData(content::StoragePartition* partition,
               base::RunLoop* run_loop) {
  base::Time time;
  partition->ClearData(
      StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      GURL(), StoragePartition::OriginMatcherFunction(),
      time, time, run_loop->QuitClosure());
}

#if BUILDFLAG(ENABLE_PLUGINS)
void ClearPluginPrivateData(content::StoragePartition* partition,
                            const GURL& storage_origin,
                            const base::Time delete_begin,
                            const base::Time delete_end,
                            base::RunLoop* run_loop) {
  partition->ClearData(
      StoragePartitionImpl::REMOVE_DATA_MASK_PLUGIN_PRIVATE_DATA,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, storage_origin,
      StoragePartition::OriginMatcherFunction(), delete_begin, delete_end,
      run_loop->QuitClosure());
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}  // namespace

class StoragePartitionImplTest : public testing::Test {
 public:
  StoragePartitionImplTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()) {}

  MockQuotaManager* GetMockManager() {
    if (!quota_manager_.get()) {
      quota_manager_ = new MockQuotaManager(
          browser_context_->IsOffTheRecord(), browser_context_->GetPath(),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO).get(),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::DB).get(),
          browser_context_->GetSpecialStoragePolicy());
    }
    return quota_manager_.get();
  }

  TestBrowserContext* browser_context() {
    return browser_context_.get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<MockQuotaManager> quota_manager_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImplTest);
};

class StoragePartitionShaderClearTest : public testing::Test {
 public:
  StoragePartitionShaderClearTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()) {
    InitShaderCacheFactorySingleton(
        base::ThreadTaskRunnerHandle::Get(),
        BrowserThread::GetTaskRunnerForThread(BrowserThread::CACHE));
    GetShaderCacheFactorySingleton()->SetCacheInfo(
        kDefaultClientId,
        BrowserContext::GetDefaultStoragePartition(browser_context())
            ->GetPath());
    cache_ = GetShaderCacheFactorySingleton()->Get(kDefaultClientId);
  }

  ~StoragePartitionShaderClearTest() override {
    cache_ = NULL;
    GetShaderCacheFactorySingleton()->RemoveCacheInfo(kDefaultClientId);
  }

  void InitCache() {
    net::TestCompletionCallback available_cb;
    int rv = cache_->SetAvailableCallback(available_cb.callback());
    ASSERT_EQ(net::OK, available_cb.GetResult(rv));
    EXPECT_EQ(0, cache_->Size());

    cache_->Cache(kCacheKey, kCacheValue);

    net::TestCompletionCallback complete_cb;

    rv = cache_->SetCacheCompleteCallback(complete_cb.callback());
    ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  }

  size_t Size() { return cache_->Size(); }

  TestBrowserContext* browser_context() {
    return browser_context_.get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;

  scoped_refptr<gpu::ShaderDiskCache> cache_;
};

// Tests ---------------------------------------------------------------------

TEST_F(StoragePartitionShaderClearTest, ClearShaderCache) {
  InitCache();
  EXPECT_EQ(1u, Size());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClearData,
                 BrowserContext::GetDefaultStoragePartition(browser_context()),
                 &run_loop));
  run_loop.Run();
  EXPECT_EQ(0u, Size());
}

TEST_F(StoragePartitionImplTest, QuotaClientMaskGeneration) {
  EXPECT_EQ(storage::QuotaClient::kFileSystem,
            StoragePartitionImpl::GenerateQuotaClientMask(
                StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS));
  EXPECT_EQ(storage::QuotaClient::kDatabase,
            StoragePartitionImpl::GenerateQuotaClientMask(
                StoragePartition::REMOVE_DATA_MASK_WEBSQL));
  EXPECT_EQ(storage::QuotaClient::kAppcache,
            StoragePartitionImpl::GenerateQuotaClientMask(
                StoragePartition::REMOVE_DATA_MASK_APPCACHE));
  EXPECT_EQ(storage::QuotaClient::kIndexedDatabase,
            StoragePartitionImpl::GenerateQuotaClientMask(
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB));
  EXPECT_EQ(storage::QuotaClient::kFileSystem |
                storage::QuotaClient::kDatabase |
                storage::QuotaClient::kAppcache |
                storage::QuotaClient::kIndexedDatabase,
            StoragePartitionImpl::GenerateQuotaClientMask(kAllQuotaRemoveMask));
}

void PopulateTestQuotaManagedPersistentData(MockQuotaManager* manager) {
  manager->AddOrigin(kOrigin2, kPersistent, kClientFile, base::Time());
  manager->AddOrigin(kOrigin3, kPersistent, kClientFile,
      base::Time::Now() - base::TimeDelta::FromDays(1));

  EXPECT_FALSE(manager->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_TRUE(manager->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_TRUE(manager->OriginHasData(kOrigin3, kPersistent, kClientFile));
}

void PopulateTestQuotaManagedTemporaryData(MockQuotaManager* manager) {
  manager->AddOrigin(kOrigin1, kTemporary, kClientFile, base::Time::Now());
  manager->AddOrigin(kOrigin3, kTemporary, kClientFile,
      base::Time::Now() - base::TimeDelta::FromDays(1));

  EXPECT_TRUE(manager->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(manager->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_TRUE(manager->OriginHasData(kOrigin3, kTemporary, kClientFile));
}

void PopulateTestQuotaManagedData(MockQuotaManager* manager) {
  // Set up kOrigin1 with a temporary quota, kOrigin2 with a persistent
  // quota, and kOrigin3 with both. kOrigin1 is modified now, kOrigin2
  // is modified at the beginning of time, and kOrigin3 is modified one day
  // ago.
  PopulateTestQuotaManagedPersistentData(manager);
  PopulateTestQuotaManagedTemporaryData(manager);
}

void PopulateTestQuotaManagedNonBrowsingData(MockQuotaManager* manager) {
  manager->AddOrigin(kOriginDevTools, kTemporary, kClientFile, base::Time());
  manager->AddOrigin(kOriginDevTools, kPersistent, kClientFile, base::Time());
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverBoth) {
  PopulateTestQuotaManagedData(GetMockManager());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverOnlyTemporary) {
  PopulateTestQuotaManagedTemporaryData(GetMockManager());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverOnlyPersistent) {
  PopulateTestQuotaManagedPersistentData(GetMockManager());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverNeither) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaData, partition, &run_loop));
  run_loop.Run();

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverSpecificOrigin) {
  PopulateTestQuotaManagedData(GetMockManager());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaDataForOrigin, partition, kOrigin1,
                            base::Time(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForLastHour) {
  PopulateTestQuotaManagedData(GetMockManager());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClearQuotaDataForOrigin, partition, GURL(),
                 base::Time::Now() - base::TimeDelta::FromHours(1), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForLastWeek) {
  PopulateTestQuotaManagedData(GetMockManager());

  base::RunLoop run_loop;
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClearQuotaDataForNonPersistent, partition,
                 base::Time::Now() - base::TimeDelta::FromDays(7), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedUnprotectedOrigins) {
  // Protect kOrigin1.
  scoped_refptr<MockSpecialStoragePolicy> mock_policy =
      new MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  PopulateTestQuotaManagedData(GetMockManager());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaDataWithOriginMatcher, partition, GURL(),
                            base::Bind(&DoesOriginMatchForUnprotectedWeb),
                            base::Time(), &run_loop));
  run_loop.Run();

  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedProtectedSpecificOrigin) {
  // Protect kOrigin1.
  scoped_refptr<MockSpecialStoragePolicy> mock_policy =
      new MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  PopulateTestQuotaManagedData(GetMockManager());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  // Try to remove kOrigin1. Expect failure.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClearQuotaDataWithOriginMatcher, partition, kOrigin1,
                 base::Bind(&DoesOriginMatchForUnprotectedWeb), base::Time(),
                 &run_loop));
  run_loop.Run();

  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedProtectedOrigins) {
  // Protect kOrigin1.
  scoped_refptr<MockSpecialStoragePolicy> mock_policy =
      new MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  PopulateTestQuotaManagedData(GetMockManager());

  // Try to remove kOrigin1. Expect success.
  base::RunLoop run_loop;
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClearQuotaDataWithOriginMatcher, partition, GURL(),
                 base::Bind(&DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                 base::Time(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedIgnoreDevTools) {
  PopulateTestQuotaManagedNonBrowsingData(GetMockManager());

  base::RunLoop run_loop;
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaDataWithOriginMatcher, partition, GURL(),
                            base::Bind(&DoesOriginMatchUnprotected),
                            base::Time(), &run_loop));
  run_loop.Run();

  // Check that devtools data isn't removed.
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOriginDevTools, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOriginDevTools, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveCookieForever) {
  RemoveCookieTester tester(browser_context());

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->SetURLRequestContext(browser_context()->GetRequestContext());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearCookies, partition, base::Time(),
                            base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(StoragePartitionImplTest, RemoveCookieLastHour) {
  RemoveCookieTester tester(browser_context());

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  base::Time an_hour_ago = base::Time::Now() - base::TimeDelta::FromHours(1);
  partition->SetURLRequestContext(browser_context()->GetRequestContext());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearCookies, partition, an_hour_ago,
                            base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(StoragePartitionImplTest, RemoveCookieWithMatcher) {
  RemoveCookieTester tester(browser_context());
  StoragePartition::CookieMatcherFunction true_predicate =
      base::Bind(&AlwaysTrueCookiePredicate);

  StoragePartition::CookieMatcherFunction false_predicate =
      base::Bind(&AlwaysFalseCookiePredicate);

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->SetURLRequestContext(browser_context()->GetRequestContext());

  // Return false from our predicate, and make sure the cookies is still around.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearCookiesWithMatcher, partition, base::Time(),
                            base::Time::Max(), false_predicate, &run_loop));
  run_loop.RunUntilIdle();
  EXPECT_TRUE(tester.ContainsCookie());

  // Now we return true from our predicate.
  base::RunLoop run_loop2;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearCookiesWithMatcher, partition, base::Time(),
                            base::Time::Max(), true_predicate, &run_loop2));
  run_loop2.RunUntilIdle();
  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(StoragePartitionImplTest, RemoveUnprotectedLocalStorageForever) {
  // Protect kOrigin1.
  scoped_refptr<MockSpecialStoragePolicy> mock_policy =
      new MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  RemoveLocalStorageTester tester(browser_context());

  tester.AddDOMStorageTestData();
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClearStuff,
                 StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                 partition, base::Time(), base::Time::Max(),
                 base::Bind(&DoesOriginMatchForUnprotectedWeb), &run_loop));
  run_loop.Run();

  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplTest, RemoveProtectedLocalStorageForever) {
  // Protect kOrigin1.
  scoped_refptr<MockSpecialStoragePolicy> mock_policy =
      new MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  RemoveLocalStorageTester tester(browser_context());

  tester.AddDOMStorageTestData();
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  partition->OverrideSpecialStoragePolicyForTesting(mock_policy.get());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClearStuff,
                 StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                 partition, base::Time(), base::Time::Max(),
                 base::Bind(&DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                 &run_loop));
  run_loop.Run();

  // Even if kOrigin1 is protected, it will be deleted since we specify
  // ClearData to delete protected data.
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplTest, RemoveLocalStorageForLastWeek) {
  RemoveLocalStorageTester tester(browser_context());

  tester.AddDOMStorageTestData();
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  base::Time a_week_ago = base::Time::Now() - base::TimeDelta::FromDays(7);

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ClearStuff,
                 StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                 partition, a_week_ago, base::Time::Max(),
                 base::Bind(&DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                 &run_loop));
  run_loop.Run();

  // kOrigin1 and kOrigin2 do not have age more than a week.
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(StoragePartitionImplTest, RemovePluginPrivateDataForever) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  RemovePluginPrivateDataTester tester(partition->GetFileSystemContext());
  tester.AddPluginPrivateTestData();
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearPluginPrivateData, partition, GURL(),
                            base::Time(), base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin2));
}

TEST_F(StoragePartitionImplTest, RemovePluginPrivateDataLastWeek) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  base::Time a_week_ago = base::Time::Now() - base::TimeDelta::FromDays(7);

  RemovePluginPrivateDataTester tester(partition->GetFileSystemContext());
  tester.AddPluginPrivateTestData();
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearPluginPrivateData, partition, GURL(),
                            a_week_ago, base::Time::Max(), &run_loop));
  run_loop.Run();

  // Origin1 has 1 file from 10 days ago, so it should remain around.
  // Origin2 has a current file, so it should be removed (even though the
  // second file is much older).
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin2));
}

TEST_F(StoragePartitionImplTest, RemovePluginPrivateDataForOrigin) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  RemovePluginPrivateDataTester tester(partition->GetFileSystemContext());
  tester.AddPluginPrivateTestData();
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearPluginPrivateData, partition, kOrigin1,
                            base::Time(), base::Time::Max(), &run_loop));
  run_loop.Run();

  // Only Origin1 should be deleted.
  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));
}

TEST_F(StoragePartitionImplTest, RemovePluginPrivateDataWhileWriting) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  RemovePluginPrivateDataTester tester(partition->GetFileSystemContext());
  tester.AddPluginPrivateTestData();
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  const char test_data[] = {0, 1, 2, 3, 4, 5};
  base::File file = tester.OpenClearKeyFileForWrite();
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int>(arraysize(test_data)),
            file.Write(0, test_data, arraysize(test_data)));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearPluginPrivateData, partition, GURL(),
                            base::Time(), base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin2));

  const char more_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(static_cast<int>(arraysize(more_data)),
            file.WriteAtCurrentPos(more_data, arraysize(more_data)));

  base::File file2 = tester.OpenClearKeyFileForWrite();
  EXPECT_FALSE(file2.IsValid());
}

TEST_F(StoragePartitionImplTest, RemovePluginPrivateDataAfterDeletion) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  RemovePluginPrivateDataTester tester(partition->GetFileSystemContext());
  tester.AddPluginPrivateTestData();
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  // Delete the single file saved for |kOrigin1|. This does not remove the
  // origin from the list of Origins. However, ClearPluginPrivateData() will
  // remove it.
  tester.DeleteClearKeyTestData();
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DataExistsForOrigin(kOrigin2));

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ClearPluginPrivateData, partition, GURL(),
                            base::Time(), base::Time::Max(), &run_loop));
  run_loop.Run();

  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DataExistsForOrigin(kOrigin2));
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

TEST(StoragePartitionImplStaticTest, CreatePredicateForHostCookies) {
  GURL url("http://www.example.com/");
  GURL url2("https://www.example.com/");
  GURL url3("https://www.google.com/");

  net::CookieOptions options;
  net::CookieStore::CookiePredicate predicate =
      StoragePartitionImpl::CreatePredicateForHostCookies(url);

  base::Time now = base::Time::Now();
  std::vector<std::unique_ptr<CanonicalCookie>> valid_cookies;
  valid_cookies.push_back(CanonicalCookie::Create(url, "A=B", now, options));
  valid_cookies.push_back(CanonicalCookie::Create(url, "C=F", now, options));
  // We should match a different scheme with the same host.
  valid_cookies.push_back(CanonicalCookie::Create(url2, "A=B", now, options));

  std::vector<std::unique_ptr<CanonicalCookie>> invalid_cookies;
  // We don't match domain cookies.
  invalid_cookies.push_back(
      CanonicalCookie::Create(url2, "A=B;domain=.example.com", now, options));
  invalid_cookies.push_back(CanonicalCookie::Create(url3, "A=B", now, options));

  for (const auto& cookie : valid_cookies)
    EXPECT_TRUE(predicate.Run(*cookie)) << cookie->DebugString();
  for (const auto& cookie : invalid_cookies)
    EXPECT_FALSE(predicate.Run(*cookie)) << cookie->DebugString();
}

}  // namespace content
