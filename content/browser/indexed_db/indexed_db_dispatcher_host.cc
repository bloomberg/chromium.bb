// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/bad_message.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/indexed_db_metadata.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_util.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"
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
    : BrowserMessageFilter(IndexedDBMsgStart),
      BrowserAssociatedInterface(this, this),
      request_context_getter_(request_context_getter),
      indexed_db_context_(indexed_db_context),
      blob_storage_context_(blob_storage_context),
      ipc_process_id_(ipc_process_id) {
  DCHECK(indexed_db_context_.get());
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
  // TODO(alecflett): uncomment these when we find the source of these leaks.
  // DCHECK(transaction_size_map_.empty());
  // DCHECK(transaction_origin_map_.empty());
}

void IndexedDBDispatcherHost::OnChannelClosing() {
  bool success = indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&IndexedDBDispatcherHost::ResetDispatcherHosts, this));

  if (!success)
    ResetDispatcherHosts();
}

void IndexedDBDispatcherHost::OnDestruct() const {
  // The last reference to the dispatcher may be a posted task, which would
  // be destructed on the IndexedDB thread. Without this override, that would
  // take the dispatcher with it. Since the dispatcher may be keeping the
  // IndexedDBContext alive, it might be destructed to on its own thread,
  // which is not supported. Ensure destruction runs on the IO thread instead.
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void IndexedDBDispatcherHost::ResetDispatcherHosts() {
  // It is important that the various *_dispatcher_host_ members are reset
  // on the IndexedDB thread, since there might be incoming messages on that
  // thread, and we must not reset the dispatcher hosts until after those
  // messages are processed.
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());

  // Prevent any pending connections from being processed.
  is_open_ = false;
}

bool IndexedDBDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool IndexedDBDispatcherHost::RegisterTransactionId(int64_t host_transaction_id,
                                                    const url::Origin& origin) {
  if (base::ContainsKey(transaction_size_map_, host_transaction_id))
    return false;
  transaction_size_map_[host_transaction_id] = 0;
  transaction_origin_map_[host_transaction_id] = origin;
  return true;
}

bool IndexedDBDispatcherHost::GetTransactionSize(int64_t host_transaction_id,
                                                 int64_t* transaction_size) {
  const auto it = transaction_size_map_.find(host_transaction_id);
  if (it == transaction_size_map_.end())
    return false;
  *transaction_size = it->second;
  return true;
}

void IndexedDBDispatcherHost::AddToTransaction(int64_t host_transaction_id,
                                               int64_t value_length) {
  transaction_size_map_[host_transaction_id] += value_length;
}

int64_t IndexedDBDispatcherHost::HostTransactionId(int64_t transaction_id) {
  // Inject the renderer process id into the transaction id, to
  // uniquely identify this transaction, and effectively bind it to
  // the renderer that initiated it. The lower 32 bits of
  // transaction_id are guaranteed to be unique within that renderer.
  base::ProcessId pid = peer_pid();
  DCHECK(!(transaction_id >> 32)) << "Transaction ids can only be 32 bits";
  static_assert(sizeof(base::ProcessId) <= sizeof(int32_t),
                "Process ID must fit in 32 bits");

  return transaction_id | (static_cast<uint64_t>(pid) << 32);
}

int64_t IndexedDBDispatcherHost::RendererTransactionId(
    int64_t host_transaction_id) {
  DCHECK(host_transaction_id >> 32 == peer_pid())
      << "Invalid renderer target for transaction id";
  return host_transaction_id & 0xffffffff;
}

// static
uint32_t IndexedDBDispatcherHost::TransactionIdToRendererTransactionId(
    int64_t host_transaction_id) {
  return host_transaction_id & 0xffffffff;
}

// static
uint32_t IndexedDBDispatcherHost::TransactionIdToProcessId(
    int64_t host_transaction_id) {
  return (host_transaction_id >> 32) & 0xffffffff;
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

bool IndexedDBDispatcherHost::IsOpen() const {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  return is_open_;
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
    const base::string16& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsValidOrigin(origin)) {
    mojo::ReportBadMessage(kInvalidOrigin);
    return;
  }

  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this, origin, std::move(callbacks_info)));
  indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE, base::Bind(&IndexedDBDispatcherHost::DeleteDatabaseOnIDBThread,
                            this, base::Passed(&callbacks), origin, name));
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

  int64_t host_transaction_id = HostTransactionId(transaction_id);

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  callbacks->SetConnectionOpenStartTime(begin_time);
  callbacks->set_host_transaction_id(host_transaction_id);
  std::unique_ptr<IndexedDBPendingConnection> connection =
      base::MakeUnique<IndexedDBPendingConnection>(
          callbacks, database_callbacks, ipc_process_id_, host_transaction_id,
          version);
  DCHECK(request_context_getter_);
  context()->GetIDBFactory()->Open(name, std::move(connection),
                                   request_context_getter_, origin,
                                   indexed_db_path);
}

void IndexedDBDispatcherHost::DeleteDatabaseOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin,
    const base::string16& name) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  DCHECK(request_context_getter_);
  context()->GetIDBFactory()->DeleteDatabase(
      name, request_context_getter_, callbacks, origin, indexed_db_path);
}

void IndexedDBDispatcherHost::FinishTransaction(int64_t host_transaction_id,
                                                bool committed) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  if (committed) {
    context()->TransactionComplete(
        transaction_origin_map_[host_transaction_id]);
  }
  transaction_origin_map_.erase(host_transaction_id);
  transaction_size_map_.erase(host_transaction_id);
}

}  // namespace content
