// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/keyword_table.h"

#include <set>

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
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/webdata/web_database.h"
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
    "prepopulate_id, created_by_policy, instant_url, last_modified, sync_guid";

namespace {

// Keys used in the meta table.
const char kBuiltinKeywordVersion[] = "Builtin Keyword Version";

// The set of columns up through version 44.  (There were different columns
// below version 29 but none of the code below needs to worry about that case.)
const char kKeywordColumnsVersion44Concatenated[] = "id || short_name || "
    "keyword || favicon_url || url || safe_for_autoreplace || "
    "originating_url || date_created || usage_count || input_encodings || "
    "show_in_default_list || suggest_url || prepopulate_id || "
    "autogenerate_keyword || logo_id || created_by_policy || instant_url || "
    "last_modified || sync_guid";
const char kKeywordColumnsVersion44[] = "id, short_name, keyword, favicon_url, "
    "url, safe_for_autoreplace, originating_url, date_created, usage_count, "
    "input_encodings, show_in_default_list, suggest_url, prepopulate_id, "
    "autogenerate_keyword, logo_id, created_by_policy, instant_url, "
    "last_modified, sync_guid";
// NOTE: Remember to change what |kKeywordColumnsVersion45| says if the column
// set in |kKeywordColumns| changes, and update any code that needs to switch
// column sets based on a version number!
const char* const kKeywordColumnsVersion45 = KeywordTable::kKeywordColumns;

// The current columns.
const char kKeywordColumnsConcatenated[] = "id || short_name || keyword || "
    "favicon_url || url || safe_for_autoreplace || originating_url || "
    "date_created || usage_count || input_encodings || show_in_default_list || "
    "suggest_url || prepopulate_id || created_by_policy || instant_url || "
    "last_modified || sync_guid";

// Inserts the data from |data| into |s|.  |s| is assumed to have slots for all
// the columns in the keyword table.  |id_column| is the slot number to bind
// |data|'s |id| to; |starting_column| is the slot number of the first of a
// contiguous set of slots to bind all the other fields to.
void BindURLToStatement(const TemplateURLData& data,
                        sql::Statement* s,
                        int id_column,
                        int starting_column) {
  s->BindInt64(id_column, data.id);
  s->BindString16(starting_column, data.short_name);
  s->BindString16(starting_column + 1, data.keyword());
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
  s->BindBool(starting_column + 12, data.created_by_policy);
  s->BindString(starting_column + 13, data.instant_url);
  s->BindInt64(starting_column + 14, data.last_modified.ToTimeT());
  s->BindString(starting_column + 15, data.sync_guid);
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
                    "created_by_policy INTEGER DEFAULT 0,"
                    "instant_url VARCHAR,"
                    "last_modified INTEGER DEFAULT 0,"
                    "sync_guid VARCHAR)") &&
       UpdateBackupSignature(WebDatabase::kCurrentVersionNumber));
}

bool KeywordTable::IsSyncable() {
  return true;
}

bool KeywordTable::AddKeyword(const TemplateURLData& data) {
  DCHECK(data.id);
  std::string query("INSERT INTO keywords (" + std::string(kKeywordColumns) +
                    ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
  sql::Statement s(db_->GetUniqueStatement(query.c_str()));
  BindURLToStatement(data, &s, 0, 1);

  return s.Run() && UpdateBackupSignature(WebDatabase::kCurrentVersionNumber);
}

bool KeywordTable::RemoveKeyword(TemplateURLID id) {
  DCHECK(id);
  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM keywords WHERE id = ?"));
  s.BindInt64(0, id);

  return s.Run() && UpdateBackupSignature(WebDatabase::kCurrentVersionNumber);
}

bool KeywordTable::GetKeywords(Keywords* keywords) {
  std::string query("SELECT " + std::string(kKeywordColumns) +
                    " FROM keywords ORDER BY id ASC");
  sql::Statement s(db_->GetUniqueStatement(query.c_str()));

  std::set<TemplateURLID> bad_entries;
  while (s.Step()) {
    keywords->push_back(TemplateURLData());
    if (!GetKeywordDataFromStatement(s, &keywords->back())) {
      bad_entries.insert(s.ColumnInt64(0));
      keywords->pop_back();
    }
  }
  bool succeeded = s.Succeeded();
  for (std::set<TemplateURLID>::const_iterator i(bad_entries.begin());
       i != bad_entries.end(); ++i)
    succeeded &= RemoveKeyword(*i);
  return succeeded;
}

bool KeywordTable::UpdateKeyword(const TemplateURLData& data) {
  DCHECK(data.id);
  sql::Statement s(db_->GetUniqueStatement("UPDATE keywords SET short_name=?, "
      "keyword=?, favicon_url=?, url=?, safe_for_autoreplace=?, "
      "originating_url=?, date_created=?, usage_count=?, input_encodings=?, "
      "show_in_default_list=?, suggest_url=?, prepopulate_id=?, "
      "created_by_policy=?, instant_url=?, last_modified=?, sync_guid=? WHERE "
      "id=?"));
  BindURLToStatement(data, &s, 16, 0);  // "16" binds id() as the last item.

  return s.Run() && UpdateBackupSignature(WebDatabase::kCurrentVersionNumber);
}

bool KeywordTable::SetDefaultSearchProviderID(int64 id) {
  // Added for http://crbug.com/116952.
  UMA_HISTOGRAM_COUNTS_100("Search.DefaultSearchProviderID",
                           static_cast<int32>(id));
  return meta_table_->SetValue(kDefaultSearchProviderKey, id) &&
      UpdateBackupSignature(WebDatabase::kCurrentVersionNumber);
}

int64 KeywordTable::GetDefaultSearchProviderID() {
  int64 value = kInvalidTemplateURLID;
  meta_table_->GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

bool KeywordTable::GetDefaultSearchProviderBackup(TemplateURLData* backup) {
  if (!IsBackupSignatureValid(WebDatabase::kCurrentVersionNumber))
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
    return false;
  }

  if (!GetKeywordDataFromStatement(s, backup))
    return false;

  // ID has no meaning for the backup and should be kInvalidTemplateURLID in
  // case the TemplateURL will be added to keywords if missing.
  backup->id = kInvalidTemplateURLID;
  return true;
}

bool KeywordTable::DidDefaultSearchProviderChange() {
  if (!IsBackupSignatureValid(WebDatabase::kCurrentVersionNumber)) {
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
  return IsBackupSignatureValid(44) || UpdateBackupSignature(44);
}

bool KeywordTable::MigrateToVersion45RemoveLogoIDAndAutogenerateColumns() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // The version 43 migration should have been written to do this, but since it
  // wasn't, we'll do it now.  Unfortunately a previous change deleted this for
  // some users, so we can't be sure this will succeed (so don't bail on error).
  meta_table_->DeleteKey("Default Search Provider Backup");

  if (!MigrateKeywordsTableForVersion45("keywords"))
    return false;

  if (IsBackupSignatureValid(44)) {
    // Migrate the keywords backup table as well.
    if (!MigrateKeywordsTableForVersion45("keywords_backup") || !SignBackup(45))
      return false;
  } else {
    // Old backup was invalid; drop the table entirely, which will trigger the
    // protector code to prompt the user and recreate the table.
    if (db_->DoesTableExist("keywords_backup") &&
        !db_->Execute("DROP TABLE keywords_backup"))
      return false;
  }

  return transaction.Commit();
}

// static
bool KeywordTable::GetKeywordDataFromStatement(const sql::Statement& s,
                                               TemplateURLData* data) {
  DCHECK(data);
  data->short_name = s.ColumnString16(1);
  data->SetKeyword(s.ColumnString16(2));
  // Due to past bugs, we might have persisted entries with empty URLs.  Avoid
  // reading these out.  (GetKeywords() will delete these entries on return.)
  // NOTE: This code should only be needed as long as we might be reading such
  // potentially-old data and can be removed afterward.
  if (s.ColumnString(4).empty())
    return false;
  data->SetURL(s.ColumnString(4));
  data->suggestions_url = s.ColumnString(11);
  data->instant_url = s.ColumnString(14);
  data->favicon_url = GURL(s.ColumnString(3));
  data->originating_url = GURL(s.ColumnString(6));
  data->show_in_default_list = s.ColumnBool(10);
  data->safe_for_autoreplace = s.ColumnBool(5);
  base::SplitString(s.ColumnString(9), ';', &data->input_encodings);
  data->id = s.ColumnInt64(0);
  data->date_created = Time::FromTimeT(s.ColumnInt64(7));
  data->last_modified = Time::FromTimeT(s.ColumnInt64(15));
  data->created_by_policy = s.ColumnBool(13);
  data->usage_count = s.ColumnInt(8);
  data->prepopulate_id = s.ColumnInt(12);
  data->sync_guid = s.ColumnString(16);
  return true;
}

bool KeywordTable::GetSignatureData(int table_version, std::string* backup) {
  DCHECK(backup);

  int64 backup_value = kInvalidTemplateURLID;
  if (!meta_table_->GetValue(kDefaultSearchIDBackupKey, &backup_value)) {
    LOG(ERROR) << "No backup id for signing.";
    return false;
  }

  std::string keywords_backup_data;
  if (!GetTableContents("keywords_backup", table_version,
                        &keywords_backup_data)) {
    LOG(ERROR) << "Can't get keywords backup data";
    return false;
  }
  *backup = base::Int64ToString(backup_value) + keywords_backup_data;
  return true;
}

bool KeywordTable::GetTableContents(const char* table_name,
                                    int table_version,
                                    std::string* contents) {
  DCHECK(contents);

  if (!db_->DoesTableExist(table_name))
    return false;

  contents->clear();
  std::string query("SELECT " +
      std::string((table_version <= 44) ?
          kKeywordColumnsVersion44Concatenated : kKeywordColumnsConcatenated) +
      " FROM " + std::string(table_name) + " ORDER BY id ASC");
  sql::Statement s((table_version == WebDatabase::kCurrentVersionNumber) ?
      db_->GetCachedStatement(sql::StatementID(table_name), query.c_str()) :
      db_->GetUniqueStatement(query.c_str()));
  while (s.Step())
    *contents += s.ColumnString(0);
  return s.Succeeded();
}

bool KeywordTable::UpdateBackupSignature(int table_version) {
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
      std::string((table_version <= 44) ?
          kKeywordColumnsVersion44 : kKeywordColumns) +
      " FROM keywords ORDER BY id ASC");
  if (!db_->Execute(query.c_str())) {
    LOG(ERROR) << "Failed to create keywords_backup table.";
    return false;
  }

  return SignBackup(table_version) && transaction.Commit();
}

bool KeywordTable::SignBackup(int table_version) {
  std::string data_to_sign;
  if (!GetSignatureData(table_version, &data_to_sign)) {
    LOG(ERROR) << "No data to sign.";
    return false;
  }

  std::string signature = protector::SignSetting(data_to_sign);
  if (signature.empty()) {
    LOG(ERROR) << "Signature is empty";
    return false;
  }

  return meta_table_->SetValue(kBackupSignatureKey, signature);
}

bool KeywordTable::IsBackupSignatureValid(int table_version) {
  std::string signature;
  std::string signature_data;
  return meta_table_->GetValue(kBackupSignatureKey, &signature) &&
         GetSignatureData(table_version, &signature_data) &&
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

bool KeywordTable::MigrateKeywordsTableForVersion45(const std::string& name) {
  // Create a new table without the columns we're dropping.
  if (!db_->Execute("CREATE TABLE keywords_temp ("
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
                    "created_by_policy INTEGER DEFAULT 0,"
                    "instant_url VARCHAR,"
                    "last_modified INTEGER DEFAULT 0,"
                    "sync_guid VARCHAR)"))
    return false;
  std::string sql("INSERT INTO keywords_temp SELECT " +
                  std::string(kKeywordColumnsVersion45) + " FROM " + name);
  if (!db_->Execute(sql.c_str()))
    return false;

  // NOTE: The ORDER BY here ensures that the uniquing process for keywords will
  // happen identically on both the normal and backup tables.
  sql = "SELECT id, keyword, url, autogenerate_keyword FROM " + name +
      " ORDER BY id ASC";
  sql::Statement s(db_->GetUniqueStatement(sql.c_str()));
  string16 placeholder_keyword(ASCIIToUTF16("dummy"));
  std::set<string16> keywords;
  while (s.Step()) {
    string16 keyword(s.ColumnString16(1));
    bool generate_keyword = keyword.empty() || s.ColumnBool(3);
    if (generate_keyword)
      keyword = placeholder_keyword;
    TemplateURLData data;
    data.SetKeyword(keyword);
    data.SetURL(s.ColumnString(2));
    TemplateURL turl(NULL, data);
    // Don't persist extension keywords to disk.  These will get added to the
    // TemplateURLService as the extensions are loaded.
    bool delete_entry = turl.IsExtensionKeyword();
    if (!delete_entry && generate_keyword) {
      // Explicitly generate keywords for all rows with the autogenerate bit set
      // or where the keyword is empty.
      SearchTermsData terms_data;
      GURL url(TemplateURLService::GenerateSearchURLUsingTermsData(&turl,
                                                                   terms_data));
      if (!url.is_valid()) {
        delete_entry = true;
      } else {
        // Ensure autogenerated keywords are unique.
        keyword = TemplateURLService::GenerateKeyword(url);
        while (keywords.count(keyword))
          keyword.append(ASCIIToUTF16("_"));
        sql::Statement u(db_->GetUniqueStatement(
            "UPDATE keywords_temp SET keyword=? WHERE id=?"));
        u.BindString16(0, keyword);
        u.BindInt64(1, s.ColumnInt64(0));
        if (!u.Run())
          return false;
      }
    }
    if (delete_entry) {
      sql::Statement u(db_->GetUniqueStatement(
          "DELETE FROM keywords_temp WHERE id=?"));
      u.BindInt64(0, s.ColumnInt64(0));
      if (!u.Run())
        return false;
    } else {
      keywords.insert(keyword);
    }
  }

  // Replace the old table with the new one.
  sql = "DROP TABLE " + name;
  if (!db_->Execute(sql.c_str()))
    return false;
  sql = "ALTER TABLE keywords_temp RENAME TO " + name;
  return db_->Execute(sql.c_str());
}
