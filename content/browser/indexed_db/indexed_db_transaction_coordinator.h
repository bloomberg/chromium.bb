// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_COORDINATOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_COORDINATOR_H_

#include <map>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/indexed_db/list_set.h"

namespace content {

class IndexedDBTransaction;

// Transactions are executed in the order the were created.
class IndexedDBTransactionCoordinator {
 public:
  IndexedDBTransactionCoordinator();
  ~IndexedDBTransactionCoordinator();

  // Called by transactions as they start and finish.
  void DidCreateTransaction(IndexedDBTransaction* transaction);
  void DidStartTransaction(IndexedDBTransaction* transaction);
  void DidFinishTransaction(IndexedDBTransaction* transaction);

#ifndef NDEBUG
  bool IsActive(IndexedDBTransaction* transaction);
#endif

  // Makes a snapshot of the transaction queue. For diagnostics only.
  std::vector<const IndexedDBTransaction*> GetTransactions() const;

 private:
  void ProcessStartedTransactions();
  bool CanRunTransaction(IndexedDBTransaction* transaction);

  // This is just an efficient way to keep references to all transactions.
  std::map<IndexedDBTransaction*, scoped_refptr<IndexedDBTransaction> >
      transactions_;

  // Transactions in different states are grouped below.
  // list_set is used to provide stable ordering; required by spec
  // for the queue, convenience for diagnostics for the rest.
  list_set<IndexedDBTransaction*> queued_transactions_;
  list_set<IndexedDBTransaction*> started_transactions_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_COORDINATOR_H_
