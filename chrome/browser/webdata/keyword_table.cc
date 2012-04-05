// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/keyword_table.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/protector/histograms.h"
#include "chrome/browser/protector/protector_utils.h"
#include "chrome/browser/search_engines/template_url.h"
#include "googleurl/src/gurl.h"
#include "sql/statement.h"
#include "sql/transaction.h"

using base::Time;

// static
const char KeywordTable::kDefaultSearchProviderKey[] =
    "Default Search Provider ID";
const char KeywordTable::kDefaultSearchIDBackupKey[] =
    "Default Search Provider ID Backup";
const char KeywordTable::kBackupSignatureKey[] =
    "Default Search Provider ID Backup Signature";
const char KeywordTable::kKeywordColumns[] = "id, short_name, keyword, "
    "favicon_url, url, safe_for_autoreplace, originating_url, date_created, "
    "usage_count, input_encodings, show_in_default_list, suggest_url, "
    "prepopulate_id, autogenerate_keyword, logo_id, created_by_policy, "
    "instant_url, last_modified, sync_guid";

namespace {

// Keys used in the meta table.
const char kBuiltinKeywordVersion[] = "Builtin Keyword Version";

const char kKeywordColumnsConcatenated[] = "id || short_name || keyword || "
    "favicon_url || url || safe_for_autoreplace || originating_url || "
    "date_created || usage_count || input_encodings || show_in_default_list || "
    "suggest_url || prepopulate_id || autogenerate_keyword || logo_id || "
    "created_by_policy || instant_url || last_modified || sync_guid";

// Inserts the data from |data| into |s|.  |s| is assumed to have slots for all
// the columns in the keyword table.  |id_column| is the slot number to bind
// |data|'s id() to; |starting_column| is the slot number of the first of a
// contiguous set of slots to bind all the other fields to.
void BindURLToStatement(const TemplateURL& url,
                        sql::Statement* s,
                        int id_column,
                        int starting_column) {
  const TemplateURLData& data = url.data();
  s->BindInt64(id_column, data.id);
  s->BindString16(starting_column, data.short_name);
  s->BindString16(starting_column + 1, data.keyword(&url));
  s->BindString(starting_column + 2, data.favicon_url.is_valid() ?
      history::HistoryDatabase::GURLToDatabaseURL(data.favicon_url) :
      std::string());
  s->BindString(starting_column + 3, data.url());
  s->BindBool(starting_column + 4, data.safe_for_autoreplace);
  s->BindString(starting_column + 5, data.originating_url.is_valid() ?
      history::HistoryDatabase::GURLToDatabaseURL(data.originating_url) :
      std::string());
  s->BindInt64(starting_column + 6, data.date_created.ToTimeT());
  s->BindInt(starting_column + 7, data.usage_count);
  s->BindString(starting_column + 8, JoinString(data.input_encodings, ';'));
  s->BindBool(starting_column + 9, data.show_in_default_list);
  s->BindString(starting_column + 10, data.suggestions_url);
  s->BindInt(starting_column + 11, data.prepopulate_id);
  s->BindBool(starting_column + 12, data.autogenerate_keyword());
  s->BindInt(starting_column + 13, 0);
  s->BindBool(starting_column + 14, data.created_by_policy);
  s->BindString(starting_column + 15, data.instant_url);
  s->BindInt64(starting_column + 16, data.last_modified.ToTimeT());
  s->BindString(starting_column + 17, data.sync_guid);
}

// Signs search provider id and returns its signature.
std::string GetSearchProviderIDSignature(int64 id) {
  return protector::SignSetting(base::Int64ToString(id));
}

// Checks if signature for search provider id is correct and returns the
// result.
bool IsSearchProviderIDValid(int64 id, const std::string& signature) {
  return protector::IsSettingValid(base::Int64ToString(id), signature);
}

}  // anonymous namespace

KeywordTable::~KeywordTable() {}

bool KeywordTable::Init() {
  return db_->DoesTableExist("keywords") ||
      (db_->Execute("CREATE TABLE keywords ("
                    "id INTEGER PRIMARY KEY,"
                    "short_name VARCHAR NOT NULL,"
                    "keyword VARCHAR NOT NULL,"
                    "favicon_url VARCHAR NOT NULL,"
                    "url VARCHAR NOT NULL,"
                    "safe_for_autoreplace INTEGER,"
                    "originating_url VARCHAR,"
                    "date_created INTEGER DEFAULT 0,"
                    "usage_count INTEGER DEFAULT 0,"
                    "input_encodings VARCHAR,"
                    "show_in_default_list INTEGER,"
                    "suggest_url VARCHAR,"
                    "prepopulate_id INTEGER DEFAULT 0,"
                    "autogenerate_keyword INTEGER DEFAULT 0,"
                    "logo_id INTEGER DEFAULT 0,"
                    "created_by_policy INTEGER DEFAULT 0,"
                    "instant_url VARCHAR,"
                    "last_modified INTEGER DEFAULT 0,"
                    "sync_guid VARCHAR)") &&
       UpdateBackupSignature());
}

bool KeywordTable::IsSyncable() {
  return true;
}

bool KeywordTable::AddKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  std::string query("INSERT INTO keywords (" + std::string(kKeywordColumns) +
                    ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
  sql::Statement s(db_->GetUniqueStatement(query.c_str()));
  BindURLToStatement(url, &s, 0, 1);

  return s.Run() && UpdateBackupSignature();
}

bool KeywordTable::RemoveKeyword(TemplateURLID id) {
  DCHECK(id);
  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM keywords WHERE id = ?"));
  s.BindInt64(0, id);

  return s.Run() && UpdateBackupSignature();
}

bool KeywordTable::GetKeywords(Keywords* keywords) {
  std::string query("SELECT " + std::string(kKeywordColumns) +
                    " FROM keywords ORDER BY id ASC");
  sql::Statement s(db_->GetUniqueStatement(query.c_str()));

  while (s.Step()) {
    keywords->push_back(TemplateURLData());
    GetKeywordDataFromStatement(s, &keywords->back());
  }
  return s.Succeeded();
}

bool KeywordTable::UpdateKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  sql::Statement s(db_->GetUniqueStatement("UPDATE keywords SET short_name=?, "
      "keyword=?, favicon_url=?, url=?, safe_for_autoreplace=?, "
      "originating_url=?, date_created=?, usage_count=?, input_encodings=?, "
      "show_in_default_list=?, suggest_url=?, prepopulate_id=?, "
      "autogenerate_keyword=?, logo_id=?, created_by_policy=?, instant_url=?, "
      "last_modified=?, sync_guid=? WHERE id=?"));
  BindURLToStatement(url, &s, 18, 0);  // "18" binds id() as the last item.

  return s.Run() && UpdateBackupSignature();
}

bool KeywordTable::SetDefaultSearchProviderID(int64 id) {
  return meta_table_->SetValue(kDefaultSearchProviderKey, id) &&
      UpdateBackupSignature();
}

int64 KeywordTable::GetDefaultSearchProviderID() {
  int64 value = kInvalidTemplateURLID;
  meta_table_->GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

bool KeywordTable::GetDefaultSearchProviderBackup(TemplateURLData* backup) {
  if (!IsBackupSignatureValid())
    return false;

  int64 backup_id = kInvalidTemplateURLID;
  if (!meta_table_->GetValue(kDefaultSearchIDBackupKey, &backup_id)) {
    LOG(ERROR) << "No default search id backup found.";
    return false;
  }
  std::string query("SELECT " + std::string(kKeywordColumns) +
                    " FROM keywords_backup WHERE id=?");
  sql::Statement s(db_->GetUniqueStatement(query.c_str()));
  s.BindInt64(0, backup_id);

  if (!s.Step()) {
    LOG_IF(ERROR, s.Succeeded())
        << "No default search provider with backup id.";
    return NULL;
  }

  GetKeywordDataFromStatement(s, backup);
  // ID has no meaning for the backup and should be kInvalidTemplateURLID in
  // case the TemplateURL will be added to keywords if missing.
  backup->id = kInvalidTemplateURLID;
  return true;
}

bool KeywordTable::DidDefaultSearchProviderChange() {
  if (!IsBackupSignatureValid()) {
    UMA_HISTOGRAM_ENUMERATION(
        protector::kProtectorHistogramDefaultSearchProvider,
        protector::kProtectorErrorBackupInvalid,
        protector::kProtectorErrorCount);
    LOG(ERROR) << "Backup signature is invalid.";
    return true;
  }

  int64 backup_id = kInvalidTemplateURLID;
  meta_table_->GetValue(kDefaultSearchIDBackupKey, &backup_id);
  int64 current_id = GetDefaultSearchProviderID();
  if (backup_id == current_id) {
    // Either this is a new profile and both IDs are kInvalidTemplateURLID or
    // the search engines with the ids are equal.
    if (backup_id == kInvalidTemplateURLID) {
      UMA_HISTOGRAM_ENUMERATION(
          protector::kProtectorHistogramDefaultSearchProvider,
          protector::kProtectorErrorValueValidZero,
          protector::kProtectorErrorCount);
      return false;
    }
    std::string backup_url;
    std::string current_url;
    if (GetKeywordAsString(backup_id, "keywords_backup", &backup_url) &&
        GetKeywordAsString(current_id, "keywords", &current_url) &&
        current_url == backup_url) {
      UMA_HISTOGRAM_ENUMERATION(
          protector::kProtectorHistogramDefaultSearchProvider,
          protector::kProtectorErrorValueValid,
          protector::kProtectorErrorCount);
      return false;
    }
  }

  UMA_HISTOGRAM_ENUMERATION(
      protector::kProtectorHistogramDefaultSearchProvider,
      protector::kProtectorErrorValueChanged,
      protector::kProtectorErrorCount);
  LOG(WARNING) << "Default Search Provider has changed.";
  return true;
}

bool KeywordTable::SetBuiltinKeywordVersion(int version) {
  return meta_table_->SetValue(kBuiltinKeywordVersion, version);
}

int KeywordTable::GetBuiltinKeywordVersion() {
  int version = 0;
  return meta_table_->GetValue(kBuiltinKeywordVersion, &version) ? version : 0;
}

bool KeywordTable::MigrateToVersion21AutoGenerateKeywordColumn() {
  return db_->Execute("ALTER TABLE keywords ADD COLUMN autogenerate_keyword "
                      "INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion25AddLogoIDColumn() {
  return db_->Execute(
      "ALTER TABLE keywords ADD COLUMN logo_id INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion26AddCreatedByPolicyColumn() {
  return db_->Execute("ALTER TABLE keywords ADD COLUMN created_by_policy "
                      "INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion28SupportsInstantColumn() {
  return db_->Execute("ALTER TABLE keywords ADD COLUMN supports_instant "
                      "INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion29InstantUrlToSupportsInstant() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
      db_->Execute("ALTER TABLE keywords ADD COLUMN instant_url VARCHAR") &&
      db_->Execute("CREATE TABLE keywords_temp ("
                   "id INTEGER PRIMARY KEY,"
                   "short_name VARCHAR NOT NULL,"
                   "keyword VARCHAR NOT NULL,"
                   "favicon_url VARCHAR NOT NULL,"
                   "url VARCHAR NOT NULL,"
                   "safe_for_autoreplace INTEGER,"
                   "originating_url VARCHAR,"
                   "date_created INTEGER DEFAULT 0,"
                   "usage_count INTEGER DEFAULT 0,"
                   "input_encodings VARCHAR,"
                   "show_in_default_list INTEGER,"
                   "suggest_url VARCHAR,"
                   "prepopulate_id INTEGER DEFAULT 0,"
                   "autogenerate_keyword INTEGER DEFAULT 0,"
                   "logo_id INTEGER DEFAULT 0,"
                   "created_by_policy INTEGER DEFAULT 0,"
                   "instant_url VARCHAR)") &&
      db_->Execute("INSERT INTO keywords_temp SELECT id, short_name, keyword, "
                   "favicon_url, url, safe_for_autoreplace, originating_url, "
                   "date_created, usage_count, input_encodings, "
                   "show_in_default_list, suggest_url, prepopulate_id, "
                   "autogenerate_keyword, logo_id, created_by_policy, "
                   "instant_url FROM keywords") &&
      db_->Execute("DROP TABLE keywords") &&
      db_->Execute("ALTER TABLE keywords_temp RENAME TO keywords") &&
      transaction.Commit();
}

bool KeywordTable::MigrateToVersion38AddLastModifiedColumn() {
  return db_->Execute(
      "ALTER TABLE keywords ADD COLUMN last_modified INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion39AddSyncGUIDColumn() {
  return db_->Execute("ALTER TABLE keywords ADD COLUMN sync_guid VARCHAR");
}

bool KeywordTable::MigrateToVersion44AddDefaultSearchProviderBackup() {
  return IsBackupSignatureValid() || UpdateBackupSignature();
}

// static
void KeywordTable::GetKeywordDataFromStatement(const sql::Statement& s,
                                               TemplateURLData* data) {
  DCHECK(data);
  data->short_name = s.ColumnString16(1);
  data->SetKeyword(s.ColumnString16(2));
  data->SetAutogenerateKeyword(s.ColumnBool(13));
  data->SetURL(s.ColumnString(4));
  data->suggestions_url = s.ColumnString(11);
  data->instant_url = s.ColumnString(16);
  data->favicon_url = GURL(s.ColumnString(3));
  data->originating_url = GURL(s.ColumnString(6));
  data->show_in_default_list = s.ColumnBool(10);
  data->safe_for_autoreplace = s.ColumnBool(5);
  base::SplitString(s.ColumnString(9), ';', &data->input_encodings);
  data->id = s.ColumnInt64(0);
  data->date_created = Time::FromTimeT(s.ColumnInt64(7));
  data->last_modified = Time::FromTimeT(s.ColumnInt64(17));
  data->created_by_policy = s.ColumnBool(15);
  data->usage_count = s.ColumnInt(8);
  data->prepopulate_id = s.ColumnInt(12);
  data->sync_guid = s.ColumnString(18);
}

bool KeywordTable::GetSignatureData(std::string* backup) {
  DCHECK(backup);

  int64 backup_value = kInvalidTemplateURLID;
  if (!meta_table_->GetValue(kDefaultSearchIDBackupKey, &backup_value)) {
    LOG(ERROR) << "No backup id for signing.";
    return false;
  }

  std::string keywords_backup_data;
  if (!GetTableContents("keywords_backup", &keywords_backup_data)) {
    LOG(ERROR) << "Can't get keywords backup data";
    return false;
  }
  *backup = base::Int64ToString(backup_value) + keywords_backup_data;
  return true;
}

bool KeywordTable::GetTableContents(const char* table_name,
                                    std::string* contents) {
  DCHECK(contents);

  if (!db_->DoesTableExist(table_name))
    return false;

  contents->clear();
  std::string query("SELECT " + std::string(kKeywordColumnsConcatenated) +
      " FROM " + std::string(table_name) + " ORDER BY id ASC");
  sql::Statement s(db_->GetCachedStatement(sql::StatementID(table_name),
                                           query.c_str()));
  while (s.Step())
    *contents += s.ColumnString(0);
  return s.Succeeded();
}

bool KeywordTable::UpdateBackupSignature() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  int64 id = kInvalidTemplateURLID;
  if (!UpdateDefaultSearchProviderIDBackup(&id)) {
    LOG(ERROR) << "Failed to update default search id backup.";
    return false;
  }

  // Backup of all keywords.
  if (db_->DoesTableExist("keywords_backup") &&
      !db_->Execute("DROP TABLE keywords_backup"))
    return false;

  std::string query("CREATE TABLE keywords_backup AS SELECT " +
      std::string(kKeywordColumns) + " FROM keywords ORDER BY id ASC");
  if (!db_->Execute(query.c_str())) {
    LOG(ERROR) << "Failed to create keywords_backup table.";
    return false;
  }

  std::string data_to_sign;
  if (!GetSignatureData(&data_to_sign)) {
    LOG(ERROR) << "No data to sign.";
    return false;
  }

  std::string signature = protector::SignSetting(data_to_sign);
  if (signature.empty()) {
    LOG(ERROR) << "Signature is empty";
    return false;
  }

  return meta_table_->SetValue(kBackupSignatureKey, signature) &&
      transaction.Commit();
}

bool KeywordTable::IsBackupSignatureValid() {
  std::string signature;
  std::string signature_data;
  return meta_table_->GetValue(kBackupSignatureKey, &signature) &&
         GetSignatureData(&signature_data) &&
         protector::IsSettingValid(signature_data, signature);
}

bool KeywordTable::GetKeywordAsString(TemplateURLID id,
                                      const std::string& table_name,
                                      std::string* result) {
  std::string query("SELECT " + std::string(kKeywordColumnsConcatenated) +
      " FROM " + table_name + " WHERE id=?");
  sql::Statement s(db_->GetUniqueStatement(query.c_str()));
  s.BindInt64(0, id);

  if (!s.Step()) {
    LOG_IF(WARNING, s.Succeeded()) << "No keyword with id: " << id
                                   << ", ignoring.";
    return true;
  }

  if (!s.Succeeded())
    return false;

  *result = s.ColumnString(0);
  return true;
}

bool KeywordTable::UpdateDefaultSearchProviderIDBackup(TemplateURLID* id) {
  DCHECK(id);
  int64 default_search_id = GetDefaultSearchProviderID();
  if (!meta_table_->SetValue(kDefaultSearchIDBackupKey,
                             default_search_id)) {
    LOG(ERROR) << "Can't write default search id backup.";
    return false;
  }

  *id = default_search_id;
  return true;
}
