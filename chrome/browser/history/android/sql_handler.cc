// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/sql_handler.h"

namespace history {

TableIDRow::TableIDRow()
    : url_id(0) {
}

TableIDRow::~TableIDRow() {
}

SQLHandler::SQLHandler(const BookmarkRow::BookmarkColumnID columns[],
                       int column_count)
    : columns_(columns, columns + column_count) {
}

SQLHandler::~SQLHandler() {
}

bool SQLHandler::HasColumnIn(const BookmarkRow& row) {
  for (std::set<BookmarkRow::BookmarkColumnID>::const_iterator i =
           columns_.begin(); i != columns_.end(); ++i) {
    if (row.is_value_set_explicitly(*i))
      return true;
  }
  return false;
}

bool SQLHandler::HasColumn(BookmarkRow::BookmarkColumnID id) {
  return columns_.find(id) != columns_.end();
}

}  // namespace history.
