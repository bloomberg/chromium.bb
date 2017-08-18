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
#include "content/common/cache_storage/cache_storage_types.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/common/quota/quota_status_code.h"

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
}

// Represents a ServiceWorker Cache as seen in
// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/ The
// asynchronous methods are executed serially. Callbacks to the public functions
// will be called so long as the cache object lives.
class CONTENT_EXPORT CacheStorageCache {
 public:
  using ErrorCallback = base::OnceCallback<void(CacheStorageError)>;
  using ResponseCallback =
      base::OnceCallback<void(CacheStorageError,
                              std::unique_ptr<ServiceWorkerResponse>,
                              std::unique_ptr<storage::BlobDataHandle>)>;
  using Responses = std::vector<ServiceWorkerResponse>;
  using BlobDataHandles = std::vector<std::unique_ptr<storage::BlobDataHandle>>;
  using ResponsesCallback =
      base::OnceCallback<void(CacheStorageError,
                              std::unique_ptr<Responses>,
                              std::unique_ptr<BlobDataHandles>)>;
  using Requests = std::vector<ServiceWorkerFetchRequest>;
  using RequestsCallback =
      base::OnceCallback<void(CacheStorageError, std::unique_ptr<Requests>)>;
  using SizeCallback = base::OnceCallback<void(int64_t)>;

  enum EntryIndex { INDEX_HEADERS = 0, INDEX_RESPONSE_BODY, INDEX_SIDE_DATA };

  static std::unique_ptr<CacheStorageCache> CreateMemoryCache(
      const GURL& origin,
      const std::string& cache_name,
      CacheStorage* cache_storage,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context);
  static std::unique_ptr<CacheStorageCache> CreatePersistentCache(
      const GURL& origin,
      const std::string& cache_name,
      CacheStorage* cache_storage,
      const base::FilePath& path,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      int64_t cache_size);

  // Returns ERROR_TYPE_NOT_FOUND if not found.
  void Match(std::unique_ptr<ServiceWorkerFetchRequest> request,
             const CacheStorageCacheQueryParams& match_params,
             ResponseCallback callback);

  // Returns CACHE_STORAGE_OK and matched responses in this cache. If there are
  // no responses, returns CACHE_STORAGE_OK and an empty vector.
  void MatchAll(std::unique_ptr<ServiceWorkerFetchRequest> request,
                const CacheStorageCacheQueryParams& match_params,
                ResponsesCallback callback);

  // Writes the side data (ex: V8 code cache) for the specified cache entry.
  // If it doesn't exist, or the |expected_response_time| differs from the
  // entry's, CACHE_STORAGE_ERROR_NOT_FOUND is returned.
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
                      ErrorCallback callback);
  void BatchDidGetUsageAndQuota(
      const std::vector<CacheStorageBatchOperation>& operations,
      ErrorCallback callback,
      int64_t space_required,
      storage::QuotaStatusCode status_code,
      int64_t usage,
      int64_t quota);
  // Callback passed to operations. If |error| is a real error, invokes
  // |error_callback|. Always invokes |completion_closure| to signal
  // completion.
  void BatchDidOneOperation(base::OnceClosure completion_closure,
                            ErrorCallback error_callback,
                            CacheStorageError error);
  // Callback invoked once all BatchDidOneOperation() calls have run.
  // Invokes |error_callback|.
  void BatchDidAllOperations(ErrorCallback error_callback);

  // Returns CACHE_STORAGE_OK and a vector of requests if there are no errors.
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

  // Set the one observer that will be notified of changes to this cache.
  // Note: Either the observer must have a lifetime longer than this instance
  // or call SetObserver(nullptr) to stop receiving notification of changes.
  void SetObserver(CacheStorageCacheObserver* observer);

  base::WeakPtr<CacheStorageCache> AsWeakPtr();

 private:
  enum class QueryCacheType { REQUESTS, REQUESTS_AND_RESPONSES, CACHE_ENTRIES };

  // The backend progresses from uninitialized, to open, to closed, and cannot
  // reverse direction.  The open step may be skipped.
  enum BackendState {
    BACKEND_UNINITIALIZED,  // No backend, create backend on first operation.
    BACKEND_OPEN,           // Backend can be used.
    BACKEND_CLOSED          // Backend cannot be used.  All ops should fail.
  };

  friend class base::RefCounted<CacheStorageCache>;
  friend class TestCacheStorageCache;
  friend class CacheStorageCacheTest;

  struct PutContext;
  struct QueryCacheContext;
  struct QueryCacheResult;

  using QueryCacheResults = std::vector<QueryCacheResult>;
  using QueryCacheCallback =
      base::OnceCallback<void(CacheStorageError,
                              std::unique_ptr<QueryCacheResults>)>;
  using Entries = std::vector<disk_cache::Entry*>;
  using ScopedBackendPtr = std::unique_ptr<disk_cache::Backend>;
  using BlobToDiskCacheIDMap =
      base::IDMap<std::unique_ptr<CacheStorageBlobToDiskCache>>;

  CacheStorageCache(
      const GURL& origin,
      const std::string& cache_name,
      const base::FilePath& path,
      CacheStorage* cache_storage,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      int64_t cache_size);

  // Runs |callback| with matching requests/response data. The data provided
  // in the QueryCacheResults depends on the |query_type|. If |query_type| is
  // CACHE_ENTRIES then only out_entries is valid. If |query_type| is REQUESTS
  // then only out_requests is valid. If |query_type| is
  // REQUESTS_AND_RESPONSES then only out_requests, out_responses, and
  // out_blob_data_handles are valid.
  void QueryCache(std::unique_ptr<ServiceWorkerFetchRequest> request,
                  const CacheStorageCacheQueryParams& options,
                  QueryCacheType query_type,
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
                        CacheStorageError match_all_error,
                        std::unique_ptr<Responses> match_all_responses,
                        std::unique_ptr<BlobDataHandles> match_all_handles);

  // MatchAll callbacks
  void MatchAllImpl(std::unique_ptr<ServiceWorkerFetchRequest> request,
                    const CacheStorageCacheQueryParams& options,
                    ResponsesCallback callback);
  void MatchAllDidQueryCache(
      ResponsesCallback callback,
      CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  // WriteSideData callbacks
  void WriteSideDataDidGetQuota(ErrorCallback callback,
                                const GURL& url,
                                base::Time expected_response_time,
                                scoped_refptr<net::IOBuffer> buffer,
                                int buf_len,
                                storage::QuotaStatusCode status_code,
                                int64_t usage,
                                int64_t quota);

  void WriteSideDataImpl(ErrorCallback callback,
                         const GURL& url,
                         base::Time expected_response_time,
                         scoped_refptr<net::IOBuffer> buffer,
                         int buf_len);
  void WriteSideDataDidGetUsageAndQuota(ErrorCallback callback,
                                        const GURL& url,
                                        base::Time expected_response_time,
                                        scoped_refptr<net::IOBuffer> buffer,
                                        int buf_len,
                                        storage::QuotaStatusCode status_code,
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
  void WriteSideDataDidWrite(ErrorCallback callback,
                             disk_cache::ScopedEntryPtr entry,
                             int expected_bytes,
                             int rv);

  // Puts the request and response object in the cache. The response body (if
  // present) is stored in the cache, but not the request body. Returns OK on
  // success.
  void Put(const CacheStorageBatchOperation& operation, ErrorCallback callback);
  void PutImpl(std::unique_ptr<PutContext> put_context);
  void PutDidDoomEntry(std::unique_ptr<PutContext> put_context, int rv);
  void PutDidGetUsageAndQuota(std::unique_ptr<PutContext> put_context,
                              storage::QuotaStatusCode status_code,
                              int64_t usage,
                              int64_t quota);
  void PutDidCreateEntry(std::unique_ptr<disk_cache::Entry*> entry_ptr,
                         std::unique_ptr<PutContext> put_context,
                         int rv);
  void PutDidWriteHeaders(std::unique_ptr<PutContext> put_context,
                          int expected_bytes,
                          int rv);
  void PutDidWriteBlobToCache(std::unique_ptr<PutContext> put_context,
                              BlobToDiskCacheIDMap::KeyType blob_to_cache_key,
                              disk_cache::ScopedEntryPtr entry,
                              bool success);

  // Asynchronously calculates the current cache size, notifies the quota
  // manager of any change from the last report, and sets cache_size_ to the new
  // size.
  void UpdateCacheSize(base::OnceClosure callback);
  void UpdateCacheSizeGotSize(std::unique_ptr<CacheStorageCacheHandle>,
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
      CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  // Keys callbacks.
  void KeysImpl(std::unique_ptr<ServiceWorkerFetchRequest> request,
                const CacheStorageCacheQueryParams& options,
                RequestsCallback callback);
  void KeysDidQueryCache(
      RequestsCallback callback,
      CacheStorageError error,
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

  void InitBackend();
  void InitDidCreateBackend(base::OnceClosure callback,
                            CacheStorageError cache_create_error);
  void InitGotCacheSize(base::OnceClosure callback,
                        CacheStorageError cache_create_error,
                        int cache_size);
  void DeleteBackendCompletedIO();

  std::unique_ptr<storage::BlobDataHandle> PopulateResponseBody(
      disk_cache::ScopedEntryPtr entry,
      ServiceWorkerResponse* response);

  // Virtual for testing.
  virtual std::unique_ptr<CacheStorageCacheHandle> CreateCacheHandle();

  // Be sure to check |backend_state_| before use.
  std::unique_ptr<disk_cache::Backend> backend_;

  GURL origin_;
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
  int64_t cache_size_;
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
