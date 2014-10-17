// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_storage_manager.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/service_worker/service_worker_cache_quota_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerCacheStorageManagerTest : public testing::Test {
 protected:
  ServiceWorkerCacheStorageManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        callback_bool_(false),
        callback_error_(
            ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR),
        callback_cache_error_(ServiceWorkerCache::ErrorTypeOK),
        origin1_("http://example1.com"),
        origin2_("http://example2.com") {}

  virtual void SetUp() override {
    ChromeBlobStorageContext* blob_storage_context(
        ChromeBlobStorageContext::GetFor(&browser_context_));
    // Wait for ChromeBlobStorageContext to finish initializing.
    base::RunLoop().RunUntilIdle();

    net::URLRequestContext* url_request_context =
        browser_context_.GetRequestContext()->GetURLRequestContext();
    if (MemoryOnly()) {
      cache_manager_ = ServiceWorkerCacheStorageManager::Create(
          base::FilePath(), base::MessageLoopProxy::current(), nullptr);
    } else {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      cache_manager_ = ServiceWorkerCacheStorageManager::Create(
          temp_dir_.path(), base::MessageLoopProxy::current(), nullptr);
    }

    cache_manager_->SetBlobParametersForCache(
        url_request_context, blob_storage_context->context()->AsWeakPtr());
  }

  virtual void TearDown() override {
    base::RunLoop().RunUntilIdle();
  }

  virtual bool MemoryOnly() { return false; }

  void BoolAndErrorCallback(
      base::RunLoop* run_loop,
      bool value,
      ServiceWorkerCacheStorage::CacheStorageError error) {
    callback_bool_ = value;
    callback_error_ = error;
    run_loop->Quit();
  }

  void CacheAndErrorCallback(
      base::RunLoop* run_loop,
      const scoped_refptr<ServiceWorkerCache>& cache,
      ServiceWorkerCacheStorage::CacheStorageError error) {
    callback_cache_ = cache;
    callback_error_ = error;
    run_loop->Quit();
  }

  void StringsAndErrorCallback(
      base::RunLoop* run_loop,
      const std::vector<std::string>& strings,
      ServiceWorkerCacheStorage::CacheStorageError error) {
    callback_strings_ = strings;
    callback_error_ = error;
    run_loop->Quit();
  }

  void CachePutCallback(base::RunLoop* run_loop,
                        ServiceWorkerCache::ErrorType error,
                        scoped_ptr<ServiceWorkerResponse> response,
                        scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
    callback_cache_error_ = error;
    run_loop->Quit();
  }

  void CacheMatchCallback(
      base::RunLoop* run_loop,
      ServiceWorkerCache::ErrorType error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
    callback_cache_error_ = error;
    callback_cache_response_ = response.Pass();
    // Deliberately drop the data handle as only the url is being tested.
    run_loop->Quit();
  }

  bool CreateCache(const GURL& origin, const std::string& cache_name) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->CreateCache(
        origin,
        cache_name,
        base::Bind(&ServiceWorkerCacheStorageManagerTest::CacheAndErrorCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_error_ !=
                 ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR;
    if (error)
      EXPECT_TRUE(!callback_cache_.get());
    else
      EXPECT_TRUE(callback_cache_.get());
    return !error;
  }

  bool Get(const GURL& origin, const std::string& cache_name) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->GetCache(
        origin,
        cache_name,
        base::Bind(&ServiceWorkerCacheStorageManagerTest::CacheAndErrorCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_error_ !=
                 ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR;
    if (error)
      EXPECT_TRUE(!callback_cache_.get());
    else
      EXPECT_TRUE(callback_cache_.get());
    return !error;
  }

  bool Open(const GURL& origin, const std::string& cache_name) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->OpenCache(
        origin,
        cache_name,
        base::Bind(&ServiceWorkerCacheStorageManagerTest::CacheAndErrorCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_error_ !=
                 ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR;
    if (error)
      EXPECT_TRUE(!callback_cache_.get());
    else
      EXPECT_TRUE(callback_cache_.get());
    return !error;
  }

  bool Has(const GURL& origin, const std::string& cache_name) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->HasCache(
        origin,
        cache_name,
        base::Bind(&ServiceWorkerCacheStorageManagerTest::BoolAndErrorCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    return callback_bool_;
  }

  bool Delete(const GURL& origin, const std::string& cache_name) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->DeleteCache(
        origin,
        cache_name,
        base::Bind(&ServiceWorkerCacheStorageManagerTest::BoolAndErrorCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    return callback_bool_;
  }

  bool Keys(const GURL& origin) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->EnumerateCaches(
        origin,
        base::Bind(
            &ServiceWorkerCacheStorageManagerTest::StringsAndErrorCallback,
            base::Unretained(this),
            base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_error_ !=
                 ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NO_ERROR;
    return !error;
  }

  bool CachePut(const scoped_refptr<ServiceWorkerCache>& cache,
                const GURL& url) {
    scoped_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest());
    scoped_ptr<ServiceWorkerResponse> response(new ServiceWorkerResponse());
    request->url = url;
    response->url = url;
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache->Put(
        request.Pass(),
        response.Pass(),
        base::Bind(&ServiceWorkerCacheStorageManagerTest::CachePutCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_cache_error_ != ServiceWorkerCache::ErrorTypeOK;
    return !error;
  }

  bool CacheMatch(const scoped_refptr<ServiceWorkerCache>& cache,
                  const GURL& url) {
    scoped_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest());
    request->url = url;
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache->Match(
        request.Pass(),
        base::Bind(&ServiceWorkerCacheStorageManagerTest::CacheMatchCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_cache_error_ != ServiceWorkerCache::ErrorTypeOK;
    return !error;
  }

 protected:
  TestBrowserContext browser_context_;
  TestBrowserThreadBundle browser_thread_bundle_;

  base::ScopedTempDir temp_dir_;
  scoped_ptr<ServiceWorkerCacheStorageManager> cache_manager_;

  scoped_refptr<ServiceWorkerCache> callback_cache_;
  int callback_bool_;
  ServiceWorkerCacheStorage::CacheStorageError callback_error_;
  ServiceWorkerCache::ErrorType callback_cache_error_;
  scoped_ptr<ServiceWorkerResponse> callback_cache_response_;
  std::vector<std::string> callback_strings_;

  const GURL origin1_;
  const GURL origin2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheStorageManagerTest);
};

class ServiceWorkerCacheStorageManagerMemoryOnlyTest
    : public ServiceWorkerCacheStorageManagerTest {
  virtual bool MemoryOnly() override { return true; }
};

class ServiceWorkerCacheStorageManagerTestP
    : public ServiceWorkerCacheStorageManagerTest,
      public testing::WithParamInterface<bool> {
  virtual bool MemoryOnly() override { return !GetParam(); }
};

TEST_F(ServiceWorkerCacheStorageManagerTest, TestsRunOnIOThread) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, CreateCache) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, CreateDuplicateCache) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_FALSE(CreateCache(origin1_, "foo"));
  EXPECT_EQ(ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_EXISTS,
            callback_error_);
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, CreateTwoCaches) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CreateCache(origin1_, "bar"));
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, CachePointersDiffer) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  scoped_refptr<ServiceWorkerCache> cache = callback_cache_;
  EXPECT_TRUE(CreateCache(origin1_, "bar"));
  EXPECT_TRUE(cache.get() != callback_cache_.get());
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, Create2CachesSameNameDiffSWs) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CreateCache(origin2_, "foo"));
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, GetCache) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  scoped_refptr<ServiceWorkerCache> cache = callback_cache_;
  EXPECT_TRUE(Get(origin1_, "foo"));
  EXPECT_TRUE(cache.get() == callback_cache_.get());
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, GetNonExistent) {
  EXPECT_FALSE(Get(origin1_, "foo"));
  EXPECT_EQ(ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NOT_FOUND,
            callback_error_);
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, OpenNewCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, OpenExistingCache) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  scoped_refptr<ServiceWorkerCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(cache.get() == callback_cache_.get());
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, HasCache) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(Has(origin1_, "foo"));
  EXPECT_TRUE(callback_bool_);
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, HasNonExistent) {
  EXPECT_FALSE(Has(origin1_, "foo"));
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, DeleteCache) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Get(origin1_, "foo"));
  EXPECT_EQ(ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NOT_FOUND,
            callback_error_);
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, DeleteTwice) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Delete(origin1_, "foo"));
  EXPECT_EQ(ServiceWorkerCacheStorage::CACHE_STORAGE_ERROR_NOT_FOUND,
            callback_error_);
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, EmptyKeys) {
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_TRUE(callback_strings_.empty());
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, SomeKeys) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CreateCache(origin1_, "bar"));
  EXPECT_TRUE(CreateCache(origin2_, "baz"));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("bar");
  EXPECT_TRUE(expected_keys == callback_strings_);
  EXPECT_TRUE(Keys(origin2_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("baz", callback_strings_[0].c_str());
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, DeletedKeysGone) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CreateCache(origin1_, "bar"));
  EXPECT_TRUE(CreateCache(origin2_, "baz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("foo", callback_strings_[0].c_str());
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, Chinese) {
  EXPECT_TRUE(CreateCache(origin1_, "你好"));
  scoped_refptr<ServiceWorkerCache> cache = callback_cache_;
  EXPECT_TRUE(Get(origin1_, "你好"));
  EXPECT_TRUE(cache.get() == callback_cache_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_TRUE("你好" == callback_strings_[0]);
}

TEST_F(ServiceWorkerCacheStorageManagerTest, EmptyKey) {
  EXPECT_TRUE(CreateCache(origin1_, ""));
  scoped_refptr<ServiceWorkerCache> cache = callback_cache_;
  EXPECT_TRUE(Get(origin1_, ""));
  EXPECT_EQ(cache.get(), callback_cache_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_TRUE("" == callback_strings_[0]);
  EXPECT_TRUE(Has(origin1_, ""));
  EXPECT_TRUE(Delete(origin1_, ""));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_F(ServiceWorkerCacheStorageManagerTest, DataPersists) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CreateCache(origin1_, "bar"));
  EXPECT_TRUE(CreateCache(origin1_, "baz"));
  EXPECT_TRUE(CreateCache(origin2_, "raz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  cache_manager_ =
      ServiceWorkerCacheStorageManager::Create(cache_manager_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("baz");
  EXPECT_TRUE(expected_keys == callback_strings_);
}

TEST_F(ServiceWorkerCacheStorageManagerMemoryOnlyTest, DataLostWhenMemoryOnly) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CreateCache(origin2_, "baz"));
  cache_manager_ =
      ServiceWorkerCacheStorageManager::Create(cache_manager_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_F(ServiceWorkerCacheStorageManagerTest, BadCacheName) {
  // Since the implementation writes cache names to disk, ensure that we don't
  // escape the directory.
  const std::string bad_name = "../../../../../../../../../../../../../../foo";
  EXPECT_TRUE(CreateCache(origin1_, bad_name));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ(bad_name.c_str(), callback_strings_[0].c_str());
}

TEST_F(ServiceWorkerCacheStorageManagerTest, BadOriginName) {
  // Since the implementation writes origin names to disk, ensure that we don't
  // escape the directory.
  GURL bad_origin("http://../../../../../../../../../../../../../../foo");
  EXPECT_TRUE(CreateCache(bad_origin, "foo"));
  EXPECT_TRUE(Keys(bad_origin));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("foo", callback_strings_[0].c_str());
}

// With a persistent cache if the client drops its reference to a
// ServiceWorkerCache
// it should be deleted.
TEST_F(ServiceWorkerCacheStorageManagerTest, DropReference) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  base::WeakPtr<ServiceWorkerCache> cache = callback_cache_->AsWeakPtr();
  callback_cache_ = NULL;
  EXPECT_TRUE(!cache);
}

// With a memory cache the cache can't be freed from memory until the client
// calls delete.
TEST_F(ServiceWorkerCacheStorageManagerMemoryOnlyTest,
       MemoryLosesReferenceOnlyAfterDelete) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  base::WeakPtr<ServiceWorkerCache> cache = callback_cache_->AsWeakPtr();
  callback_cache_ = NULL;
  EXPECT_TRUE(cache);
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(cache);
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, RecreateCacheOnDemand) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  callback_cache_ = NULL;
  EXPECT_TRUE(Get(origin1_, "foo"));
  EXPECT_TRUE(CacheMatch(callback_cache_, GURL("http://example.com/foo")));
}

TEST_P(ServiceWorkerCacheStorageManagerTestP, DeleteBeforeRelease) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_TRUE(callback_cache_->AsWeakPtr());
}

class ServiceWorkerCacheQuotaClientTest
    : public ServiceWorkerCacheStorageManagerTest {
 protected:
  ServiceWorkerCacheQuotaClientTest() {}

  virtual void SetUp() override {
    ServiceWorkerCacheStorageManagerTest::SetUp();
    quota_client_.reset(
        new ServiceWorkerCacheQuotaClient(cache_manager_->AsWeakPtr()));
  }

  void UsageCallback(base::RunLoop* run_loop, int64 usage) {
    callback_usage_ = usage;
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

  int64 QuotaGetOriginUsage(const GURL& origin) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    quota_client_->GetOriginUsage(
        origin,
        storage::kStorageTypeTemporary,
        base::Bind(&ServiceWorkerCacheQuotaClientTest::UsageCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();
    return callback_usage_;
  }

  size_t QuotaGetOriginsForType() {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    quota_client_->GetOriginsForType(
        storage::kStorageTypeTemporary,
        base::Bind(&ServiceWorkerCacheQuotaClientTest::OriginsCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();
    return callback_origins_.size();
  }

  size_t QuotaGetOriginsForHost(const std::string& host) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    quota_client_->GetOriginsForHost(
        storage::kStorageTypeTemporary,
        host,
        base::Bind(&ServiceWorkerCacheQuotaClientTest::OriginsCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();
    return callback_origins_.size();
  }

  bool QuotaDeleteOriginData(const GURL& origin) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    quota_client_->DeleteOriginData(
        origin,
        storage::kStorageTypeTemporary,
        base::Bind(&ServiceWorkerCacheQuotaClientTest::DeleteOriginCallback,
                   base::Unretained(this),
                   base::Unretained(loop.get())));
    loop->Run();
    return callback_status_ == storage::kQuotaStatusOk;
  }

  bool QuotaDoesSupport(storage::StorageType type) {
    return quota_client_->DoesSupport(type);
  }

  scoped_ptr<ServiceWorkerCacheQuotaClient> quota_client_;

  storage::QuotaStatusCode callback_status_;
  int64 callback_usage_;
  std::set<GURL> callback_origins_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheQuotaClientTest);
};

class ServiceWorkerCacheQuotaClientTestP
    : public ServiceWorkerCacheQuotaClientTest,
      public testing::WithParamInterface<bool> {
  virtual bool MemoryOnly() override { return !GetParam(); }
};

TEST_P(ServiceWorkerCacheQuotaClientTestP, QuotaID) {
  EXPECT_EQ(storage::QuotaClient::kServiceWorkerCache, quota_client_->id());
}

TEST_P(ServiceWorkerCacheQuotaClientTestP, QuotaGetOriginUsage) {
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  // TODO(jkarlin): Once usage is working properly update this value.
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
}

TEST_P(ServiceWorkerCacheQuotaClientTestP, QuotaGetOriginsForType) {
  EXPECT_EQ(0u, QuotaGetOriginsForType());
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CreateCache(origin1_, "bar"));
  EXPECT_TRUE(CreateCache(origin2_, "foo"));
  EXPECT_EQ(2u, QuotaGetOriginsForType());
}

TEST_P(ServiceWorkerCacheQuotaClientTestP, QuotaGetOriginsForHost) {
  EXPECT_EQ(0u, QuotaGetOriginsForHost("example.com"));
  EXPECT_TRUE(CreateCache(GURL("http://example.com:8080"), "foo"));
  EXPECT_TRUE(CreateCache(GURL("http://example.com:9000"), "foo"));
  EXPECT_TRUE(CreateCache(GURL("ftp://example.com"), "foo"));
  EXPECT_TRUE(CreateCache(GURL("http://example2.com"), "foo"));
  EXPECT_EQ(3u, QuotaGetOriginsForHost("example.com"));
  EXPECT_EQ(1u, QuotaGetOriginsForHost("example2.com"));
  EXPECT_TRUE(callback_origins_.find(GURL("http://example2.com")) !=
              callback_origins_.end());
  EXPECT_EQ(0u, QuotaGetOriginsForHost("unknown.com"));
}

TEST_P(ServiceWorkerCacheQuotaClientTestP, QuotaDeleteOriginData) {
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
  EXPECT_TRUE(CreateCache(origin1_, "bar"));
  EXPECT_TRUE(CreateCache(origin2_, "baz"));

  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));

  EXPECT_FALSE(Has(origin1_, "foo"));
  EXPECT_FALSE(Has(origin1_, "bar"));
  EXPECT_TRUE(Has(origin2_, "baz"));
  EXPECT_TRUE(CreateCache(origin1_, "foo"));
}

TEST_P(ServiceWorkerCacheQuotaClientTestP, QuotaDoesSupport) {
  EXPECT_TRUE(QuotaDoesSupport(storage::kStorageTypeTemporary));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypePersistent));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypeSyncable));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypeQuotaNotManaged));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypeUnknown));
}

INSTANTIATE_TEST_CASE_P(ServiceWorkerCacheStorageManagerTests,
                        ServiceWorkerCacheStorageManagerTestP,
                        ::testing::Values(false, true));

INSTANTIATE_TEST_CASE_P(ServiceWorkerCacheQuotaClientTests,
                        ServiceWorkerCacheQuotaClientTestP,
                        ::testing::Values(false, true));

}  // namespace content
