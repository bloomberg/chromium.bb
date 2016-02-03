// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/quota/mock_quota_manager_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cache_storage_usage_info.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Returns a BlobProtocolHandler that uses |blob_storage_context|. Caller owns
// the memory.
scoped_ptr<storage::BlobProtocolHandler> CreateMockBlobProtocolHandler(
    storage::BlobStorageContext* blob_storage_context) {
  // The FileSystemContext and thread task runner are not actually used but a
  // task runner is needed to avoid a DCHECK in BlobURLRequestJob ctor.
  return make_scoped_ptr(new storage::BlobProtocolHandler(
      blob_storage_context, NULL, base::ThreadTaskRunnerHandle::Get().get()));
}

class CacheStorageManagerTest : public testing::Test {
 public:
  CacheStorageManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        blob_storage_context_(nullptr),
        callback_bool_(false),
        callback_error_(CACHE_STORAGE_OK),
        origin1_("http://example1.com"),
        origin2_("http://example2.com") {}

  void SetUp() override {
    ChromeBlobStorageContext* blob_storage_context(
        ChromeBlobStorageContext::GetFor(&browser_context_));
    // Wait for ChromeBlobStorageContext to finish initializing.
    base::RunLoop().RunUntilIdle();

    blob_storage_context_ = blob_storage_context->context();

    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context->context()));

    net::URLRequestContext* url_request_context =
        browser_context_.GetRequestContext()->GetURLRequestContext();

    url_request_context->set_job_factory(url_request_job_factory_.get());

    if (!MemoryOnly())
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    quota_policy_ = new MockSpecialStoragePolicy;
    mock_quota_manager_ = new MockQuotaManager(
        MemoryOnly() /* is incognito */, temp_dir_.path(),
        base::ThreadTaskRunnerHandle::Get().get(),
        base::ThreadTaskRunnerHandle::Get().get(), quota_policy_.get());
    mock_quota_manager_->SetQuota(
        GURL(origin1_), storage::kStorageTypeTemporary, 1024 * 1024 * 100);
    mock_quota_manager_->SetQuota(
        GURL(origin2_), storage::kStorageTypeTemporary, 1024 * 1024 * 100);

    quota_manager_proxy_ = new MockQuotaManagerProxy(
        mock_quota_manager_.get(), base::ThreadTaskRunnerHandle::Get().get());

    cache_manager_ = CacheStorageManager::Create(
        temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
        quota_manager_proxy_);

    cache_manager_->SetBlobParametersForCache(
        browser_context_.GetRequestContext(),
        blob_storage_context->context()->AsWeakPtr());
  }

  void TearDown() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    base::RunLoop().RunUntilIdle();
  }

  virtual bool MemoryOnly() { return false; }

  void BoolAndErrorCallback(base::RunLoop* run_loop,
                            bool value,
                            CacheStorageError error) {
    callback_bool_ = value;
    callback_error_ = error;
    run_loop->Quit();
  }

  void CacheAndErrorCallback(base::RunLoop* run_loop,
                             const scoped_refptr<CacheStorageCache>& cache,
                             CacheStorageError error) {
    callback_cache_ = cache;
    callback_error_ = error;
    run_loop->Quit();
  }

  void StringsAndErrorCallback(base::RunLoop* run_loop,
                               const std::vector<std::string>& strings,
                               CacheStorageError error) {
    callback_strings_ = strings;
    callback_error_ = error;
    run_loop->Quit();
  }

  void CachePutCallback(base::RunLoop* run_loop, CacheStorageError error) {
    callback_error_ = error;
    run_loop->Quit();
  }

  void CacheMatchCallback(
      base::RunLoop* run_loop,
      CacheStorageError error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
    callback_error_ = error;
    callback_cache_response_ = std::move(response);
    callback_data_handle_ = std::move(blob_data_handle);
    run_loop->Quit();
  }

  bool Open(const GURL& origin, const std::string& cache_name) {
    base::RunLoop loop;
    cache_manager_->OpenCache(
        origin, cache_name,
        base::Bind(&CacheStorageManagerTest::CacheAndErrorCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    bool error = callback_error_ != CACHE_STORAGE_OK;
    if (error)
      EXPECT_TRUE(!callback_cache_.get());
    else
      EXPECT_TRUE(callback_cache_.get());
    return !error;
  }

  bool Has(const GURL& origin, const std::string& cache_name) {
    base::RunLoop loop;
    cache_manager_->HasCache(
        origin, cache_name,
        base::Bind(&CacheStorageManagerTest::BoolAndErrorCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_bool_;
  }

  bool Delete(const GURL& origin, const std::string& cache_name) {
    base::RunLoop loop;
    cache_manager_->DeleteCache(
        origin, cache_name,
        base::Bind(&CacheStorageManagerTest::BoolAndErrorCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_bool_;
  }

  bool Keys(const GURL& origin) {
    base::RunLoop loop;
    cache_manager_->EnumerateCaches(
        origin, base::Bind(&CacheStorageManagerTest::StringsAndErrorCallback,
                           base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool StorageMatch(const GURL& origin,
                    const std::string& cache_name,
                    const GURL& url) {
    scoped_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest());
    request->url = url;
    base::RunLoop loop;
    cache_manager_->MatchCache(
        origin, cache_name, std::move(request),
        base::Bind(&CacheStorageManagerTest::CacheMatchCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool StorageMatchAll(const GURL& origin, const GURL& url) {
    scoped_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest());
    request->url = url;
    base::RunLoop loop;
    cache_manager_->MatchAllCaches(
        origin, std::move(request),
        base::Bind(&CacheStorageManagerTest::CacheMatchCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool CachePut(const scoped_refptr<CacheStorageCache>& cache,
                const GURL& url) {
    ServiceWorkerFetchRequest request;
    request.url = url;

    scoped_ptr<storage::BlobDataBuilder> blob_data(
        new storage::BlobDataBuilder(base::GenerateGUID()));
    blob_data->AppendData(url.spec());

    scoped_ptr<storage::BlobDataHandle> blob_handle =
        blob_storage_context_->AddFinishedBlob(blob_data.get());
    ServiceWorkerResponse response(
        url, 200, "OK", blink::WebServiceWorkerResponseTypeDefault,
        ServiceWorkerHeaderMap(), blob_handle->uuid(), url.spec().size(),
        GURL(), blink::WebServiceWorkerResponseErrorUnknown);

    CacheStorageBatchOperation operation;
    operation.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
    operation.request = request;
    operation.response = response;

    base::RunLoop loop;
    cache->BatchOperation(
        std::vector<CacheStorageBatchOperation>(1, operation),
        base::Bind(&CacheStorageManagerTest::CachePutCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool CacheMatch(const scoped_refptr<CacheStorageCache>& cache,
                  const GURL& url) {
    scoped_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest());
    request->url = url;
    base::RunLoop loop;
    cache->Match(std::move(request),
                 base::Bind(&CacheStorageManagerTest::CacheMatchCallback,
                            base::Unretained(this), base::Unretained(&loop)));
    loop.Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  CacheStorage* CacheStorageForOrigin(const GURL& origin) {
    return cache_manager_->FindOrCreateCacheStorage(origin);
  }

  int64_t GetOriginUsage(const GURL& origin) {
    base::RunLoop loop;
    cache_manager_->GetOriginUsage(
        origin, base::Bind(&CacheStorageManagerTest::UsageCallback,
                           base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_usage_;
  }

  void UsageCallback(base::RunLoop* run_loop, int64_t usage) {
    callback_usage_ = usage;
    run_loop->Quit();
  }

  std::vector<CacheStorageUsageInfo> GetAllOriginsUsage() {
    base::RunLoop loop;
    cache_manager_->GetAllOriginsUsage(
        base::Bind(&CacheStorageManagerTest::AllOriginsUsageCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_all_origins_usage_;
  }

  void AllOriginsUsageCallback(
      base::RunLoop* run_loop,
      const std::vector<CacheStorageUsageInfo>& usage) {
    callback_all_origins_usage_ = usage;
    run_loop->Quit();
  }

 protected:
  // Temporary directory must be allocated first so as to be destroyed last.
  base::ScopedTempDir temp_dir_;

  TestBrowserContext browser_context_;
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  storage::BlobStorageContext* blob_storage_context_;

  scoped_refptr<MockSpecialStoragePolicy> quota_policy_;
  scoped_refptr<MockQuotaManager> mock_quota_manager_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_ptr<CacheStorageManager> cache_manager_;

  scoped_refptr<CacheStorageCache> callback_cache_;
  int callback_bool_;
  CacheStorageError callback_error_;
  scoped_ptr<ServiceWorkerResponse> callback_cache_response_;
  scoped_ptr<storage::BlobDataHandle> callback_data_handle_;
  std::vector<std::string> callback_strings_;

  const GURL origin1_;
  const GURL origin2_;

  int64_t callback_usage_;
  std::vector<CacheStorageUsageInfo> callback_all_origins_usage_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStorageManagerTest);
};

class CacheStorageManagerMemoryOnlyTest : public CacheStorageManagerTest {
 public:
  bool MemoryOnly() override { return true; }
};

class CacheStorageManagerTestP : public CacheStorageManagerTest,
                                 public testing::WithParamInterface<bool> {
 public:
  bool MemoryOnly() override { return !GetParam(); }
};

TEST_F(CacheStorageManagerTest, TestsRunOnIOThread) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

TEST_P(CacheStorageManagerTestP, OpenCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, OpenTwoCaches) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
}

TEST_P(CacheStorageManagerTestP, CachePointersDiffer) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_NE(callback_cache_.get(), cache.get());
}

TEST_P(CacheStorageManagerTestP, Open2CachesSameNameDiffOrigins) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin2_, "foo"));
  EXPECT_NE(cache.get(), callback_cache_.get());
}

TEST_P(CacheStorageManagerTestP, OpenExistingCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(callback_cache_.get(), cache.get());
}

TEST_P(CacheStorageManagerTestP, HasCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Has(origin1_, "foo"));
  EXPECT_TRUE(callback_bool_);
}

TEST_P(CacheStorageManagerTestP, HasNonExistent) {
  EXPECT_FALSE(Has(origin1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, DeleteCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Has(origin1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, DeleteTwice) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Delete(origin1_, "foo"));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_P(CacheStorageManagerTestP, EmptyKeys) {
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_TRUE(callback_strings_.empty());
}

TEST_P(CacheStorageManagerTestP, SomeKeys) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "baz"));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("bar");
  EXPECT_EQ(expected_keys, callback_strings_);
  EXPECT_TRUE(Keys(origin2_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("baz", callback_strings_[0].c_str());
}

TEST_P(CacheStorageManagerTestP, DeletedKeysGone) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "baz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("foo", callback_strings_[0].c_str());
}

TEST_P(CacheStorageManagerTestP, StorageMatchEntryExists) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchNoEntry) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/bar")));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchNoCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_FALSE(StorageMatch(origin1_, "bar", GURL("http://example.com/foo")));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllEntryExists) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllNoEntry) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_FALSE(StorageMatchAll(origin1_, GURL("http://example.com/bar")));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllNoCaches) {
  EXPECT_FALSE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_F(CacheStorageManagerTest, StorageReuseCacheName) {
  // Deleting a cache and creating one with the same name and adding an entry
  // with the same URL should work. (see crbug.com/542668)
  const GURL kTestURL = GURL("http://example.com/foo");
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, kTestURL));
  EXPECT_TRUE(CacheMatch(callback_cache_, kTestURL));
  scoped_ptr<storage::BlobDataHandle> data_handle =
      std::move(callback_data_handle_);

  EXPECT_TRUE(Delete(origin1_, "foo"));
  // The cache is deleted but the handle to one of its entries is still
  // open. Creating a new cache in the same directory would fail on Windows.
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, kTestURL));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllEntryExistsTwice) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));

  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchInOneOfMany) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(origin1_, "baz"));

  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, Chinese) {
  EXPECT_TRUE(Open(origin1_, "你好"));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, "你好"));
  EXPECT_EQ(callback_cache_.get(), cache.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("你好", callback_strings_[0].c_str());
}

TEST_F(CacheStorageManagerTest, EmptyKey) {
  EXPECT_TRUE(Open(origin1_, ""));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, ""));
  EXPECT_EQ(cache.get(), callback_cache_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("", callback_strings_[0].c_str());
  EXPECT_TRUE(Has(origin1_, ""));
  EXPECT_TRUE(Delete(origin1_, ""));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_F(CacheStorageManagerTest, DataPersists) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin1_, "baz"));
  EXPECT_TRUE(Open(origin2_, "raz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("baz");
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_F(CacheStorageManagerMemoryOnlyTest, DataLostWhenMemoryOnly) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin2_, "baz"));
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_F(CacheStorageManagerTest, BadCacheName) {
  // Since the implementation writes cache names to disk, ensure that we don't
  // escape the directory.
  const std::string bad_name = "../../../../../../../../../../../../../../foo";
  EXPECT_TRUE(Open(origin1_, bad_name));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ(bad_name.c_str(), callback_strings_[0].c_str());
}

TEST_F(CacheStorageManagerTest, BadOriginName) {
  // Since the implementation writes origin names to disk, ensure that we don't
  // escape the directory.
  GURL bad_origin("http://../../../../../../../../../../../../../../foo");
  EXPECT_TRUE(Open(bad_origin, "foo"));
  EXPECT_TRUE(Keys(bad_origin));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("foo", callback_strings_[0].c_str());
}

TEST_P(CacheStorageManagerTestP, DeleteBeforeRelease) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_TRUE(callback_cache_->AsWeakPtr());
}

TEST_P(CacheStorageManagerTestP, OpenRunsSerially) {
  EXPECT_FALSE(Delete(origin1_, "tmp"));  // Init storage.
  CacheStorage* cache_storage = CacheStorageForOrigin(origin1_);
  cache_storage->StartAsyncOperationForTesting();

  base::RunLoop open_loop;
  cache_manager_->OpenCache(
      origin1_, "foo",
      base::Bind(&CacheStorageManagerTest::CacheAndErrorCallback,
                 base::Unretained(this), base::Unretained(&open_loop)));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_cache_);

  cache_storage->CompleteAsyncOperationForTesting();
  open_loop.Run();
  EXPECT_TRUE(callback_cache_);
}

TEST_P(CacheStorageManagerTestP, GetOriginUsage) {
  EXPECT_EQ(0, GetOriginUsage(origin1_));
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_LT(0, GetOriginUsage(origin1_));
  EXPECT_EQ(0, GetOriginUsage(origin2_));
}

TEST_P(CacheStorageManagerTestP, GetAllOriginsUsage) {
  EXPECT_EQ(0ULL, GetAllOriginsUsage().size());
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  std::vector<CacheStorageUsageInfo> usage = GetAllOriginsUsage();
  EXPECT_EQ(1ULL, usage.size());
  const CacheStorageUsageInfo& info = usage[0];
  EXPECT_EQ(origin1_, info.origin);
  EXPECT_LT(0, info.total_size_bytes);
  if (MemoryOnly())
    EXPECT_TRUE(info.last_modified.is_null());
  else
    EXPECT_FALSE(info.last_modified.is_null());
}

TEST_F(CacheStorageManagerMemoryOnlyTest, MemoryBackedSize) {
  CacheStorage* cache_storage = CacheStorageForOrigin(origin1_);
  EXPECT_EQ(0, cache_storage->MemoryBackedSize());

  EXPECT_TRUE(Open(origin1_, "foo"));
  scoped_refptr<CacheStorageCache> foo_cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, "bar"));
  scoped_refptr<CacheStorageCache> bar_cache = callback_cache_;
  EXPECT_EQ(0, cache_storage->MemoryBackedSize());

  EXPECT_TRUE(CachePut(foo_cache, GURL("http://example.com/foo")));
  EXPECT_LT(0, cache_storage->MemoryBackedSize());
  int64_t foo_size = cache_storage->MemoryBackedSize();

  EXPECT_TRUE(CachePut(bar_cache, GURL("http://example.com/foo")));
  EXPECT_EQ(foo_size * 2, cache_storage->MemoryBackedSize());
}

TEST_F(CacheStorageManagerTest, MemoryBackedSizePersistent) {
  CacheStorage* cache_storage = CacheStorageForOrigin(origin1_);
  EXPECT_EQ(0, cache_storage->MemoryBackedSize());
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_EQ(0, cache_storage->MemoryBackedSize());
}

TEST_F(CacheStorageManagerTest, DeleteUnreferencedCacheDirectories) {
  // Create a referenced cache.
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));

  // Create an unreferenced directory next to the referenced one.
  base::FilePath origin_path = CacheStorageManager::ConstructOriginPath(
      cache_manager_->root_path(), origin1_);
  base::FilePath unreferenced_path = origin_path.AppendASCII("bar");
  EXPECT_TRUE(CreateDirectory(unreferenced_path));
  EXPECT_TRUE(base::DirectoryExists(unreferenced_path));

  // Create a new StorageManager so that the next time the cache is opened
  // the unreferenced directory can be deleted.
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());

  // Verify that the referenced cache still works.
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CacheMatch(callback_cache_, GURL("http://example.com/foo")));

  // Verify that the unreferenced cache is gone.
  EXPECT_FALSE(base::DirectoryExists(unreferenced_path));
}

class CacheStorageMigrationTest : public CacheStorageManagerTest {
 protected:
  CacheStorageMigrationTest() : cache1_("foo"), cache2_("bar") {}

  void SetUp() override {
    CacheStorageManagerTest::SetUp();

    // Populate a cache, then move it to the "legacy" location
    // so that tests can verify the results of migration.
    legacy_path_ = CacheStorageManager::ConstructLegacyOriginPath(
        cache_manager_->root_path(), origin1_);
    new_path_ = CacheStorageManager::ConstructOriginPath(
        cache_manager_->root_path(), origin1_);

    ASSERT_FALSE(base::DirectoryExists(legacy_path_));
    ASSERT_FALSE(base::DirectoryExists(new_path_));
    ASSERT_TRUE(Open(origin1_, cache1_));
    ASSERT_TRUE(Open(origin1_, cache2_));
    callback_cache_ = nullptr;
    ASSERT_FALSE(base::DirectoryExists(legacy_path_));
    ASSERT_TRUE(base::DirectoryExists(new_path_));

    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    cache_manager_ = CacheStorageManager::Create(cache_manager_.get());

    ASSERT_TRUE(base::Move(new_path_, legacy_path_));
    ASSERT_TRUE(base::DirectoryExists(legacy_path_));
    ASSERT_FALSE(base::DirectoryExists(new_path_));
  }

  base::FilePath legacy_path_;
  base::FilePath new_path_;

  const std::string cache1_;
  const std::string cache2_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageMigrationTest);
};

TEST_F(CacheStorageMigrationTest, OpenCache) {
  EXPECT_TRUE(Open(origin1_, cache1_));
  EXPECT_FALSE(base::DirectoryExists(legacy_path_));
  EXPECT_TRUE(base::DirectoryExists(new_path_));

  EXPECT_TRUE(Keys(origin1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back(cache1_);
  expected_keys.push_back(cache2_);
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_F(CacheStorageMigrationTest, DeleteCache) {
  EXPECT_TRUE(Delete(origin1_, cache1_));
  EXPECT_FALSE(base::DirectoryExists(legacy_path_));
  EXPECT_TRUE(base::DirectoryExists(new_path_));

  EXPECT_TRUE(Keys(origin1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back(cache2_);
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_F(CacheStorageMigrationTest, GetOriginUsage) {
  EXPECT_GT(GetOriginUsage(origin1_), 0);
  EXPECT_FALSE(base::DirectoryExists(legacy_path_));
  EXPECT_TRUE(base::DirectoryExists(new_path_));
}

TEST_F(CacheStorageMigrationTest, MoveFailure) {
  // Revert the migration.
  ASSERT_TRUE(base::Move(legacy_path_, new_path_));
  ASSERT_FALSE(base::DirectoryExists(legacy_path_));
  ASSERT_TRUE(base::DirectoryExists(new_path_));

  // Make a dummy legacy directory.
  ASSERT_TRUE(base::CreateDirectory(legacy_path_));

  // Ensure that migration doesn't stomp existing new directory,
  // but does clean up old directory.
  EXPECT_TRUE(Open(origin1_, cache1_));
  EXPECT_FALSE(base::DirectoryExists(legacy_path_));
  EXPECT_TRUE(base::DirectoryExists(new_path_));

  EXPECT_TRUE(Keys(origin1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back(cache1_);
  expected_keys.push_back(cache2_);
  EXPECT_EQ(expected_keys, callback_strings_);
}

// Tests that legacy caches created via a hash of the cache name instead of a
// random name are migrated and continue to function in a new world of random
// cache names.
class MigratedLegacyCacheDirectoryNameTest : public CacheStorageManagerTest {
 protected:
  MigratedLegacyCacheDirectoryNameTest()
      : legacy_cache_name_("foo"), stored_url_("http://example.com/foo") {}

  void SetUp() override {
    CacheStorageManagerTest::SetUp();

    // Create a cache that is stored on disk with the legacy naming scheme (hash
    // of the name) and without a directory name in the index.
    base::FilePath origin_path = CacheStorageManager::ConstructOriginPath(
        cache_manager_->root_path(), origin1_);

    // Populate a legacy cache.
    ASSERT_TRUE(Open(origin1_, legacy_cache_name_));
    EXPECT_TRUE(CachePut(callback_cache_, stored_url_));
    base::FilePath new_path = callback_cache_->path();

    // Close the cache's backend so that the files can be moved.
    callback_cache_->Close(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();

    // Legacy index files didn't have the cache directory, so remove it from the
    // index.
    base::FilePath index_path = origin_path.AppendASCII("index.txt");
    std::string index_contents;
    base::ReadFileToString(index_path, &index_contents);
    CacheStorageIndex index;
    ASSERT_TRUE(index.ParseFromString(index_contents));
    ASSERT_EQ(1, index.cache_size());
    index.mutable_cache(0)->clear_cache_dir();
    ASSERT_TRUE(index.SerializeToString(&index_contents));
    index.ParseFromString(index_contents);
    ASSERT_EQ(index_contents.size(),
              (uint32_t)base::WriteFile(index_path, index_contents.c_str(),
                                        index_contents.size()));

    // Move the cache to the legacy location.
    legacy_path_ = origin_path.AppendASCII(HexedHash(legacy_cache_name_));
    ASSERT_FALSE(base::DirectoryExists(legacy_path_));
    ASSERT_TRUE(base::Move(new_path, legacy_path_));

    // Create a new manager to reread the index file and force migration.
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
  }

  static std::string HexedHash(const std::string& value) {
    std::string value_hash = base::SHA1HashString(value);
    std::string valued_hexed_hash = base::ToLowerASCII(
        base::HexEncode(value_hash.c_str(), value_hash.length()));
    return valued_hexed_hash;
  }

  base::FilePath legacy_path_;
  const std::string legacy_cache_name_;
  const GURL stored_url_;

  DISALLOW_COPY_AND_ASSIGN(MigratedLegacyCacheDirectoryNameTest);
};

TEST_F(MigratedLegacyCacheDirectoryNameTest, LegacyCacheMigrated) {
  EXPECT_TRUE(Open(origin1_, legacy_cache_name_));

  // Verify that the cache migrated away from the legacy_path_ directory.
  ASSERT_FALSE(base::DirectoryExists(legacy_path_));

  // Verify that the existing entry still works.
  EXPECT_TRUE(CacheMatch(callback_cache_, stored_url_));

  // Verify that adding new entries works.
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo2")));
  EXPECT_TRUE(CacheMatch(callback_cache_, GURL("http://example.com/foo2")));
}

TEST_F(MigratedLegacyCacheDirectoryNameTest,
       RandomDirectoryCacheSideBySideWithLegacy) {
  EXPECT_TRUE(Open(origin1_, legacy_cache_name_));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(CachePut(callback_cache_, stored_url_));
  EXPECT_TRUE(CacheMatch(callback_cache_, stored_url_));
}

TEST_F(MigratedLegacyCacheDirectoryNameTest, DeleteLegacyCacheAndRecreateNew) {
  EXPECT_TRUE(Delete(origin1_, legacy_cache_name_));
  EXPECT_TRUE(Open(origin1_, legacy_cache_name_));
  EXPECT_FALSE(CacheMatch(callback_cache_, stored_url_));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo2")));
  EXPECT_TRUE(CacheMatch(callback_cache_, GURL("http://example.com/foo2")));
}

class CacheStorageQuotaClientTest : public CacheStorageManagerTest {
 protected:
  CacheStorageQuotaClientTest() {}

  void SetUp() override {
    CacheStorageManagerTest::SetUp();
    quota_client_.reset(
        new CacheStorageQuotaClient(cache_manager_->AsWeakPtr()));
  }

  void QuotaUsageCallback(base::RunLoop* run_loop, int64_t usage) {
    callback_quota_usage_ = usage;
    run_loop->Quit();
  }

  void OriginsCallback(base::RunLoop* run_loop, const std::set<GURL>& origins) {
    callback_origins_ = origins;
    run_loop->Quit();
  }

  void DeleteOriginCallback(base::RunLoop* run_loop,
                            storage::QuotaStatusCode status) {
    callback_status_ = status;
    run_loop->Quit();
  }

  int64_t QuotaGetOriginUsage(const GURL& origin) {
    base::RunLoop loop;
    quota_client_->GetOriginUsage(
        origin, storage::kStorageTypeTemporary,
        base::Bind(&CacheStorageQuotaClientTest::QuotaUsageCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_quota_usage_;
  }

  size_t QuotaGetOriginsForType() {
    base::RunLoop loop;
    quota_client_->GetOriginsForType(
        storage::kStorageTypeTemporary,
        base::Bind(&CacheStorageQuotaClientTest::OriginsCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_origins_.size();
  }

  size_t QuotaGetOriginsForHost(const std::string& host) {
    base::RunLoop loop;
    quota_client_->GetOriginsForHost(
        storage::kStorageTypeTemporary, host,
        base::Bind(&CacheStorageQuotaClientTest::OriginsCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_origins_.size();
  }

  bool QuotaDeleteOriginData(const GURL& origin) {
    base::RunLoop loop;
    quota_client_->DeleteOriginData(
        origin, storage::kStorageTypeTemporary,
        base::Bind(&CacheStorageQuotaClientTest::DeleteOriginCallback,
                   base::Unretained(this), base::Unretained(&loop)));
    loop.Run();
    return callback_status_ == storage::kQuotaStatusOk;
  }

  bool QuotaDoesSupport(storage::StorageType type) {
    return quota_client_->DoesSupport(type);
  }

  scoped_ptr<CacheStorageQuotaClient> quota_client_;

  storage::QuotaStatusCode callback_status_;
  int64_t callback_quota_usage_ = 0;
  std::set<GURL> callback_origins_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageQuotaClientTest);
};

class CacheStorageQuotaClientDiskOnlyTest : public CacheStorageQuotaClientTest {
 public:
  bool MemoryOnly() override { return false; }
};

class CacheStorageQuotaClientTestP : public CacheStorageQuotaClientTest,
                                     public testing::WithParamInterface<bool> {
  bool MemoryOnly() override { return !GetParam(); }
};

TEST_P(CacheStorageQuotaClientTestP, QuotaID) {
  EXPECT_EQ(storage::QuotaClient::kServiceWorkerCache, quota_client_->id());
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginUsage) {
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_LT(0, QuotaGetOriginUsage(origin1_));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginsForType) {
  EXPECT_EQ(0u, QuotaGetOriginsForType());
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "foo"));
  EXPECT_EQ(2u, QuotaGetOriginsForType());
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginsForHost) {
  EXPECT_EQ(0u, QuotaGetOriginsForHost("example.com"));
  EXPECT_TRUE(Open(GURL("http://example.com:8080"), "foo"));
  EXPECT_TRUE(Open(GURL("http://example.com:9000"), "foo"));
  EXPECT_TRUE(Open(GURL("ftp://example.com"), "foo"));
  EXPECT_TRUE(Open(GURL("http://example2.com"), "foo"));
  EXPECT_EQ(3u, QuotaGetOriginsForHost("example.com"));
  EXPECT_EQ(1u, QuotaGetOriginsForHost("example2.com"));
  EXPECT_TRUE(callback_origins_.find(GURL("http://example2.com")) !=
              callback_origins_.end());
  EXPECT_EQ(0u, QuotaGetOriginsForHost("unknown.com"));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDeleteOriginData) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  // Call put to test that initialized caches are properly deleted too.
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "baz"));

  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));

  EXPECT_FALSE(Has(origin1_, "foo"));
  EXPECT_FALSE(Has(origin1_, "bar"));
  EXPECT_TRUE(Has(origin2_, "baz"));
  EXPECT_TRUE(Open(origin1_, "foo"));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDeleteEmptyOrigin) {
  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));
}

TEST_F(CacheStorageQuotaClientDiskOnlyTest, QuotaDeleteUnloadedOriginData) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  // Call put to test that initialized caches are properly deleted too.
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));

  // Close the cache backend so that it writes out its index to disk.
  base::RunLoop run_loop;
  callback_cache_->Close(run_loop.QuitClosure());
  run_loop.Run();

  // Create a new CacheStorageManager that hasn't yet loaded the origin.
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
  quota_client_.reset(new CacheStorageQuotaClient(cache_manager_->AsWeakPtr()));

  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDoesSupport) {
  EXPECT_TRUE(QuotaDoesSupport(storage::kStorageTypeTemporary));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypePersistent));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypeSyncable));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypeQuotaNotManaged));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypeUnknown));
}

INSTANTIATE_TEST_CASE_P(CacheStorageManagerTests,
                        CacheStorageManagerTestP,
                        ::testing::Values(false, true));

INSTANTIATE_TEST_CASE_P(CacheStorageQuotaClientTests,
                        CacheStorageQuotaClientTestP,
                        ::testing::Values(false, true));

}  // namespace content
