// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_TABLE_H_
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_TABLE_H_
#pragma once

#include "app/sql/connection.h"
#include "app/sql/init_status.h"
#include "app/sql/meta_table.h"

// An abstract base class representing a table within a WebDatabase.
// Each table should subclass this, adding type-specific methods as needed.
class WebDatabaseTable {
 protected:
  explicit WebDatabaseTable(sql::Connection* db, sql::MetaTable* meta_table)
      : db_(db), meta_table_(meta_table) {}
  virtual ~WebDatabaseTable() {
    db_ = NULL;
    meta_table_ = NULL;
  };

  // Attempts to initialize the table and returns true if successful.
  virtual bool Init() = 0;

  // In order to encourage developers to think about sync when adding or
  // or altering new tables, this method must be implemented. Please get in
  // contact with the sync team if you believe you're making a change that they
  // should be aware of (or if you could break something).
  // TODO(andybons): Implement something more robust.
  virtual bool IsSyncable() = 0;

  sql::Connection* db_;
  sql::MetaTable* meta_table_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDatabaseTable);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_TABLE_H_
