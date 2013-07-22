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

  if (queued_transactions_.has(transaction)) {
    DCHECK(!started_transactions_.has(transaction));
    queued_transactions_.erase(transaction);
  } else {
    if (started_transactions_.has(transaction))
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
  if (queued_transactions_.has(transaction))
    found = true;
  if (started_transactions_.has(transaction)) {
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

  list_set<IndexedDBTransaction*>::const_iterator it =
      queued_transactions_.begin();
  while (it != queued_transactions_.end()) {
    IndexedDBTransaction* transaction = *it;
    ++it;
    if (CanRunTransaction(transaction)) {
      queued_transactions_.erase(transaction);
      started_transactions_.insert(transaction);
      transaction->Run();
    }
  }
}

static bool DoScopesOverlap(const std::set<int64>& scope1,
                            const std::set<int64>& scope2) {
  for (std::set<int64>::const_iterator it = scope1.begin(); it != scope1.end();
       ++it) {
    if (scope2.find(*it) != scope2.end())
      return true;
  }
  return false;
}

bool IndexedDBTransactionCoordinator::CanRunTransaction(
    IndexedDBTransaction* transaction) {
  DCHECK(queued_transactions_.has(transaction));
  switch (transaction->mode()) {
    case indexed_db::TRANSACTION_VERSION_CHANGE:
      DCHECK_EQ(static_cast<size_t>(1), queued_transactions_.size());
      DCHECK(started_transactions_.empty());
      return true;

    case indexed_db::TRANSACTION_READ_ONLY:
      return true;

    case indexed_db::TRANSACTION_READ_WRITE:
      for (list_set<IndexedDBTransaction*>::const_iterator it =
               started_transactions_.begin();
           it != started_transactions_.end();
           ++it) {
        IndexedDBTransaction* other = *it;
        if (other->mode() == indexed_db::TRANSACTION_READ_WRITE &&
            DoScopesOverlap(transaction->scope(), other->scope()))
          return false;
      }
      for (list_set<IndexedDBTransaction*>::const_iterator it =
               queued_transactions_.begin();
           *it != transaction;
           ++it) {
        DCHECK(it != queued_transactions_.end());
        IndexedDBTransaction* other = *it;
        if (other->mode() == indexed_db::TRANSACTION_READ_WRITE &&
            DoScopesOverlap(transaction->scope(), other->scope()))
          return false;
      }
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace content
