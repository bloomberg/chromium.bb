// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/id_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/bad_message.h"
#include "content/common/cache_storage/cache_storage_types.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"
#include "third_party/WebKit/public/mojom/quota/quota_types.mojom.h"
#include "third_party/WebKit/public/platform/modules/cache_storage/cache_storage.mojom.h"
#include "url/origin.h"

namespace crypto {
class SymmetricKey;
}

namespace net {
class URLRequestContextGetter;
}

namespace storage {
class BlobDataHandle;
class BlobStorageContext;
class QuotaManagerProxy;
}

namespace content {
class CacheStorage;
class CacheStorageBlobToDiskCache;
class CacheStorageCacheHandle;
class CacheStorageCacheObserver;
class CacheStorageScheduler;
class TestCacheStorageCache;

namespace proto {
class CacheMetadata;
class CacheResponse;
}

namespace cache_storage_cache_unittest {
class TestCacheStorageCache;
class CacheStorageCacheTest;
}  // namespace cache_storage_cache_unittest

// Represents a ServiceWorker Cache as seen in
// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/ The
// asynchronous methods are executed serially. Callbacks to the public functions
// will be called so long as the cache object lives.
class CONTENT_EXPORT CacheStorageCache {
 public:
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError)>;
  using BadMessageCallback =
      base::OnceCallback<void(bad_message::BadMessageReason)>;
  using ResponseCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              std::unique_ptr<ServiceWorkerResponse>,
                              std::unique_ptr<storage::BlobDataHandle>)>;
  using BlobDataHandles = std::vector<std::unique_ptr<storage::BlobDataHandle>>;
  using ResponsesCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              std::vector<ServiceWorkerResponse>,
                              std::unique_ptr<BlobDataHandles>)>;
  using Requests = std::vector<ServiceWorkerFetchRequest>;
  using RequestsCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              std::unique_ptr<Requests>)>;
  using SizeCallback = base::OnceCallback<void(int64_t)>;
  using SizePaddingCallback = base::OnceCallback<void(int64_t, int64_t)>;

  enum EntryIndex { INDEX_HEADERS = 0, INDEX_RESPONSE_BODY, INDEX_SIDE_DATA };

  static std::unique_ptr<CacheStorageCache> CreateMemoryCache(
      const url::Origin& origin,
      const std::string& cache_name,
      CacheStorage* cache_storage,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      std::unique_ptr<crypto::SymmetricKey> cache_padding_key);
  static std::unique_ptr<CacheStorageCache> CreatePersistentCache(
      const url::Origin& origin,
      const std::string& cache_name,
      CacheStorage* cache_storage,
      const base::FilePath& path,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      int64_t cache_size,
      int64_t cache_padding,
      std::unique_ptr<crypto::SymmetricKey> cache_padding_key);
  static int64_t CalculateResponsePadding(
      const ServiceWorkerResponse& response,
      const crypto::SymmetricKey* padding_key,
      int side_data_size);
  static int32_t GetResponsePaddingVersion();

  // Returns ERROR_TYPE_NOT_FOUND if not found.
  void Match(std::unique_ptr<ServiceWorkerFetchRequest> request,
             const CacheStorageCacheQueryParams& match_params,
             ResponseCallback callback);

  // Returns blink::mojom::CacheStorageError::kSuccess and matched
  // responses in this cache. If there are no responses, returns
  // blink::mojom::CacheStorageError::kSuccess and an empty vector.
  void MatchAll(std::unique_ptr<ServiceWorkerFetchRequest> request,
                const CacheStorageCacheQueryParams& match_params,
                ResponsesCallback callback);

  // Writes the side data (ex: V8 code cache) for the specified cache entry.
  // If it doesn't exist, or the |expected_response_time| differs from the
  // entry's, blink::mojom::CacheStorageError::kErrorNotFound is returned.
  // Note: This "side data" is same meaning as "metadata" in HTTPCache. We use
  // "metadata" in cache_storage.proto for the pair of headers of a request and
  // a response. To avoid the confusion we use "side data" here.
  void WriteSideData(CacheStorageCache::ErrorCallback callback,
                     const GURL& url,
                     base::Time expected_response_time,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len);

  // Runs given batch operations. This corresponds to the Batch Cache Operations
  // algorithm in the spec.
  //
  // |operations| cannot mix PUT and DELETE operations and cannot contain
  // multiple DELETE operations.
  //
  // In the case of the PUT operation, puts request and response objects in the
  // cache and returns OK when all operations are successfully completed.
  // In the case of the DELETE operation, returns ERROR_NOT_FOUND if a specified
  // entry is not found. Otherwise deletes it and returns OK.
  //
  // TODO(nhiroki): This function should run all operations atomically.
  // http://crbug.com/486637
  void BatchOperation(const std::vector<CacheStorageBatchOperation>& operations,
                      ErrorCallback callback,
                      BadMessageCallback bad_message_callback);
  void BatchDidGetUsageAndQuota(
      const std::vector<CacheStorageBatchOperation>& operations,
      ErrorCallback callback,
      BadMessageCallback bad_message_callback,
      uint64_t space_required,
      uint64_t side_data_size,
      blink::mojom::QuotaStatusCode status_code,
      int64_t usage,
      int64_t quota);
  // Callback passed to operations. If |error| is a real error, invokes
  // |error_callback|. Always invokes |completion_closure| to signal
  // completion.
  void BatchDidOneOperation(base::OnceClosure completion_closure,
                            ErrorCallback error_callback,
                            blink::mojom::CacheStorageError error);
  // Callback invoked once all BatchDidOneOperation() calls have run.
  // Invokes |error_callback|.
  void BatchDidAllOperations(ErrorCallback error_callback);

  // Returns blink::mojom::CacheStorageError::kSuccess and a vector of
  // requests if there are no errors.
  void Keys(std::unique_ptr<ServiceWorkerFetchRequest> request,
            const CacheStorageCacheQueryParams& options,
            RequestsCallback callback);

  // Closes the backend. Future operations that require the backend
  // will exit early. Close should only be called once per CacheStorageCache.
  void Close(base::OnceClosure callback);

  // The size of the cache's contents.
  void Size(SizeCallback callback);

  // Gets the cache's size, closes the backend, and then runs |callback| with
  // the cache's size.
  void GetSizeThenClose(SizeCallback callback);

  // Async operations in progress will cancel and not run their callbacks.
  virtual ~CacheStorageCache();

  base::FilePath path() const { return path_; }

  std::string cache_name() const { return cache_name_; }

  int64_t cache_size() const { return cache_size_; }

  int64_t cache_padding() const { return cache_padding_; }

  const crypto::SymmetricKey* cache_padding_key() const {
    return cache_padding_key_.get();
  }

  // Return the total cache size (actual size + padding). If either is unknown
  // then CacheStorage::kSizeUnknown is returned.
  int64_t PaddedCacheSize() const;

  // Set the one observer that will be notified of changes to this cache.
  // Note: Either the observer must have a lifetime longer than this instance
  // or call SetObserver(nullptr) to stop receiving notification of changes.
  void SetObserver(CacheStorageCacheObserver* observer);

  base::WeakPtr<CacheStorageCache> AsWeakPtr();

 private:
  // QueryCache types:
  enum QueryCacheFlags {
    QUERY_CACHE_REQUESTS = 0x1,
    QUERY_CACHE_RESPONSES_WITH_BODIES = 0x2,
    QUERY_CACHE_RESPONSES_NO_BODIES = 0x4,
    QUERY_CACHE_ENTRIES = 0x8,
  };

  // The backend progresses from uninitialized, to open, to closed, and cannot
  // reverse direction.  The open step may be skipped.
  enum BackendState {
    BACKEND_UNINITIALIZED,  // No backend, create backend on first operation.
    BACKEND_OPEN,           // Backend can be used.
    BACKEND_CLOSED          // Backend cannot be used.  All ops should fail.
  };

  friend class base::RefCounted<CacheStorageCache>;
  friend class cache_storage_cache_unittest::TestCacheStorageCache;
  friend class cache_storage_cache_unittest::CacheStorageCacheTest;

  struct PutContext;
  struct QueryCacheContext;
  struct QueryCacheResult;

  using QueryTypes = int32_t;
  using QueryCacheResults = std::vector<QueryCacheResult>;
  using QueryCacheCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              std::unique_ptr<QueryCacheResults>)>;
  using Entries = std::vector<disk_cache::Entry*>;
  using ScopedBackendPtr = std::unique_ptr<disk_cache::Backend>;
  using BlobToDiskCacheIDMap =
      base::IDMap<std::unique_ptr<CacheStorageBlobToDiskCache>>;

  CacheStorageCache(
      const url::Origin& origin,
      const std::string& cache_name,
      const base::FilePath& path,
      CacheStorage* cache_storage,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      int64_t cache_size,
      int64_t cache_padding,
      std::unique_ptr<crypto::SymmetricKey> cache_padding_key);

  // Runs |callback| with matching requests/response data. The data provided
  // in the QueryCacheResults depends on the |query_type|. If |query_type| is
  // CACHE_ENTRIES then only out_entries is valid. If |query_type| is REQUESTS
  // then only out_requests is valid. If |query_type| is
  // REQUESTS_AND_RESPONSES then only out_requests, out_responses, and
  // out_blob_data_handles are valid.
  void QueryCache(std::unique_ptr<ServiceWorkerFetchRequest> request,
                  const CacheStorageCacheQueryParams& options,
                  QueryTypes query_types,
                  QueryCacheCallback callback);
  void QueryCacheDidOpenFastPath(
      std::unique_ptr<QueryCacheContext> query_cache_context,
      int rv);
  void QueryCacheOpenNextEntry(
      std::unique_ptr<QueryCacheContext> query_cache_context);
  void QueryCacheFilterEntry(
      std::unique_ptr<QueryCacheContext> query_cache_context,
      int rv);
  void QueryCacheDidReadMetadata(
      std::unique_ptr<QueryCacheContext> query_cache_context,
      disk_cache::ScopedEntryPtr entry,
      std::unique_ptr<proto::CacheMetadata> metadata);
  static bool QueryCacheResultCompare(const QueryCacheResult& lhs,
                                      const QueryCacheResult& rhs);

  // Match callbacks
  void MatchImpl(std::unique_ptr<ServiceWorkerFetchRequest> request,
                 const CacheStorageCacheQueryParams& match_params,
                 ResponseCallback callback);
  void MatchDidMatchAll(ResponseCallback callback,
                        blink::mojom::CacheStorageError match_all_error,
                        std::vector<ServiceWorkerResponse> match_all_responses,
                        std::unique_ptr<BlobDataHandles> match_all_handles);

  // MatchAll callbacks
  void MatchAllImpl(std::unique_ptr<ServiceWorkerFetchRequest> request,
                    const CacheStorageCacheQueryParams& options,
                    ResponsesCallback callback);
  void MatchAllDidQueryCache(
      ResponsesCallback callback,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  // WriteSideData callbacks
  void WriteSideDataDidGetQuota(ErrorCallback callback,
                                const GURL& url,
                                base::Time expected_response_time,
                                scoped_refptr<net::IOBuffer> buffer,
                                int buf_len,
                                blink::mojom::QuotaStatusCode status_code,
                                int64_t usage,
                                int64_t quota);

  void WriteSideDataImpl(ErrorCallback callback,
                         const GURL& url,
                         base::Time expected_response_time,
                         scoped_refptr<net::IOBuffer> buffer,
                         int buf_len);
  void WriteSideDataDidGetUsageAndQuota(
      ErrorCallback callback,
      const GURL& url,
      base::Time expected_response_time,
      scoped_refptr<net::IOBuffer> buffer,
      int buf_len,
      blink::mojom::QuotaStatusCode status_code,
      int64_t usage,
      int64_t quota);
  void WriteSideDataDidOpenEntry(ErrorCallback callback,
                                 base::Time expected_response_time,
                                 scoped_refptr<net::IOBuffer> buffer,
                                 int buf_len,
                                 std::unique_ptr<disk_cache::Entry*> entry_ptr,
                                 int rv);
  void WriteSideDataDidReadMetaData(
      ErrorCallback callback,
      base::Time expected_response_time,
      scoped_refptr<net::IOBuffer> buffer,
      int buf_len,
      disk_cache::ScopedEntryPtr entry,
      std::unique_ptr<proto::CacheMetadata> headers);
  void WriteSideDataDidWrite(
      ErrorCallback callback,
      disk_cache::ScopedEntryPtr entry,
      int expected_bytes,
      std::unique_ptr<content::proto::CacheResponse> response,
      int side_data_size_before_write,
      int rv);

  // Puts the request and response object in the cache. The response body (if
  // present) is stored in the cache, but not the request body. Returns OK on
  // success.
  void Put(const CacheStorageBatchOperation& operation, ErrorCallback callback);
  void PutImpl(std::unique_ptr<PutContext> put_context);
  void PutDidDeleteEntry(std::unique_ptr<PutContext> put_context,
                         blink::mojom::CacheStorageError error);
  void PutDidGetUsageAndQuota(std::unique_ptr<PutContext> put_context,
                              blink::mojom::QuotaStatusCode status_code,
                              int64_t usage,
                              int64_t quota);
  void PutDidCreateEntry(std::unique_ptr<disk_cache::Entry*> entry_ptr,
                         std::unique_ptr<PutContext> put_context,
                         int rv);
  void PutDidWriteHeaders(std::unique_ptr<PutContext> put_context,
                          int expected_bytes,
                          int rv);
  void PutWriteBlobToCache(std::unique_ptr<PutContext> put_context,
                           int disk_cache_body_index);
  void PutDidWriteBlobToCache(std::unique_ptr<PutContext> put_context,
                              BlobToDiskCacheIDMap::KeyType blob_to_cache_key,
                              disk_cache::ScopedEntryPtr entry,
                              bool success);

  // Asynchronously calculates the current cache size, notifies the quota
  // manager of any change from the last report, and sets cache_size_ to the new
  // size.
  void UpdateCacheSize(base::OnceClosure callback);
  void UpdateCacheSizeGotSize(CacheStorageCacheHandle,
                              base::OnceClosure callback,
                              int current_cache_size);

  // Returns ERROR_NOT_FOUND if not found. Otherwise deletes and returns OK.
  void Delete(const CacheStorageBatchOperation& operation,
              ErrorCallback callback);
  void DeleteImpl(std::unique_ptr<ServiceWorkerFetchRequest> request,
                  const CacheStorageCacheQueryParams& match_params,
                  ErrorCallback callback);
  void DeleteDidQueryCache(
      ErrorCallback callback,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  // Keys callbacks.
  void KeysImpl(std::unique_ptr<ServiceWorkerFetchRequest> request,
                const CacheStorageCacheQueryParams& options,
                RequestsCallback callback);
  void KeysDidQueryCache(
      RequestsCallback callback,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  void CloseImpl(base::OnceClosure callback);

  void SizeImpl(SizeCallback callback);

  void GetSizeThenCloseDidGetSize(SizeCallback callback, int64_t cache_size);

  // Loads the backend and calls the callback with the result (true for
  // success). The callback will always be called. Virtual for tests.
  virtual void CreateBackend(ErrorCallback callback);
  void CreateBackendDidCreate(ErrorCallback callback,
                              std::unique_ptr<ScopedBackendPtr> backend_ptr,
                              int rv);

  // Calculate the size and padding of the cache.
  void CalculateCacheSizePadding(SizePaddingCallback callback);
  void CalculateCacheSizePaddingGotSize(SizePaddingCallback callback,
                                        int cache_size);
  void PaddingDidQueryCache(
      SizePaddingCallback callback,
      int cache_size,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  // Calculate the size (but not padding) of the cache.
  void CalculateCacheSize(const net::CompletionCallback& callback);

  void InitBackend();
  void InitDidCreateBackend(base::OnceClosure callback,
                            blink::mojom::CacheStorageError cache_create_error);
  void InitGotCacheSize(base::OnceClosure callback,
                        blink::mojom::CacheStorageError cache_create_error,
                        int cache_size);
  void InitGotCacheSizeAndPadding(
      base::OnceClosure callback,
      blink::mojom::CacheStorageError cache_create_error,
      int64_t cache_size,
      int64_t cache_padding);
  void DeleteBackendCompletedIO();

  std::unique_ptr<storage::BlobDataHandle> PopulateResponseBody(
      disk_cache::ScopedEntryPtr entry,
      ServiceWorkerResponse* response);

  // Virtual for testing.
  virtual CacheStorageCacheHandle CreateCacheHandle();

  // Be sure to check |backend_state_| before use.
  std::unique_ptr<disk_cache::Backend> backend_;

  url::Origin origin_;
  const std::string cache_name_;
  base::FilePath path_;

  // Raw pointer is safe because CacheStorage owns this object.
  CacheStorage* cache_storage_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  BackendState backend_state_ = BACKEND_UNINITIALIZED;
  std::unique_ptr<CacheStorageScheduler> scheduler_;
  bool initializing_ = false;
  // The actual cache size (not including padding).
  int64_t cache_size_;
  int64_t cache_padding_ = 0;
  std::unique_ptr<crypto::SymmetricKey> cache_padding_key_;
  int64_t last_reported_size_ = 0;
  size_t max_query_size_bytes_;
  CacheStorageCacheObserver* cache_observer_;

  // Owns the elements of the list
  BlobToDiskCacheIDMap active_blob_to_disk_cache_writers_;

  // Whether or not to store data in disk or memory.
  bool memory_only_;

  // Active while waiting for the backend to finish its closing up, and contains
  // the callback passed to CloseImpl.
  base::OnceClosure post_backend_closed_callback_;

  base::WeakPtrFactory<CacheStorageCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_
