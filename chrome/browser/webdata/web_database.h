// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/webdata/web_database_table.h"
#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

namespace base {
class FilePath;
}

namespace content {
class NotificationService;
}

// This class manages a SQLite database that stores various web page meta data.
class WebDatabase {
 public:
  enum State {
    COMMIT_NOT_NEEDED,
    COMMIT_NEEDED
  };
  // Exposed publicly so the keyword table can access it.
  static const int kCurrentVersionNumber;

  WebDatabase();
  virtual ~WebDatabase();

  // Adds a database table. Ownership remains with the caller, which
  // must ensure that the lifetime of |table| exceeds this object's
  // lifetime. Must only be called before Init.
  void AddTable(WebDatabaseTable* table);

  // Retrieves a table based on its |key|.
  WebDatabaseTable* GetTable(WebDatabaseTable::TypeKey key);

  // Initialize the database given a name. The name defines where the SQLite
  // file is. If this returns an error code, no other method should be called.
  //
  // Before calling this method, you must call AddTable for any
  // WebDatabaseTable objects that are supposed to participate in
  // managing the database.
  sql::InitStatus Init(const base::FilePath& db_name);

  // Transactions management
  void BeginTransaction();
  void CommitTransaction();

  // Exposed for testing only.
  sql::Connection* GetSQLConnection();

 private:
  // Used by |Init()| to migration database schema from older versions to
  // current version.
  sql::InitStatus MigrateOldVersionsAsNeeded();

  sql::Connection db_;
  sql::MetaTable meta_table_;

  // Map of all the different tables that have been added to this
  // object. Non-owning.
  typedef std::map<WebDatabaseTable::TypeKey, WebDatabaseTable*> TableMap;
  TableMap tables_;

  scoped_ptr<content::NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabase);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
