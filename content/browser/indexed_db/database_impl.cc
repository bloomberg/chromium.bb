// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/database_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"

using blink::IndexedDBIndexKeys;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using std::swap;

namespace blink {
class IndexedDBKeyRange;
}

namespace content {
namespace {
const char kInvalidBlobUuid[] = "Blob does not exist";
const char kInvalidBlobFilePath[] = "Blob file path is invalid";
const char kTransactionAlreadyExists[] = "Transaction already exists";

void LogUMAPutBlobCount(size_t blob_count) {
  UMA_HISTOGRAM_COUNTS_1000("WebCore.IndexedDB.PutBlobsCount", blob_count);
}

}  // namespace

// Expect to be created on IDB sequence and called/destroyed on IO thread.
class DatabaseImpl::IOHelper {
 public:
  enum class LoadResultCode {
    kNoop,
    kAbort,
    kInvalidBlobPath,
    kSuccess,
  };

  struct LoadResult {
    LoadResultCode code;
    blink::mojom::IDBValuePtr value;
    std::vector<IndexedDBBlobInfo> blob_info;
  };

  IOHelper(base::SequencedTaskRunner* idb_runner,
           scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
           int64_t ipc_process_id);
  ~IOHelper();

  void LoadBlobsOnIOThread(blink::mojom::IDBValuePtr value,
                           base::WaitableEvent* signal_when_finished,
                           LoadResult* result);

 private:
  // Friends to enable OnDestruct() delegation.
  friend class BrowserThread;
  friend class base::DeleteHelper<DatabaseImpl::IOHelper>;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  int64_t ipc_process_id_;
  SEQUENCE_CHECKER(sequence_checker_);
};

DatabaseImpl::DatabaseImpl(std::unique_ptr<IndexedDBConnection> connection,
                           const url::Origin& origin,
                           IndexedDBDispatcherHost* dispatcher_host,
                           scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : io_helper_(new IOHelper(idb_runner.get(),
                              dispatcher_host->blob_storage_context(),
                              dispatcher_host->ipc_process_id())),
      dispatcher_host_(dispatcher_host),
      indexed_db_context_(dispatcher_host->context()),
      connection_(std::move(connection)),
      origin_(origin),
      idb_runner_(std::move(idb_runner)),
      weak_factory_(this) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection_);
  indexed_db_context_->ConnectionOpened(origin_, connection_.get());
}

DatabaseImpl::~DatabaseImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (connection_->IsConnected())
    connection_->Close();
  indexed_db_context_->ConnectionClosed(origin_, connection_.get());
}

void DatabaseImpl::CreateObjectStore(int64_t transaction_id,
                                     int64_t object_store_id,
                                     const base::string16& name,
                                     const IndexedDBKeyPath& key_path,
                                     bool auto_increment) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->CreateObjectStore(transaction, object_store_id, name,
                                             key_path, auto_increment);
}

void DatabaseImpl::DeleteObjectStore(int64_t transaction_id,
                                     int64_t object_store_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->DeleteObjectStore(transaction, object_store_id);
}

void DatabaseImpl::RenameObjectStore(int64_t transaction_id,
                                     int64_t object_store_id,
                                     const base::string16& new_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->RenameObjectStore(transaction, object_store_id,
                                             new_name);
}

void DatabaseImpl::CreateTransaction(
    int64_t transaction_id,
    const std::vector<int64_t>& object_store_ids,
    blink::mojom::IDBTransactionMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  if (connection_->GetTransaction(transaction_id)) {
    mojo::ReportBadMessage(kTransactionAlreadyExists);
    return;
  }

  IndexedDBTransaction* transaction = connection_->CreateTransaction(
      transaction_id,
      std::set<int64_t>(object_store_ids.begin(), object_store_ids.end()), mode,
      new IndexedDBBackingStore::Transaction(
          connection_->database()->backing_store()));
  connection_->database()->RegisterAndScheduleTransaction(transaction);
}

void DatabaseImpl::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  connection_->Close();
}

void DatabaseImpl::VersionChangeIgnored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  connection_->VersionChangeIgnored();
}

void DatabaseImpl::AddObserver(int64_t transaction_id,
                               int32_t observer_id,
                               bool include_transaction,
                               bool no_records,
                               bool values,
                               uint32_t operation_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  IndexedDBObserver::Options options(include_transaction, no_records, values,
                                     operation_types);
  connection_->database()->AddPendingObserver(transaction, observer_id,
                                              options);
}

void DatabaseImpl::RemoveObservers(const std::vector<int32_t>& observers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  connection_->RemoveObservers(observers);
}

void DatabaseImpl::Get(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(callbacks_info), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->Get(transaction, object_store_id, index_id,
                               std::make_unique<IndexedDBKeyRange>(key_range),
                               key_only, callbacks);
}

void DatabaseImpl::GetAll(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    bool key_only,
    int64_t max_count,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(callbacks_info), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->GetAll(
      transaction, object_store_id, index_id,
      std::make_unique<IndexedDBKeyRange>(key_range), key_only, max_count,
      std::move(callbacks));
}

void DatabaseImpl::Put(
    int64_t transaction_id,
    int64_t object_store_id,
    blink::mojom::IDBValuePtr value_ptr,
    const IndexedDBKey& key,
    blink::mojom::IDBPutMode mode,
    const std::vector<IndexedDBIndexKeys>& index_keys,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(dispatcher_host_);

  IOHelper::LoadResult result;
  if (value_ptr->blob_or_file_info.empty()) {
    // If there are no blobs to process, we don't need to hop to the IO thread
    // to load blobs.
    result.code = IOHelper::LoadResultCode::kSuccess;
    result.value = std::move(value_ptr);
    result.blob_info = std::vector<IndexedDBBlobInfo>();
    LogUMAPutBlobCount(result.blob_info.size());
  } else {
    // TODO(crbug.com/932869): Remove IO thread hop entirely.
    base::WaitableEvent signal_when_finished(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    // |io_helper_| is owned by |this| and this call is synchronized with a
    // WaitableEvent, so |io_helper_| is guaranteed to remain alive throughout
    // the duration of the LoadBlobsOnIOThread() invocation.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&DatabaseImpl::IOHelper::LoadBlobsOnIOThread,
                       base::Unretained(io_helper_.get()), std::move(value_ptr),
                       &signal_when_finished, &result));
    signal_when_finished.Wait();
  }

  switch (result.code) {
    case IOHelper::LoadResultCode::kNoop:
      return;
    case IOHelper::LoadResultCode::kAbort: {
      IndexedDBDatabaseError error(blink::kWebIDBDatabaseExceptionUnknownError,
                                   kInvalidBlobUuid);
      scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
          dispatcher_host_->AsWeakPtr(), origin_, std::move(callbacks_info),
          dispatcher_host_->context()->TaskRunner()));

      callbacks->OnError(error);

      if (!connection_->IsConnected())
        return;

      IndexedDBTransaction* transaction =
          connection_->GetTransaction(transaction_id);
      if (!transaction)
        return;

      connection_->AbortTransaction(transaction, error);
      return;
    }
    case IOHelper::LoadResultCode::kInvalidBlobPath: {
      mojo::ReportBadMessage(kInvalidBlobFilePath);
      return;
    }
    case IOHelper::LoadResultCode::kSuccess: {
      if (!connection_->IsConnected())
        return;

      IndexedDBTransaction* transaction =
          connection_->GetTransaction(transaction_id);
      if (!transaction)
        return;

      // Value size recorded in IDBObjectStore before we can auto-wrap in a
      // blob. 1KB to 10MB.
      UMA_HISTOGRAM_COUNTS_10000("WebCore.IndexedDB.PutKeySize",
                                 key.size_estimate() / 1024);

      uint64_t commit_size = result.value->bits.size() + key.size_estimate();
      IndexedDBValue value;
      // TODO(crbug.com/902498): Use mojom traits to map directly to
      // std::string.
      value.bits =
          std::string(result.value->bits.begin(), result.value->bits.end());
      // Release result.value->bits std::vector.
      result.value->bits.clear();
      swap(value.blob_info, result.blob_info);
      scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
          dispatcher_host_->AsWeakPtr(), origin_, std::move(callbacks_info),
          dispatcher_host_->context()->TaskRunner()));
      connection_->database()->Put(transaction, object_store_id, &value,
                                   std::make_unique<IndexedDBKey>(key), mode,
                                   std::move(callbacks), index_keys);

      // Size can't be big enough to overflow because it represents the
      // actual bytes passed through IPC.
      transaction->set_size(transaction->size() + commit_size);
      return;
    }
    default:
      NOTREACHED();
      return;
  }
}

void DatabaseImpl::SetIndexKeys(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKey& primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->SetIndexKeys(
      transaction, object_store_id, std::make_unique<IndexedDBKey>(primary_key),
      index_keys);
}

void DatabaseImpl::SetIndexesReady(int64_t transaction_id,
                                   int64_t object_store_id,
                                   const std::vector<int64_t>& index_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->SetIndexesReady(transaction, object_store_id,
                                           index_ids);
}

void DatabaseImpl::OpenCursor(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction,
    bool key_only,
    blink::mojom::IDBTaskType task_type,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(callbacks_info), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->OpenCursor(
      transaction, object_store_id, index_id,
      std::make_unique<IndexedDBKeyRange>(key_range), direction, key_only,
      task_type, std::move(callbacks));
}

void DatabaseImpl::Count(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(callbacks_info), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->Count(transaction, object_store_id, index_id,
                                 std::make_unique<IndexedDBKeyRange>(key_range),
                                 std::move(callbacks));
}

void DatabaseImpl::DeleteRange(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(callbacks_info), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->DeleteRange(
      transaction, object_store_id,
      std::make_unique<IndexedDBKeyRange>(key_range), std::move(callbacks));
}

void DatabaseImpl::GetKeyGeneratorCurrentNumber(
    int64_t transaction_id,
    int64_t object_store_id,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(callbacks_info), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->GetKeyGeneratorCurrentNumber(
      transaction, object_store_id, std::move(callbacks));
}

void DatabaseImpl::Clear(
    int64_t transaction_id,
    int64_t object_store_id,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(callbacks_info), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->Clear(transaction, object_store_id, callbacks);
}

void DatabaseImpl::CreateIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const base::string16& name,
                               const IndexedDBKeyPath& key_path,
                               bool unique,
                               bool multi_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->CreateIndex(transaction, object_store_id, index_id,
                                       name, key_path, unique, multi_entry);
}

void DatabaseImpl::DeleteIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->DeleteIndex(transaction, object_store_id, index_id);
}

void DatabaseImpl::RenameIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const base::string16& new_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->database()->RenameIndex(transaction, object_store_id, index_id,
                                       new_name);
}

void DatabaseImpl::Abort(int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->AbortTransaction(
      transaction,
      IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionAbortError,
                             "Transaction aborted by user."));
}

void DatabaseImpl::Commit(int64_t transaction_id, int64_t num_errors_handled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  transaction->SetNumErrorsHandled(num_errors_handled);

  // Always allow empty or delete-only transactions.
  if (transaction->size() == 0) {
    connection_->database()->Commit(transaction);
    return;
  }

  indexed_db_context_->quota_manager_proxy()->GetUsageAndQuota(
      indexed_db_context_->TaskRunner(), origin_,
      blink::mojom::StorageType::kTemporary,
      base::BindOnce(&DatabaseImpl::OnGotUsageAndQuotaForCommit,
                     weak_factory_.GetWeakPtr(), transaction_id));
}

DatabaseImpl::IOHelper::IOHelper(
    base::SequencedTaskRunner* idb_runner,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    int64_t ipc_process_id)
    : blob_storage_context_(blob_storage_context),
      ipc_process_id_(ipc_process_id) {
  DCHECK(idb_runner->RunsTasksInCurrentSequence());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DatabaseImpl::IOHelper::~IOHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DatabaseImpl::IOHelper::LoadBlobsOnIOThread(
    blink::mojom::IDBValuePtr value,
    base::WaitableEvent* signal_when_finished,
    LoadResult* result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedClosureRunner signal_runner(
      base::BindOnce([](base::WaitableEvent* signal) { signal->Signal(); },
                     signal_when_finished));

  if (!blob_storage_context_) {
    result->code = IOHelper::LoadResultCode::kNoop;
    return;
  }

  // Should only be called if there are blobs to process.
  CHECK(!value->blob_or_file_info.empty());

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  base::CheckedNumeric<uint64_t> total_blob_size = 0;
  std::vector<IndexedDBBlobInfo> blob_info(value->blob_or_file_info.size());
  for (size_t i = 0; i < value->blob_or_file_info.size(); ++i) {
    blink::mojom::IDBBlobInfoPtr& info = value->blob_or_file_info[i];

    std::unique_ptr<storage::BlobDataHandle> handle =
        blob_storage_context_->context()->GetBlobDataFromUUID(info->uuid);

    // Due to known issue crbug.com/351753, blobs can die while being passed to
    // a different process. So this case must be handled gracefully.
    // TODO(dmurph): Revert back to using mojo::ReportBadMessage once fixed.
    UMA_HISTOGRAM_BOOLEAN("Storage.IndexedDB.PutValidBlob",
                          handle.get() != nullptr);
    if (!handle) {
      result->code = LoadResultCode::kAbort;
      return;
    }
    uint64_t size = handle->size();
    UMA_HISTOGRAM_MEMORY_KB("Storage.IndexedDB.PutBlobSizeKB", size / 1024ull);
    total_blob_size += size;

    if (info->file) {
      if (!info->file->path.empty() &&
          !policy->CanReadFile(ipc_process_id_, info->file->path)) {
        result->code = LoadResultCode::kInvalidBlobPath;
        return;
      }
      blob_info[i] = IndexedDBBlobInfo(std::move(handle), info->file->path,
                                       info->file->name, info->mime_type);
      if (info->size != -1) {
        blob_info[i].set_last_modified(info->file->last_modified);
        blob_info[i].set_size(info->size);
      }
    } else {
      blob_info[i] =
          IndexedDBBlobInfo(std::move(handle), info->mime_type, info->size);
    }
  }
  LogUMAPutBlobCount(blob_info.size());
  uint64_t blob_size = total_blob_size.ValueOrDefault(0U);
  if (blob_size != 0) {
    // Bytes to kilobytes.
    UMA_HISTOGRAM_COUNTS_1M("WebCore.IndexedDB.PutBlobsTotalSize",
                            blob_size / 1024);
  }
  result->code = LoadResultCode::kSuccess;
  result->value = std::move(value);
  result->blob_info = std::move(blob_info);
}

void DatabaseImpl::OnGotUsageAndQuotaForCommit(
    int64_t transaction_id,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // May have disconnected while quota check was pending.
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (status == blink::mojom::QuotaStatusCode::kOk &&
      usage + transaction->size() <= quota) {
    connection_->database()->Commit(transaction);
  } else {
    connection_->AbortTransaction(
        transaction,
        IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionQuotaError));
  }
}

}  // namespace content
