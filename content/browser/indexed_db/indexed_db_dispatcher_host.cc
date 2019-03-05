// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/indexed_db/cursor_impl.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_util.h"
#include "url/origin.h"

namespace content {

namespace {

blink::mojom::IDBStatus GetIndexedDBStatus(leveldb::Status status) {
  if (status.ok())
    return blink::mojom::IDBStatus::OK;
  else if (status.IsNotFound())
    return blink::mojom::IDBStatus::NotFound;
  else if (status.IsCorruption())
    return blink::mojom::IDBStatus::Corruption;
  else if (status.IsNotSupportedError())
    return blink::mojom::IDBStatus::NotSupported;
  else if (status.IsInvalidArgument())
    return blink::mojom::IDBStatus::InvalidArgument;
  else
    return blink::mojom::IDBStatus::IOError;
}

void CallCompactionStatusCallbackOnIDBThread(
    IndexedDBDispatcherHost::AbortTransactionsAndCompactDatabaseCallback
        mojo_callback,
    leveldb::Status status) {
  std::move(mojo_callback).Run(GetIndexedDBStatus(status));
}

void CallAbortStatusCallbackOnIDBThread(
    IndexedDBDispatcherHost::AbortTransactionsForDatabaseCallback mojo_callback,
    leveldb::Status status) {
  std::move(mojo_callback).Run(GetIndexedDBStatus(status));
}

}  // namespace

// TODO(cmp): Flatten calls / remove this class once IDB task runner CL settles.
class IndexedDBDispatcherHost::IDBSequenceHelper {
 public:
  IDBSequenceHelper(int ipc_process_id,
                    scoped_refptr<IndexedDBContextImpl> indexed_db_context)
      : ipc_process_id_(ipc_process_id),
        indexed_db_context_(std::move(indexed_db_context)) {}
  ~IDBSequenceHelper() {}

  void GetDatabaseInfoOnIDBThread(scoped_refptr<IndexedDBCallbacks> callbacks,
                                  const url::Origin& origin);

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
  void AbortTransactionsAndCompactDatabaseOnIDBThread(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin);
  void AbortTransactionsForDatabaseOnIDBThread(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin);

 private:
  const int ipc_process_id_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  DISALLOW_COPY_AND_ASSIGN(IDBSequenceHelper);
};

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    int ipc_process_id,
    scoped_refptr<IndexedDBContextImpl> indexed_db_context,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context)
    : indexed_db_context_(std::move(indexed_db_context)),
      blob_storage_context_(std::move(blob_storage_context)),
      ipc_process_id_(ipc_process_id),
      idb_helper_(std::make_unique<IDBSequenceHelper>(ipc_process_id_,
                                                      indexed_db_context_)),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(indexed_db_context_);
  DCHECK(blob_storage_context_);
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBDispatcherHost::AddBinding(
    blink::mojom::IDBFactoryRequest request,
    const url::Origin& origin) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bindings_.AddBinding(this, std::move(request), {origin});
}

void IndexedDBDispatcherHost::AddDatabaseBinding(
    std::unique_ptr<blink::mojom::IDBDatabase> database,
    blink::mojom::IDBDatabaseAssociatedRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  database_bindings_.AddBinding(std::move(database), std::move(request));
}

void IndexedDBDispatcherHost::AddCursorBinding(
    std::unique_ptr<CursorImpl> cursor,
    blink::mojom::IDBCursorAssociatedRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* cursor_ptr = cursor.get();
  mojo::BindingId binding_id =
      cursor_bindings_.AddBinding(std::move(cursor), std::move(request));
  cursor_ptr->OnRemoveBinding(
      base::BindOnce(&IndexedDBDispatcherHost::RemoveCursorBinding,
                     weak_factory_.GetWeakPtr(), binding_id));
}

void IndexedDBDispatcherHost::RemoveCursorBinding(mojo::BindingId binding_id) {
  cursor_bindings_.RemoveBinding(binding_id);
}

void IndexedDBDispatcherHost::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Since |this| is destructed on the IDB task runner, the next call would be
  // issued and run before any destruction event.  This guarantees that the
  // base::Unretained(this) usage is safe below.
  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IndexedDBDispatcherHost::InvalidateWeakPtrsAndClearBindings,
          base::Unretained(this)));
}

void IndexedDBDispatcherHost::GetDatabaseInfo(
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = bindings_.dispatch_context();
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this->AsWeakPtr(), context.origin,
                             std::move(callbacks_info), IDBTaskRunner()));
  idb_helper_->GetDatabaseInfoOnIDBThread(std::move(callbacks), context.origin);
}

void IndexedDBDispatcherHost::GetDatabaseNames(
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = bindings_.dispatch_context();
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this->AsWeakPtr(), context.origin,
                             std::move(callbacks_info), IDBTaskRunner()));
  idb_helper_->GetDatabaseNamesOnIDBThread(std::move(callbacks),
                                           context.origin);
}

void IndexedDBDispatcherHost::Open(
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info,
    blink::mojom::IDBDatabaseCallbacksAssociatedPtrInfo database_callbacks_info,
    const base::string16& name,
    int64_t version,
    int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = bindings_.dispatch_context();
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this->AsWeakPtr(), context.origin,
                             std::move(callbacks_info), IDBTaskRunner()));
  scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks(
      new IndexedDBDatabaseCallbacks(indexed_db_context_,
                                     std::move(database_callbacks_info),
                                     IDBTaskRunner()));
  idb_helper_->OpenOnIDBThread(std::move(callbacks),
                               std::move(database_callbacks), context.origin,
                               name, version, transaction_id);
}

void IndexedDBDispatcherHost::DeleteDatabase(
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info,
    const base::string16& name,
    bool force_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = bindings_.dispatch_context();
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(this->AsWeakPtr(), context.origin,
                             std::move(callbacks_info), IDBTaskRunner()));
  idb_helper_->DeleteDatabaseOnIDBThread(std::move(callbacks), context.origin,
                                         name, force_close);
}

void IndexedDBDispatcherHost::AbortTransactionsAndCompactDatabase(
    AbortTransactionsAndCompactDatabaseCallback mojo_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = bindings_.dispatch_context();
  base::OnceCallback<void(leveldb::Status)> callback_on_io = base::BindOnce(
      &CallCompactionStatusCallbackOnIDBThread, std::move(mojo_callback));
  idb_helper_->AbortTransactionsAndCompactDatabaseOnIDBThread(
      std::move(callback_on_io), context.origin);
}

void IndexedDBDispatcherHost::AbortTransactionsForDatabase(
    AbortTransactionsForDatabaseCallback mojo_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& context = bindings_.dispatch_context();
  base::OnceCallback<void(leveldb::Status)> callback_on_io = base::BindOnce(
      &CallAbortStatusCallbackOnIDBThread, std::move(mojo_callback));
  idb_helper_->AbortTransactionsForDatabaseOnIDBThread(
      std::move(callback_on_io), context.origin);
}

void IndexedDBDispatcherHost::InvalidateWeakPtrsAndClearBindings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  cursor_bindings_.CloseAllBindings();
  database_bindings_.CloseAllBindings();
}

base::SequencedTaskRunner* IndexedDBDispatcherHost::IDBTaskRunner() const {
  return indexed_db_context_->TaskRunner();
}

void IndexedDBDispatcherHost::IDBSequenceHelper::GetDatabaseInfoOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->GetDatabaseInfo(callbacks, origin,
                                                        indexed_db_path);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::GetDatabaseNamesOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->GetDatabaseNames(callbacks, origin,
                                                         indexed_db_path);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::OpenOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    const url::Origin& origin,
    const base::string16& name,
    int64_t version,
    int64_t transaction_id) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  std::unique_ptr<IndexedDBPendingConnection> connection =
      std::make_unique<IndexedDBPendingConnection>(
          callbacks, database_callbacks, ipc_process_id_, transaction_id,
          version);
  indexed_db_context_->GetIDBFactory()->Open(name, std::move(connection),
                                             origin, indexed_db_path);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::DeleteDatabaseOnIDBThread(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const url::Origin& origin,
    const base::string16& name,
    bool force_close) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  indexed_db_context_->GetIDBFactory()->DeleteDatabase(
      name, callbacks, origin, indexed_db_path, force_close);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::
    AbortTransactionsAndCompactDatabaseOnIDBThread(
        base::OnceCallback<void(leveldb::Status)> callback,
        const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  indexed_db_context_->GetIDBFactory()->AbortTransactionsAndCompactDatabase(
      std::move(callback), origin);
}

void IndexedDBDispatcherHost::IDBSequenceHelper::
    AbortTransactionsForDatabaseOnIDBThread(
        base::OnceCallback<void(leveldb::Status)> callback,
        const url::Origin& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());

  indexed_db_context_->GetIDBFactory()->AbortTransactionsForDatabase(
      std::move(callback), origin);
}

}  // namespace content
