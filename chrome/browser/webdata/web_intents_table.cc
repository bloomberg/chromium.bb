// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_intents_table.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "sql/statement.h"

WebIntentsTable::WebIntentsTable(sql::Connection* db,
                                 sql::MetaTable* meta_table)
    : WebDatabaseTable(db, meta_table) {
}

WebIntentsTable::~WebIntentsTable() {
}

bool WebIntentsTable::Init() {
  if (db_->DoesTableExist("web_intents"))
    return true;

  if (!db_->Execute("CREATE TABLE web_intents ("
                    "service_url LONGVARCHAR,"
                    "action VARCHAR,"
                    "type VARCHAR,"
                    "UNIQUE (service_url, action, type))")) {
    NOTREACHED();
    return false;
  }

  if (!db_->Execute("CREATE INDEX web_intents_index "
                     "ON web_intents (action)")) {
    NOTREACHED();
    return false;
  }

  return true;
}

// TODO(jhawkins): Figure out Sync story.
bool WebIntentsTable::IsSyncable() {
  return false;
}

bool WebIntentsTable::GetWebIntents(const string16& action,
                                    std::vector<WebIntentData>* intents) {
  DCHECK(intents);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT service_url, action, type FROM web_intents "
      "WHERE action=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, action);
  while (s.Step()) {
    WebIntentData intent;
    string16 tmp = s.ColumnString16(0);
    intent.service_url = GURL(tmp);

    intent.action = s.ColumnString16(1);
    intent.type = s.ColumnString16(2);

    intents->push_back(intent);
  }
  return true;
}

bool WebIntentsTable::SetWebIntent(const string16& action,
                                   const string16& type,
                                   const GURL& service_url) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO web_intents (service_url, type, action) "
      "VALUES (?, ?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, service_url.spec());
  s.BindString16(1, type);
  s.BindString16(2, action);
  return s.Run();
}

bool WebIntentsTable::RemoveWebIntent(const string16& action,
                                      const string16& type,
                                      const GURL& service_url) {
  sql::Statement delete_s(db_->GetUniqueStatement(
      "DELETE FROM web_intents "
      "WHERE service_url = ? AND action = ? AND type = ?"));
  if (!delete_s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  delete_s.BindString(0, service_url.spec());
  delete_s.BindString16(1, action);
  delete_s.BindString16(2, type);
  return delete_s.Run();
}

