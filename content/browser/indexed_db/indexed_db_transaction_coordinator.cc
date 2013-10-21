// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"

namespace content {

IndexedDBTransactionCoordinator::IndexedDBTransactionCoordinator() {}

IndexedDBTransactionCoordinator::~IndexedDBTransactionCoordinator() {
  DCHECK(!transactions_.size());
  DCHECK(!queued_transactions_.size());
  DCHECK(!started_transactions_.size());
}

void IndexedDBTransactionCoordinator::DidCreateTransaction(
    IndexedDBTransaction* transaction) {
  DCHECK(transactions_.find(transaction) == transactions_.end());
  DCHECK(transaction->queue_status() == IndexedDBTransaction::CREATED);
  transactions_[transaction] = transaction;
}

void IndexedDBTransactionCoordinator::DidStartTransaction(
    IndexedDBTransaction* transaction) {
  DCHECK(transactions_.find(transaction) != transactions_.end());

  queued_transactions_.insert(transaction);
  ProcessStartedTransactions();
}

void IndexedDBTransactionCoordinator::DidFinishTransaction(
    IndexedDBTransaction* transaction) {
  DCHECK(transactions_.find(transaction) != transactions_.end());

  if (queued_transactions_.count(transaction)) {
    DCHECK(!started_transactions_.count(transaction));
    queued_transactions_.erase(transaction);
  } else {
    if (started_transactions_.count(transaction))
      started_transactions_.erase(transaction);
  }
  transactions_.erase(transaction);

  ProcessStartedTransactions();
}

#ifndef NDEBUG
// Verifies internal consistency while returning whether anything is found.
bool IndexedDBTransactionCoordinator::IsActive(
    IndexedDBTransaction* transaction) {
  bool found = false;
  if (queued_transactions_.count(transaction))
    found = true;
  if (started_transactions_.count(transaction)) {
    DCHECK(!found);
    found = true;
  }
  DCHECK_EQ(found, (transactions_.find(transaction) != transactions_.end()));
  return found;
}
#endif

std::vector<const IndexedDBTransaction*>
IndexedDBTransactionCoordinator::GetTransactions() const {
  std::vector<const IndexedDBTransaction*> result;

  for (list_set<IndexedDBTransaction*>::const_iterator it =
           started_transactions_.begin();
       it != started_transactions_.end();
       ++it) {
    result.push_back(*it);
  }
  for (list_set<IndexedDBTransaction*>::const_iterator it =
           queued_transactions_.begin();
       it != queued_transactions_.end();
       ++it) {
    result.push_back(*it);
  }

  return result;
}

void IndexedDBTransactionCoordinator::ProcessStartedTransactions() {
  if (queued_transactions_.empty())
    return;

  DCHECK(started_transactions_.empty() ||
         (*started_transactions_.begin())->mode() !=
             indexed_db::TRANSACTION_VERSION_CHANGE);

  // The locked_scope set accumulates the ids of object stores in the scope of
  // running read/write transactions. Other read-write transactions with
  // stores in this set may not be started. Read-only transactions may start,
  // taking a snapshot of the database, which does not include uncommitted
  // data. ("Version change" transactions are exclusive, but handled by the
  // connection sequencing in IndexedDBDatabase.)
  std::set<int64> locked_scope;
  for (list_set<IndexedDBTransaction*>::const_iterator it =
           started_transactions_.begin();
       it != started_transactions_.end();
       ++it) {
    IndexedDBTransaction* transaction = *it;
    if (transaction->mode() == indexed_db::TRANSACTION_READ_WRITE) {
      // Running read/write transactions have exclusive access to the object
      // stores within their scopes.
      locked_scope.insert(transaction->scope().begin(),
                          transaction->scope().end());
    }
  }

  list_set<IndexedDBTransaction*>::const_iterator it =
      queued_transactions_.begin();
  while (it != queued_transactions_.end()) {
    IndexedDBTransaction* transaction = *it;
    ++it;
    if (CanRunTransaction(transaction, locked_scope)) {
      transaction->set_queue_status(IndexedDBTransaction::UNBLOCKED);
      queued_transactions_.erase(transaction);
      started_transactions_.insert(transaction);
      transaction->Run();
    } else {
      transaction->set_queue_status(IndexedDBTransaction::BLOCKED);
    }
    if (transaction->mode() == indexed_db::TRANSACTION_READ_WRITE) {
      // Either the transaction started, so it has exclusive access to the
      // stores in its scope, or per the spec the transaction which was
      // created first must get access first, so the stores are also locked.
      locked_scope.insert(transaction->scope().begin(),
                          transaction->scope().end());
    }
  }
}

template<typename T>
static bool DoSetsIntersect(const std::set<T>& set1,
                            const std::set<T>& set2) {
  typename std::set<T>::const_iterator it1 = set1.begin();
  typename std::set<T>::const_iterator it2 = set2.begin();
  while (it1 != set1.end() && it2 != set2.end()) {
    if (*it1 < *it2)
      ++it1;
    else if (*it2 < *it1)
      ++it2;
    else
      return true;
  }
  return false;
}

bool IndexedDBTransactionCoordinator::CanRunTransaction(
    IndexedDBTransaction* const transaction,
    const std::set<int64>& locked_scope) const {
  DCHECK(queued_transactions_.count(transaction));
  switch (transaction->mode()) {
    case indexed_db::TRANSACTION_VERSION_CHANGE:
      DCHECK_EQ(static_cast<size_t>(1), queued_transactions_.size());
      DCHECK(started_transactions_.empty());
      DCHECK(locked_scope.empty());
      return true;

    case indexed_db::TRANSACTION_READ_ONLY:
      return true;

    case indexed_db::TRANSACTION_READ_WRITE:
      return !DoSetsIntersect(transaction->scope(), locked_scope);
  }
  NOTREACHED();
  return false;
}

}  // namespace content
