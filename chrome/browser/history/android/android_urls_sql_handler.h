// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_ANDROID_URLS_SQL_HANDLER_H_
#define CHROME_BROWSER_HISTORY_ANDROID_ANDROID_URLS_SQL_HANDLER_H_

#include "chrome/browser/history/android/sql_handler.h"

namespace history {

class HistoryDatabase;

// The SQLHanlder implementation for android_urls table.
class AndroidURLsSQLHandler : public SQLHandler {
 public:
  explicit AndroidURLsSQLHandler(HistoryDatabase* history_db);
  virtual ~AndroidURLsSQLHandler();

  virtual bool Update(const HistoryAndBookmarkRow& row,
                      const TableIDRows& ids_set) OVERRIDE;

  virtual bool Insert(HistoryAndBookmarkRow* row) OVERRIDE;

  virtual bool Delete(const TableIDRows& ids_set) OVERRIDE;

 private:
  HistoryDatabase* history_db_;

  DISALLOW_COPY_AND_ASSIGN(AndroidURLsSQLHandler);
};

}  // namespace history.

#endif  // CHROME_BROWSER_HISTORY_ANDROID_ANDROID_URLS_SQL_HANDLER_H_
