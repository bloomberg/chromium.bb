// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_util.h"
#include "url/origin.h"

namespace content {

namespace {

const char kInvalidOrigin[] = "Origin is invalid";

bool IsValidOrigin(const url::Origin& origin) {
  return !origin.unique();
}

}  // namespace

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    int ipc_process_id,
    net::URLRequestContextGetter* request_context_getter,
    IndexedDBContextImpl* indexed_db_context,
    ChromeBlobStorageContext* blob_storage_context)
    : request_context_getter_(request_context_getter),
      indexed_db_context_(indexed_db_context),
      blob_storage_context_(blob_storage_context),
      ipc_process_id_(ipc_process_id) {
  DCHECK(indexed_db_context_.get());
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {}

void IndexedDBDispatcherHost::AddBinding(
    ::indexed_db::mojom::FactoryAssociatedRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

std::string IndexedDBDispatcherHost::HoldBlobData(
    const IndexedDBBlobInfo& blob_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::string uuid = blob_info.uuid();
  storage::BlobStorageContext* context = blob_storage_context_->context();
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
  if (uuid.empty()) {
    uuid = base::GenerateGUID();
    storage::BlobDataBuilder blob_data_builder(uuid);
    blob_data_builder.set_content_type(base::UTF16ToUTF8(blob_info.type()));
    blob_data_builder.AppendFile(blob_info.file_path(), 0, blob_info.size(),
                                 blob_info.last_modified());
    blob_data_handle = context->AddFinishedBlob(&blob_data_builder);
  } else {
    auto iter = blob_data_handle_map_.find(uuid);
    if (iter != blob_data_handle_map_.end()) {
      iter->second.second += 1;
      return uuid;
    }
    blob_data_handle = context->GetBlobDataFromUUID(uuid);
  }

  DCHECK(!base::ContainsKey(blob_data_handle_map_, uuid));
  blob_data_handle_map_[uuid] = std::make_pair(std::move(blob_data_handle), 1);
  return uuid;
}

void IndexedDBDispatcherHost::DropBlobData(const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const auto& iter = blob_data_handle_map_.find(uuid);
  if (iter == blob_data_handle_map_.end()) {
    DLOG(FATAL) << "Failed to find blob UUID in map:" << uuid;
    return;
  }

  DCHECK_GE(iter->second.second, 1);
  if (iter->second.second == 1)
    blob_data_handle_map_.erase(iter);
  else
    --iter->second.second;
}

void IndexedDBDispatcherHost::GetDatabaseNames(
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this, origin, std::move(callbacks_info)));
  indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&IndexedDBDispatcherHost::GetDatabaseNamesOnIDBThread, this,
                 base::Passed(&callbacks), origin));
}

void IndexedDBDispatcherHost::Open(
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
    ::indexed_db::mojom::DatabaseCallbacksAssociatedPtrInfo
        database_callbacks_info,
    const url::Origin& origin,
    const base::string16& name,
    int64_t version,
    int64_t transaction_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this, origin, std::move(callbacks_info)));
  scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks(
      new IndexedDBDatabaseCallbacks(this, std::move(database_callbacks_info)));
  indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&IndexedDBDispatcherHost::OpenOnIDBThread, this,
                 base::Passed(&callbacks), base::Passed(&database_callbacks),
                 origin, name, version, transaction_id));
}

void IndexedDBDispatcherHost::DeleteDatabase(
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
    const url::Origin& origin,
    const base::string16& name,
    bool force_close) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this, origin, std::move(callbacks_info)));
  indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE, base::Bind(&IndexedDBDispatcherHost::DeleteDatabaseOnIDBThread,
                            this, base::Passed(&callbacks), origin, name,
                            force_close));
}

void IndexedDBDispatcherHost::GetDatabaseNamesOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  context()->GetIDBFactory()->GetDatabaseNames(
      callbacks, origin, indexed_db_path, request_context_getter_);
}

void IndexedDBDispatcherHost::OpenOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    const url::Origin& origin,
    const base::string16& name,
    int64_t version,
    int64_t transaction_id) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());

  base::TimeTicks begin_time = base::TimeTicks::Now();
  base::FilePath indexed_db_path = indexed_db_context_->data_path();

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  callbacks->SetConnectionOpenStartTime(begin_time);
  std::unique_ptr<IndexedDBPendingConnection> connection =
      base::MakeUnique<IndexedDBPendingConnection>(
          callbacks, database_callbacks, ipc_process_id_, transaction_id,
          version);
  DCHECK(request_context_getter_);
  context()->GetIDBFactory()->Open(name, std::move(connection),
                                   request_context_getter_, origin,
                                   indexed_db_path);
}

void IndexedDBDispatcherHost::DeleteDatabaseOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin,
    const base::string16& name,
    bool force_close) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  DCHECK(request_context_getter_);
  context()->GetIDBFactory()->DeleteDatabase(
      name, request_context_getter_, callbacks, origin, indexed_db_path,
      force_close);
}

}  // namespace content
