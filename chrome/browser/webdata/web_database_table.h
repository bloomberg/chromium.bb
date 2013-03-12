// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_TABLE_H_
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_TABLE_H_

#include "base/logging.h"

namespace sql {
class Connection;
class MetaTable;
}

// An abstract base class representing a table within a WebDatabase.
// Each table should subclass this, adding type-specific methods as needed.
class WebDatabaseTable {
 public:
  WebDatabaseTable(sql::Connection* db, sql::MetaTable* meta_table);
  virtual ~WebDatabaseTable();

  // Attempts to initialize the table and returns true if successful.
  virtual bool Init() = 0;

  // In order to encourage developers to think about sync when adding or
  // or altering new tables, this method must be implemented. Please get in
  // contact with the sync team if you believe you're making a change that they
  // should be aware of (or if you could break something).
  // TODO(andybons): Implement something more robust.
  virtual bool IsSyncable() = 0;

  // Migrates this table to |version|. Returns false if there was
  // migration work to do and it failed, true otherwise.
  //
  // |app_locale| is the locale of the app. Passed as a parameter as
  // |it can only be safely queried on the UI thread.
  //
  // Implementations may set |*update_compatible_version| to true if
  // the compatible version should be changed to |version|.
  // Implementations should otherwise not modify this parameter.
  virtual bool MigrateToVersion(int version,
                                const std::string& app_locale,
                                bool* update_compatible_version) = 0;

 protected:
  sql::Connection* db_;
  sql::MetaTable* meta_table_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDatabaseTable);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_TABLE_H_
