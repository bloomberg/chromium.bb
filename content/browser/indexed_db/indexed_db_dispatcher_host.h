// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/common/indexed_db/indexed_db.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"

namespace url {
class Origin;
}

namespace content {
class IndexedDBBlobInfo;
class IndexedDBCallbacks;
class IndexedDBContextImpl;
class IndexedDBDatabaseCallbacks;

// Handles all IndexedDB related messages from a particular renderer process.
class IndexedDBDispatcherHost
    : public base::RefCountedThreadSafe<IndexedDBDispatcherHost,
                                        BrowserThread::DeleteOnIOThread>,
      public ::indexed_db::mojom::Factory {
 public:
  // Only call the constructor from the UI thread.
  IndexedDBDispatcherHost(int ipc_process_id,
                          net::URLRequestContextGetter* request_context_getter,
                          IndexedDBContextImpl* indexed_db_context,
                          ChromeBlobStorageContext* blob_storage_context);

  void AddBinding(::indexed_db::mojom::FactoryAssociatedRequest request);

  // A shortcut for accessing our context.
  IndexedDBContextImpl* context() const { return indexed_db_context_.get(); }
  storage::BlobStorageContext* blob_storage_context() const {
    return blob_storage_context_->context();
  }
  int ipc_process_id() const { return ipc_process_id_; }

  std::string HoldBlobData(const IndexedDBBlobInfo& blob_info);
  void DropBlobData(const std::string& uuid);

 private:
  // Friends to enable OnDestruct() delegation.
  friend class BrowserThread;
  friend class base::DeleteHelper<IndexedDBDispatcherHost>;

  ~IndexedDBDispatcherHost() override;

  // indexed_db::mojom::Factory implementation:
  void GetDatabaseNames(
      ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
      const url::Origin& origin) override;
  void Open(::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
            ::indexed_db::mojom::DatabaseCallbacksAssociatedPtrInfo
                database_callbacks_info,
            const url::Origin& origin,
            const base::string16& name,
            int64_t version,
            int64_t transaction_id) override;
  void DeleteDatabase(
      ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
      const url::Origin& origin,
      const base::string16& name,
      bool force_close) override;

  void GetDatabaseNamesOnIDBThread(scoped_refptr<IndexedDBCallbacks> callbacks,
                                   const url::Origin& origin);
  void OpenOnIDBThread(
      scoped_refptr<IndexedDBCallbacks> callbacks,
      scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
      const url::Origin& origin,
      const base::string16& name,
      int64_t version,
      int64_t transaction_id);
  void DeleteDatabaseOnIDBThread(scoped_refptr<IndexedDBCallbacks> callbacks,
                                 const url::Origin& origin,
                                 const base::string16& name,
                                 bool force_close);

  void ResetDispatcherHosts();

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Maps blob uuid to a pair (handle, ref count). Entry is added and/or count
  // is incremented in HoldBlobData(), and count is decremented and/or entry
  // removed in DropBlobData().
  std::map<std::string,
           std::pair<std::unique_ptr<storage::BlobDataHandle>, int>>
      blob_data_handle_map_;

  // Used to set file permissions for blob storage.
  const int ipc_process_id_;

  mojo::AssociatedBindingSet<::indexed_db::mojom::Factory> bindings_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
