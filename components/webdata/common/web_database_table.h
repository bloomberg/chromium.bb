// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_TABLE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_TABLE_H_

#include "base/logging.h"
#include "base/macros.h"
#include "components/webdata/common/webdata_export.h"

namespace sql {
class Database;
class MetaTable;
}

// An abstract base class representing a table within a WebDatabase.
// Each table should subclass this, adding type-specific methods as needed.
class WEBDATA_EXPORT WebDatabaseTable {
 public:
  // To look up a WebDatabaseTable of a certain type from WebDatabase,
  // we use a void* key, so that we can simply use the address of one
  // of the type's statics.
  typedef void* TypeKey;

  // The object is not ready for use until Init() has been called.
  WebDatabaseTable();
  virtual ~WebDatabaseTable();

  // Retrieves the TypeKey for the concrete subtype.
  virtual TypeKey GetTypeKey() const = 0;

  // Stores the passed members as instance variables.
  void Init(sql::Database* db, sql::MetaTable* meta_table);

  // Create all of the expected SQL tables if they do not already exist.
  // Returns true on success, false on failure.
  virtual bool CreateTablesIfNecessary() = 0;

  // In order to encourage developers to think about sync when adding or
  // or altering new tables, this method must be implemented. Please get in
  // contact with the sync team if you believe you're making a change that they
  // should be aware of (or if you could break something).
  // TODO(andybons): Implement something more robust.
  virtual bool IsSyncable() = 0;

  // Migrates this table to |version|. Returns false if there was
  // migration work to do and it failed, true otherwise.
  //
  // Implementations may set |*update_compatible_version| to true if the
  // compatible version should be changed to |version|, i.e., if the change will
  // break previous versions when they try to use the updated database.
  // Implementations should otherwise not modify this parameter.
  virtual bool MigrateToVersion(int version,
                                bool* update_compatible_version) = 0;

 protected:
  // Non-owning. These are owned by WebDatabase, valid as long as that
  // class exists. Since lifetime of WebDatabaseTable objects slightly
  // exceeds that of WebDatabase, they should not be used in
  // ~WebDatabaseTable.
  sql::Database* db_;
  sql::MetaTable* meta_table_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDatabaseTable);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_TABLE_H_
