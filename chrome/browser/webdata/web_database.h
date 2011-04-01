// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
#pragma once

#include "app/sql/connection.h"
#include "app/sql/init_status.h"
#include "app/sql/meta_table.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/token_service_table.h"
#include "chrome/browser/webdata/web_apps_table.h"

class FilePath;
class NotificationService;

// This class manages a SQLite database that stores various web page meta data.
class WebDatabase {
 public:
  WebDatabase();
  virtual ~WebDatabase();

  // Initialize the database given a name. The name defines where the SQLite
  // file is. If this returns an error code, no other method should be called.
  sql::InitStatus Init(const FilePath& db_name);

  // Transactions management
  void BeginTransaction();
  void CommitTransaction();

  virtual AutofillTable* GetAutofillTable();
  virtual KeywordTable* GetKeywordTable();
  virtual LoginsTable* GetLoginsTable();
  virtual TokenServiceTable* GetTokenServiceTable();
  virtual WebAppsTable* GetWebAppsTable();

  // Exposed for testing only.
  sql::Connection* GetSQLConnection();

 private:
  // Used by |Init()| to migration database schema from older versions to
  // current version.
  sql::InitStatus MigrateOldVersionsAsNeeded();

  sql::Connection db_;
  sql::MetaTable meta_table_;

  scoped_ptr<AutofillTable> autofill_table_;
  scoped_ptr<KeywordTable> keyword_table_;
  scoped_ptr<LoginsTable> logins_table_;
  scoped_ptr<TokenServiceTable> token_service_table_;
  scoped_ptr<WebAppsTable> web_apps_table_;

  scoped_ptr<NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabase);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
