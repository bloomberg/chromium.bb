// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_FAVICON_SQL_HANDLER_H_
#define CHROME_BROWSER_HISTORY_ANDROID_FAVICON_SQL_HANDLER_H_

#include "chrome/browser/history/android/sql_handler.h"

namespace history {

class ThumbnailDatabase;

// The SQL handler implementation for icon_mapping and favicon table.
class FaviconSQLHandler : public SQLHandler {
 public:
  explicit FaviconSQLHandler(ThumbnailDatabase* thumbnail_db);
  virtual ~FaviconSQLHandler();

  // SQLHandler overrides:
  virtual bool Update(const HistoryAndBookmarkRow& row,
                      const TableIDRows& ids_set) OVERRIDE;
  virtual bool Delete(const TableIDRows& ids_set) OVERRIDE;
  virtual bool Insert(HistoryAndBookmarkRow* row) OVERRIDE;

 private:
  // Deletes the given favicons if they are not used by any pages. Returns
  // true if all unused favicons are deleted.
  bool DeleteUnusedFavicon(const std::vector<favicon_base::FaviconID>& ids);

  ThumbnailDatabase* thumbnail_db_;

  DISALLOW_COPY_AND_ASSIGN(FaviconSQLHandler);
};

}  // namespace history.

#endif  // CHROME_BROWSER_HISTORY_ANDROID_FAVICON_SQL_HANDLER_H_
