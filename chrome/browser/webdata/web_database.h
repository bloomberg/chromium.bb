// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_

#include "base/memory/scoped_ptr.h"
#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

class AutofillTable;
class FilePath;
class KeywordTable;
class LoginsTable;
class TokenServiceTable;
class WebAppsTable;
class WebIntentsTable;

namespace content {
class NotificationService;
}

// This class manages a SQLite database that stores various web page meta data.
class WebDatabase {
 public:
  // Exposed publicly so the keyword table can access it.
  static const int kCurrentVersionNumber;

  WebDatabase();
  virtual ~WebDatabase();

  // Initialize the database given a name. The name defines where the SQLite
  // file is. If this returns an error code, no other method should be called.
  // Requires the |app_locale| to be passed as a parameter as the locale can
  // only safely be queried on the UI thread.
  sql::InitStatus Init(const FilePath& db_name, const std::string& app_locale);

  // Transactions management
  void BeginTransaction();
  void CommitTransaction();

  virtual AutofillTable* GetAutofillTable();
  virtual KeywordTable* GetKeywordTable();
  virtual LoginsTable* GetLoginsTable();
  virtual TokenServiceTable* GetTokenServiceTable();
  virtual WebAppsTable* GetWebAppsTable();
  virtual WebIntentsTable* GetWebIntentsTable();

  // Exposed for testing only.
  sql::Connection* GetSQLConnection();

 private:
  // Used by |Init()| to migration database schema from older versions to
  // current version.  Requires the |app_locale| to be passed as a parameter as
  // the locale can only safely be queried on the UI thread.
  sql::InitStatus MigrateOldVersionsAsNeeded(const std::string& app_locale);

  sql::Connection db_;
  sql::MetaTable meta_table_;

  scoped_ptr<AutofillTable> autofill_table_;
  scoped_ptr<KeywordTable> keyword_table_;
  scoped_ptr<LoginsTable> logins_table_;
  scoped_ptr<TokenServiceTable> token_service_table_;
  scoped_ptr<WebAppsTable> web_apps_table_;
  scoped_ptr<WebIntentsTable> web_intents_table_;

  scoped_ptr<content::NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabase);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
