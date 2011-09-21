// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_intents_table.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "sql/statement.h"

namespace {
bool ExtractIntents(sql::Statement* s,
                    std::vector<WebIntentServiceData>* services) {
  DCHECK(s);
  while (s->Step()) {
    WebIntentServiceData service;
    string16 tmp = s->ColumnString16(0);
    service.service_url = GURL(tmp);

    service.action = s->ColumnString16(1);
    service.type = s->ColumnString16(2);
    service.title = s->ColumnString16(3);
    tmp = s->ColumnString16(4);
     // Default to window disposition.
    service.disposition = WebIntentServiceData::DISPOSITION_WINDOW;
    if (tmp == ASCIIToUTF16("inline"))
      service.disposition = WebIntentServiceData::DISPOSITION_INLINE;
    services->push_back(service);
  }
  return true;
}
}

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
                    "title VARCHAR,"
                    "disposition VARCHAR,"
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

bool WebIntentsTable::GetWebIntents(
    const string16& action,
    std::vector<WebIntentServiceData>* intents) {
  DCHECK(intents);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT service_url, action, type, title, disposition FROM web_intents "
      "WHERE action=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, action);
  return ExtractIntents(&s, intents);
}

bool WebIntentsTable::GetAllWebIntents(
    std::vector<WebIntentServiceData>* intents) {
  DCHECK(intents);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT service_url, action, type, title, disposition FROM web_intents"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  return ExtractIntents(&s, intents);
}

bool WebIntentsTable::SetWebIntent(const WebIntentServiceData& intent) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO web_intents "
      "(service_url, type, action, title, disposition) "
      "VALUES (?, ?, ?, ?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  // Default to window disposition.
  string16 disposition = ASCIIToUTF16("window");
  if (intent.disposition == WebIntentServiceData::DISPOSITION_INLINE)
    disposition = ASCIIToUTF16("inline");
  s.BindString(0, intent.service_url.spec());
  s.BindString16(1, intent.type);
  s.BindString16(2, intent.action);
  s.BindString16(3, intent.title);
  s.BindString16(4, disposition);
  return s.Run();
}

// TODO(jhawkins): Investigate the need to remove rows matching only
// |intent.service_url|. It's unlikely the user will be given the ability to
// remove at the granularity of actions or types.
bool WebIntentsTable::RemoveWebIntent(const WebIntentServiceData& intent) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM web_intents "
      "WHERE service_url = ? AND action = ? AND type = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, intent.service_url.spec());
  s.BindString16(1, intent.action);
  s.BindString16(2, intent.type);
  return s.Run();
}
