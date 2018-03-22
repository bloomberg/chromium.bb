// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_index.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/strong_associated_binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"

namespace url {
class Origin;
}

namespace content {

class CacheStorageContextImpl;

// Handles Cache Storage related messages sent to the browser process from
// child processes. One host instance exists per child process. All
// messages are processed on the IO thread.
class CONTENT_EXPORT CacheStorageDispatcherHost
    : public base::RefCountedThreadSafe<CacheStorageDispatcherHost,
                                        BrowserThread::DeleteOnIOThread>,
      public blink::mojom::CacheStorage {
 public:
  CacheStorageDispatcherHost();

  // Runs on UI thread.
  void Init(CacheStorageContextImpl* context);

  // Binds Mojo request to this instance, must be called on IO thread.
  // NOTE: The same CacheStorageDispatcherHost instance may be bound to
  // different clients on different origins. Each context is kept on
  // BindingSet's context. This guarantees that the browser process uses the
  // origin of the client known at the binding time, instead of relying on the
  // client to provide its origin at every method call.
  void AddBinding(blink::mojom::CacheStorageRequest request,
                  const url::Origin& origin);

 private:
  // Friends to allow BrowserThread::DeleteOnIOThread delegation.
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<CacheStorageDispatcherHost>;

  class CacheImpl;

  typedef std::map<std::string, std::list<storage::BlobDataHandle>>
      UUIDToBlobDataHandleList;

  ~CacheStorageDispatcherHost() override;

  // Called by Init() on IO thread.
  void CreateCacheListener(CacheStorageContextImpl* context);

  // Mojo CacheStorage Interface implementation:
  void Keys(blink::mojom::CacheStorage::KeysCallback callback) override;
  void Delete(const base::string16& cache_name,
              blink::mojom::CacheStorage::DeleteCallback callback) override;
  void Has(const base::string16& cache_name,
           blink::mojom::CacheStorage::HasCallback callback) override;
  void Match(const content::ServiceWorkerFetchRequest& request,
             const content::CacheStorageCacheQueryParams& match_params,
             blink::mojom::CacheStorage::MatchCallback callback) override;
  void Open(const base::string16& cache_name,
            blink::mojom::CacheStorage::OpenCallback callback) override;
  void BlobDataHandled(const std::string& uuid) override;

  // Callbacks used by Mojo implementation:
  void OnKeysCallback(KeysCallback callback,
                      const CacheStorageIndex& cache_index);
  void OnHasCallback(blink::mojom::CacheStorage::HasCallback callback,
                     bool has_cache,
                     blink::mojom::CacheStorageError error);
  void OnMatchCallback(
      blink::mojom::CacheStorage::MatchCallback callback,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<ServiceWorkerResponse> response,
      std::unique_ptr<storage::BlobDataHandle> blob_data_handle);
  void OnOpenCallback(url::Origin origin,
                      blink::mojom::CacheStorage::OpenCallback callback,
                      CacheStorageCacheHandle cache_handle,
                      blink::mojom::CacheStorageError error);

  // Stores blob handles while waiting for acknowledgement of receipt from the
  // renderer.
  void StoreBlobDataHandle(const storage::BlobDataHandle& blob_data_handle);
  void DropBlobDataHandle(const std::string& uuid);

  UUIDToBlobDataHandleList blob_handle_store_;

  scoped_refptr<CacheStorageContextImpl> context_;

  mojo::BindingSet<blink::mojom::CacheStorage, url::Origin> bindings_;
  mojo::StrongAssociatedBindingSet<blink::mojom::CacheStorageCache>
      cache_bindings_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
