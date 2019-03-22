// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/transaction_impl.h"

#include <utility>

#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"

namespace content {

TransactionImpl::TransactionImpl(
    base::WeakPtr<IndexedDBTransaction> transaction,
    const url::Origin& origin,
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : dispatcher_host_(dispatcher_host),
      indexed_db_context_(dispatcher_host->context()),
      transaction_(std::move(transaction)),
      origin_(origin),
      idb_runner_(std::move(idb_runner)),
      weak_factory_(this) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK(dispatcher_host_);
  DCHECK(transaction_);
}

TransactionImpl::~TransactionImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TransactionImpl::CreateObjectStore(int64_t object_store_id,
                                        const base::string16& name,
                                        const blink::IndexedDBKeyPath& key_path,
                                        bool auto_increment) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  connection->database()->CreateObjectStore(transaction_.get(), object_store_id,
                                            name, key_path, auto_increment);
}

void TransactionImpl::DeleteObjectStore(int64_t object_store_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  connection->database()->DeleteObjectStore(transaction_.get(),
                                            object_store_id);
}

}  // namespace content
