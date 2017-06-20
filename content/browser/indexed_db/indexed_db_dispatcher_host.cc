// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner.h"
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

class IndexedDBDispatcherHost::IDBSequenceHelper {
 public:
  IDBSequenceHelper(
      int ipc_process_id,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<IndexedDBContextImpl> indexed_db_context)
      : ipc_process_id_(ipc_process_id),
        request_context_getter_(std::move(request_context_getter)),
        indexed_db_context_(std::move(indexed_db_context)) {}
  ~IDBSequenceHelper() {}

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

 private:
  const int ipc_process_id_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  DISALLOW_COPY_AND_ASSIGN(IDBSequenceHelper);
};

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    int ipc_process_id,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_refptr<IndexedDBContextImpl> indexed_db_context,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context)
    : indexed_db_context_(std::move(indexed_db_context)),
      blob_storage_context_(std::move(blob_storage_context)),
      idb_runner_(indexed_db_context_->TaskRunner()),
      ipc_process_id_(ipc_process_id),
      weak_factory_(this) {
  // Can be null in unittests.
  idb_helper_ = idb_runner_
                    ? new IDBSequenceHelper(ipc_process_id_,
                                            std::move(request_context_getter),
                                            indexed_db_context_)
                    : nullptr;
  DCHECK(indexed_db_context_.get());
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
  if (idb_helper_)
    idb_runner_->DeleteSoon(FROM_HERE, idb_helper_);
}

void IndexedDBDispatcherHost::AddBinding(
    ::indexed_db::mojom::FactoryAssociatedRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void IndexedDBDispatcherHost::AddDatabaseBinding(
    std::unique_ptr<::indexed_db::mojom::Database> database,
    ::indexed_db::mojom::DatabaseAssociatedRequest request) {
  database_bindings_.AddBinding(std::move(database), std::move(request));
}

void IndexedDBDispatcherHost::AddCursorBinding(
    std::unique_ptr<::indexed_db::mojom::Cursor> cursor,
    ::indexed_db::mojom::CursorAssociatedRequest request) {
  cursor_bindings_.AddBinding(std::move(cursor), std::move(request));
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

void IndexedDBDispatcherHost::RenderProcessExited(
    RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &IndexedDBDispatcherHost::InvalidateWeakPtrsAndClearBindings,
          base::Unretained(this)));
}

void IndexedDBDispatcherHost::GetDatabaseNames(
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      this->AsWeakPtr(), origin, std::move(callbacks_info), idb_runner_));
  idb_runner_->PostTask(
      FROM_HERE, base::BindOnce(&IDBSequenceHelper::GetDatabaseNamesOnIDBThread,
                                base::Unretained(idb_helper_),
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

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      this->AsWeakPtr(), origin, std::move(callbacks_info), idb_runner_));
  scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks(
      new IndexedDBDatabaseCallbacks(indexed_db_context_,
                                     std::move(database_callbacks_info)));
  idb_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&IDBSequenceHelper::OpenOnIDBThread,
                     base::Unretained(idb_helper_), base::Passed(&callbacks),
                     base::Passed(&database_callbacks), origin, name, version,
                     transaction_id));
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

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      this->AsWeakPtr(), origin, std::move(callbacks_info), idb_runner_));
  idb_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&IDBSequenceHelper::DeleteDatabaseOnIDBThread,
                     base::Unretained(idb_helper_), base::Passed(&callbacks),
                     origin, name, force_close));
}

void IndexedDBDispatcherHost::InvalidateWeakPtrsAndClearBindings() {
  weak_factory_.InvalidateWeakPtrs();
  cursor_bindings_.CloseAllBindings();
  database_bindings_.CloseAllBindings();
}

void IndexedDBDispatcherHost::IDBSequenceHelper::GetDatabaseNamesOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->GetDatabaseNames(
      callbacks, origin, indexed_db_path, request_context_getter_);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::OpenOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    const url::Origin& origin,
    const base::string16& name,
    int64_t version,
    int64_t transaction_id) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

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
  indexed_db_context_->GetIDBFactory()->Open(name, std::move(connection),
                                             request_context_getter_, origin,
                                             indexed_db_path);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::DeleteDatabaseOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin,
    const base::string16& name,
    bool force_close) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  DCHECK(request_context_getter_);
  indexed_db_context_->GetIDBFactory()->DeleteDatabase(
      name, request_context_getter_, callbacks, origin, indexed_db_path,
      force_close);
}

}  // namespace content
