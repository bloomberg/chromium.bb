// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOCK_BROWSING_DATA_INDEXED_DB_HELPER_H_
#define CHROME_BROWSER_MOCK_BROWSING_DATA_INDEXED_DB_HELPER_H_
#pragma once

#include <list>
#include <map>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"

// Mock for BrowsingDataIndexedDBHelper.
// Use AddIndexedDBSamples() or add directly to response_ list, then
// call Notify().
class MockBrowsingDataIndexedDBHelper
    : public BrowsingDataIndexedDBHelper {
 public:
  MockBrowsingDataIndexedDBHelper();

  // Adds some IndexedDBInfo samples.
  void AddIndexedDBSamples();

  // Notifies the callback.
  void Notify();

  // Marks all indexed db files as existing.
  void Reset();

  // Returns true if all indexed db files were deleted since the last
  // Reset() invokation.
  bool AllDeleted();

  // BrowsingDataIndexedDBHelper.
  virtual void StartFetching(
      Callback1<const std::list<IndexedDBInfo>& >::Type* callback);
  virtual void CancelNotification();
  virtual void DeleteIndexedDB(const GURL& origin);

 private:
  virtual ~MockBrowsingDataIndexedDBHelper();

  scoped_ptr<Callback1<const std::list<IndexedDBInfo>& >::Type > callback_;
  std::map<GURL, bool> origins_;
  std::list<IndexedDBInfo> response_;
};

#endif  // CHROME_BROWSER_MOCK_BROWSING_DATA_INDEXED_DB_HELPER_H_
