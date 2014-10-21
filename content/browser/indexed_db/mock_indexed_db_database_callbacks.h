// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_DATABASE_CALLBACKS_H_

#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"

namespace content {

class MockIndexedDBDatabaseCallbacks : public IndexedDBDatabaseCallbacks {
 public:
  MockIndexedDBDatabaseCallbacks();

  void OnVersionChange(int64 old_version, int64 new_version) override {}
  void OnForcedClose() override;
  void OnAbort(int64 transaction_id,
               const IndexedDBDatabaseError& error) override;
  void OnComplete(int64 transaction_id) override {}

  bool abort_called() const { return abort_called_; }
  bool forced_close_called() const { return forced_close_called_; }

 private:
  ~MockIndexedDBDatabaseCallbacks() override {}

  bool abort_called_;
  bool forced_close_called_;

  DISALLOW_COPY_AND_ASSIGN(MockIndexedDBDatabaseCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_DATABASE_CALLBACKS_H_
