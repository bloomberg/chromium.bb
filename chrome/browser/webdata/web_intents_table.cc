// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_intents_table.h"
#include <string>

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"

using webkit_glue::WebIntentServiceData;

namespace {

bool ExtractIntents(sql::Statement* s,
                    std::vector<WebIntentServiceData>* services) {
  DCHECK(s);
  if (!s->is_valid())
    return false;

  while (s->Step()) {
    WebIntentServiceData service;
    service.action = s->ColumnString16(0);
    service.type = s->ColumnString16(1);
    service.scheme = s->ColumnString16(2);
    service.service_url = GURL(s->ColumnString16(3));
    service.title = s->ColumnString16(4);
    // Default to window disposition.
    service.disposition = WebIntentServiceData::DISPOSITION_WINDOW;
    if (s->ColumnString16(5) == ASCIIToUTF16("inline"))
      service.disposition = WebIntentServiceData::DISPOSITION_INLINE;
    services->push_back(service);
  }
  return s->Succeeded();
}

}  // namespace

WebIntentsTable::WebIntentsTable(sql::Connection* db,
                                 sql::MetaTable* meta_table)
    : WebDatabaseTable(db, meta_table) {
}

WebIntentsTable::~WebIntentsTable() {
}

bool WebIntentsTable::Init() {
  if (!db_->DoesTableExist("web_intents")) {
    if (!db_->Execute("CREATE TABLE web_intents ("
                      " service_url LONGVARCHAR,"
                      " action VARCHAR,"
                      " type VARCHAR,"
                      " title LONGVARCHAR,"
                      " disposition VARCHAR,"
                      " scheme VARCHAR,"
                      " UNIQUE (service_url, action, scheme, type))")) {
      return false;
    }
    if (!db_->Execute("CREATE INDEX IF NOT EXISTS web_intents_index"
                      " ON web_intents (action)"))
      return false;
    if (!db_->Execute("CREATE INDEX IF NOT EXISTS web_intents_index"
                      " ON web_intents (scheme)"))
      return false;
  }

  if (!db_->DoesTableExist("web_intents_defaults")) {
    if (!db_->Execute("CREATE TABLE web_intents_defaults ("
                      " action VARCHAR,"
                      " type VARCHAR,"
                      " url_pattern LONGVARCHAR,"
                      " user_date INTEGER,"
                      " suppression INTEGER,"
                      " service_url LONGVARCHAR,"
                      " scheme VARCHAR,"
                      " UNIQUE (action, scheme, type, url_pattern))")) {
      return false;
    }
    if (!db_->Execute("CREATE INDEX IF NOT EXISTS web_intents_default_index"
                      " ON web_intents_defaults (action)"))
      return false;

    if (!db_->Execute("CREATE INDEX IF NOT EXISTS web_intents_default_index"
                      " ON web_intents_defaults (scheme)"))
      return false;
  }

  return true;
}

// TODO(jhawkins): Figure out Sync story.
bool WebIntentsTable::IsSyncable() {
  return false;
}

// Updates the table by way of renaming the old tables, rerunning
// the Init method, then selecting old values into the new tables.
bool WebIntentsTable::MigrateToVersion46AddSchemeColumn() {

  if (!db_->Execute("ALTER TABLE web_intents RENAME TO old_web_intents")) {
    DLOG(WARNING) << "Could not backup web_intents table.";
    return false;
  }

  if (!db_->Execute("ALTER TABLE web_intents_defaults"
                    " RENAME TO old_web_intents_defaults")) {
    DLOG(WARNING) << "Could not backup web_intents_defaults table.";
    return false;
  }

  if (!Init()) return false;

  int error = db_->ExecuteAndReturnErrorCode(
      "INSERT INTO web_intents"
      " (service_url, action, type, title, disposition)"
      " SELECT "
      " service_url, action, type, title, disposition"
      " FROM old_web_intents");

  if (error != SQLITE_OK) {
    DLOG(WARNING) << "Could not copy old intent data to upgraded table."
        << db_->GetErrorMessage();
  }


  error = db_->ExecuteAndReturnErrorCode(
      "INSERT INTO web_intents_defaults"
      " (service_url, action, type, url_pattern, user_date, suppression)"
      " SELECT "
      " service_url, action, type, url_pattern, user_date, suppression"
      " FROM old_web_intents_defaults");

  if (error != SQLITE_OK) {
    DLOG(WARNING) << "Could not copy old intent defaults to upgraded table."
        << db_->GetErrorMessage();
  }

  if (!db_->Execute("DROP table old_web_intents")) {
    LOG(WARNING) << "Could not drop backup web_intents table.";
    return false;
  }

  if (!db_->Execute("DROP table old_web_intents_defaults")) {
    DLOG(WARNING) << "Could not drop backup web_intents_defaults table.";
    return false;
  }

  return true;
}

bool WebIntentsTable::GetWebIntentServices(
    const string16& action,
    std::vector<WebIntentServiceData>* services) {
  DCHECK(services);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT action, type, scheme, service_url, title, disposition"
      " FROM web_intents"
      " WHERE action=?"));
  s.BindString16(0, action);

  return ExtractIntents(&s, services);
}

bool WebIntentsTable::GetWebIntentServicesForScheme(
    const string16& scheme,
    std::vector<WebIntentServiceData>* services) {
  DCHECK(services);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT action, type, scheme, service_url, title, disposition"
      " FROM web_intents"
      " WHERE scheme=?"));
  s.BindString16(0, scheme);

  return ExtractIntents(&s, services);
}

// TODO(gbillock): This currently does a full-table scan. Eventually we will
// store registrations by domain, and so have an indexed origin. At that time,
// this should just change to do lookup by origin instead of URL.
bool WebIntentsTable::GetWebIntentServicesForURL(
    const string16& service_url,
    std::vector<WebIntentServiceData>* services) {
  DCHECK(services);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT action, type, scheme, service_url, title, disposition"
      " FROM web_intents"
      " WHERE service_url=?"));
  s.BindString16(0, service_url);

  return ExtractIntents(&s, services);
}

bool WebIntentsTable::GetAllWebIntentServices(
    std::vector<WebIntentServiceData>* services) {
  DCHECK(services);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT action, type, scheme, service_url, title, disposition"
      " FROM web_intents"));

  return ExtractIntents(&s, services);
}

bool WebIntentsTable::SetWebIntentService(const WebIntentServiceData& service) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO web_intents "
      "(action, type, scheme, service_url, title, disposition) "
      "VALUES (?, ?, ?, ?, ?, ?)"));

  // Default to window disposition.
  string16 disposition = ASCIIToUTF16("window");
  if (service.disposition == WebIntentServiceData::DISPOSITION_INLINE)
    disposition = ASCIIToUTF16("inline");
  s.BindString16(0, service.action);
  s.BindString16(1, service.type);
  s.BindString16(2, service.scheme);
  s.BindString(3, service.service_url.spec());
  s.BindString16(4, service.title);
  s.BindString16(5, disposition);

  return s.Run();
}

// TODO(jhawkins): Investigate the need to remove rows matching only
// |service.service_url|. It's unlikely the user will be given the ability to
// remove at the granularity of actions or types.
bool WebIntentsTable::RemoveWebIntentService(
    const WebIntentServiceData& service) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM web_intents "
      "WHERE action = ? AND type = ? AND scheme = ? AND service_url = ?"));

  s.BindString16(0, service.action);
  s.BindString16(1, service.type);
  s.BindString16(2, service.scheme);
  s.BindString(3, service.service_url.spec());

  return s.Run();
}

bool WebIntentsTable::GetDefaultServices(
    const string16& action,
    std::vector<DefaultWebIntentService>* default_services) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT action, type, url_pattern, user_date, suppression, "
      "service_url FROM web_intents_defaults "
      "WHERE action=?"));
  s.BindString16(0, action);

  while (s.Step()) {
    DefaultWebIntentService entry;
    entry.action = s.ColumnString16(0);
    entry.type = s.ColumnString16(1);
    if (entry.url_pattern.Parse(s.ColumnString(2)) !=
        URLPattern::PARSE_SUCCESS) {
      return false;
    }
    entry.user_date = s.ColumnInt(3);
    entry.suppression = s.ColumnInt64(4);
    entry.service_url = s.ColumnString(5);

    default_services->push_back(entry);
  }

  return s.Succeeded();
}

bool WebIntentsTable::GetAllDefaultServices(
    std::vector<DefaultWebIntentService>* default_services) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT action, type, url_pattern, user_date, suppression, "
      "service_url FROM web_intents_defaults"));

  while (s.Step()) {
    DefaultWebIntentService entry;
    entry.action = s.ColumnString16(0);
    entry.type = s.ColumnString16(1);
    if (entry.url_pattern.Parse(s.ColumnString(2)) !=
        URLPattern::PARSE_SUCCESS) {
      return false;
    }
    entry.user_date = s.ColumnInt(3);
    entry.suppression = s.ColumnInt64(4);
    entry.service_url = s.ColumnString(5);

    default_services->push_back(entry);
  }

  return s.Succeeded();

}

bool WebIntentsTable::SetDefaultService(
    const DefaultWebIntentService& default_service) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO web_intents_defaults "
      "(action, type, url_pattern, user_date, suppression, service_url) "
      "VALUES (?, ?, ?, ?, ?, ?)"));
  s.BindString16(0, default_service.action);
  s.BindString16(1, default_service.type);
  s.BindString(2, default_service.url_pattern.GetAsString());
  s.BindInt(3, default_service.user_date);
  s.BindInt64(4, default_service.suppression);
  s.BindString(5, default_service.service_url);

  return s.Run();
}

bool WebIntentsTable::RemoveDefaultService(
    const DefaultWebIntentService& default_service) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM web_intents_defaults "
      "WHERE action = ? AND type = ? AND url_pattern = ?"));
  s.BindString16(0, default_service.action);
  s.BindString16(1, default_service.type);
  s.BindString(2, default_service.url_pattern.GetAsString());

  return s.Run();
}

bool WebIntentsTable::RemoveServiceDefaults(const GURL& service_url) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM web_intents_defaults WHERE service_url = ?"));
  s.BindString(0, service_url.spec());

  return s.Run();
}
