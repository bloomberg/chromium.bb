// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_URLS_SQL_HANDLER_H_
#define CHROME_BROWSER_HISTORY_ANDROID_URLS_SQL_HANDLER_H_

#include "chrome/browser/history/android/sql_handler.h"

namespace history {

class HistoryDatabase;

// This class is the SQLHandler implementation for urls table.
class UrlsSQLHandler : public SQLHandler {
 public:
  explicit UrlsSQLHandler(HistoryDatabase* history_db);
  virtual ~UrlsSQLHandler();

  // Overriden from SQLHandler.
  virtual bool Insert(HistoryAndBookmarkRow* row) OVERRIDE;
  virtual bool Update(const HistoryAndBookmarkRow& row,
                      const TableIDRows& ids_set) OVERRIDE;
  virtual bool Delete(const TableIDRows& ids_set) OVERRIDE;

 private:
  HistoryDatabase* history_db_;

  DISALLOW_COPY_AND_ASSIGN(UrlsSQLHandler);
};

}  // namespace history.

#endif  // CHROME_BROWSER_HISTORY_ANDROID_URLS_SQL_HANDLER_H_
