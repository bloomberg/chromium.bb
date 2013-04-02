// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/keyword_table.h"

#include <set>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_database.h"
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

namespace {

// Keys used in the meta table.
const char kBuiltinKeywordVersion[] = "Builtin Keyword Version";

const std::string ColumnsForVersion(int version, bool concatenated) {
  std::vector<std::string> columns;

  columns.push_back("id");
  columns.push_back("short_name");
  columns.push_back("keyword");
  columns.push_back("favicon_url");
  columns.push_back("url");
  columns.push_back("safe_for_autoreplace");
  columns.push_back("originating_url");
  columns.push_back("date_created");
  columns.push_back("usage_count");
  columns.push_back("input_encodings");
  columns.push_back("show_in_default_list");
  columns.push_back("suggest_url");
  columns.push_back("prepopulate_id");
  if (version <= 44) {
    // Columns removed after version 44.
    columns.push_back("autogenerate_keyword");
    columns.push_back("logo_id");
  }
  columns.push_back("created_by_policy");
  columns.push_back("instant_url");
  columns.push_back("last_modified");
  columns.push_back("sync_guid");
  if (version >= 47) {
    // Column added in version 47.
    columns.push_back("alternate_urls");
  }
  if (version >= 49) {
    // Column added in version 49.
    columns.push_back("search_terms_replacement_key");
  }

  return JoinString(columns, std::string(concatenated ? " || " : ", "));
}


// Inserts the data from |data| into |s|.  |s| is assumed to have slots for all
// the columns in the keyword table.  |id_column| is the slot number to bind
// |data|'s |id| to; |starting_column| is the slot number of the first of a
// contiguous set of slots to bind all the other fields to.
void BindURLToStatement(const TemplateURLData& data,
                        sql::Statement* s,
                        int id_column,
                        int starting_column) {
  // Serialize |alternate_urls| to JSON.
  // TODO(beaudoin): Check what it would take to use a new table to store
  // alternate_urls while keeping backups and table signature in a good state.
  // See: crbug.com/153520
  ListValue alternate_urls_value;
  for (size_t i = 0; i < data.alternate_urls.size(); ++i)
    alternate_urls_value.AppendString(data.alternate_urls[i]);
  std::string alternate_urls;
  base::JSONWriter::Write(&alternate_urls_value, &alternate_urls);

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
  s->BindString(starting_column + 16, alternate_urls);
  s->BindString(starting_column + 17, data.search_terms_replacement_key);
}

int table_key = 0;

WebDatabaseTable::TypeKey GetKey() {
  return reinterpret_cast<void*>(&table_key);
}

}  // namespace

KeywordTable::KeywordTable() {
}

KeywordTable::~KeywordTable() {}

KeywordTable* KeywordTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<KeywordTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey KeywordTable::GetTypeKey() const {
  return GetKey();
}

bool KeywordTable::Init(sql::Connection* db, sql::MetaTable* meta_table) {
  WebDatabaseTable::Init(db, meta_table);
  return db_->DoesTableExist("keywords") ||
      db_->Execute("CREATE TABLE keywords ("
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
                   "sync_guid VARCHAR,"
                   "alternate_urls VARCHAR,"
                   "search_terms_replacement_key VARCHAR)");
}

bool KeywordTable::IsSyncable() {
  return true;
}

bool KeywordTable::MigrateToVersion(int version,
                                    bool* update_compatible_version) {
  // Migrate if necessary.
  switch (version) {
    case 21:
      *update_compatible_version = true;
      return MigrateToVersion21AutoGenerateKeywordColumn();
    case 25:
      *update_compatible_version = true;
      return MigrateToVersion25AddLogoIDColumn();
    case 26:
      *update_compatible_version = true;
      return MigrateToVersion26AddCreatedByPolicyColumn();
    case 28:
      *update_compatible_version = true;
      return MigrateToVersion28SupportsInstantColumn();
    case 29:
      *update_compatible_version = true;
      return MigrateToVersion29InstantURLToSupportsInstant();
    case 38:
      *update_compatible_version = true;
      return MigrateToVersion38AddLastModifiedColumn();
    case 39:
      *update_compatible_version = true;
      return MigrateToVersion39AddSyncGUIDColumn();
    case 44:
      *update_compatible_version = true;
      return MigrateToVersion44AddDefaultSearchProviderBackup();
    case 45:
      *update_compatible_version = true;
      return MigrateToVersion45RemoveLogoIDAndAutogenerateColumns();
    case 47:
      *update_compatible_version = true;
      return MigrateToVersion47AddAlternateURLsColumn();
    case 48:
      *update_compatible_version = true;
      return MigrateToVersion48RemoveKeywordsBackup();
    case 49:
      *update_compatible_version = true;
      return MigrateToVersion49AddSearchTermsReplacementKeyColumn();
  }

  return true;
}

bool KeywordTable::AddKeyword(const TemplateURLData& data) {
  DCHECK(data.id);
  std::string query("INSERT INTO keywords (" + GetKeywordColumns() +
                    ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
  sql::Statement s(db_->GetUniqueStatement(query.c_str()));
  BindURLToStatement(data, &s, 0, 1);

  return s.Run();
}

bool KeywordTable::RemoveKeyword(TemplateURLID id) {
  DCHECK(id);
  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM keywords WHERE id = ?"));
  s.BindInt64(0, id);

  return s.Run();
}

bool KeywordTable::GetKeywords(Keywords* keywords) {
  std::string query("SELECT " + GetKeywordColumns() +
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
      "created_by_policy=?, instant_url=?, last_modified=?, sync_guid=?, "
      "alternate_urls=?, search_terms_replacement_key=? WHERE id=?"));
  BindURLToStatement(data, &s, 18, 0);  // "18" binds id() as the last item.

  return s.Run();
}

bool KeywordTable::SetDefaultSearchProviderID(int64 id) {
  return meta_table_->SetValue(kDefaultSearchProviderKey, id);
}

int64 KeywordTable::GetDefaultSearchProviderID() {
  int64 value = kInvalidTemplateURLID;
  meta_table_->GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

bool KeywordTable::SetBuiltinKeywordVersion(int version) {
  return meta_table_->SetValue(kBuiltinKeywordVersion, version);
}

int KeywordTable::GetBuiltinKeywordVersion() {
  int version = 0;
  return meta_table_->GetValue(kBuiltinKeywordVersion, &version) ? version : 0;
}

// static
std::string KeywordTable::GetKeywordColumns() {
  return ColumnsForVersion(WebDatabase::kCurrentVersionNumber, false);
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

bool KeywordTable::MigrateToVersion29InstantURLToSupportsInstant() {
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
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  int64 default_search_id = GetDefaultSearchProviderID();
  if (!meta_table_->SetValue("Default Search Provider ID Backup",
                             default_search_id))
    return false;

  // Backup of all keywords.
  if (db_->DoesTableExist("keywords_backup") &&
      !db_->Execute("DROP TABLE keywords_backup"))
    return false;

  std::string query("CREATE TABLE keywords_backup AS SELECT " +
      ColumnsForVersion(44, false) + " FROM keywords ORDER BY id ASC");
  if (!db_->Execute(query.c_str()))
    return false;

  return transaction.Commit();
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

  // Migrate the keywords backup table as well.
  if (!MigrateKeywordsTableForVersion45("keywords_backup") ||
      !meta_table_->SetValue("Default Search Provider ID Backup Signature",
                             ""))
    return false;

  return transaction.Commit();
}

bool KeywordTable::MigrateToVersion47AddAlternateURLsColumn() {
  sql::Transaction transaction(db_);

  // Fill the |alternate_urls| column with empty strings, otherwise it breaks
  // code relying on GetTableContents that concatenates the strings from all
  // the columns.
  if (!transaction.Begin() ||
      !db_->Execute("ALTER TABLE keywords ADD COLUMN "
                    "alternate_urls VARCHAR DEFAULT ''"))
    return false;

  // Migrate the keywords backup table as well.
  if (!db_->Execute("ALTER TABLE keywords_backup ADD COLUMN "
                    "alternate_urls VARCHAR DEFAULT ''") ||
      !meta_table_->SetValue("Default Search Provider ID Backup Signature",
                             ""))
    return false;

  return transaction.Commit();
}

bool KeywordTable::MigrateToVersion48RemoveKeywordsBackup() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (!meta_table_->DeleteKey("Default Search Provider ID Backup") ||
      !meta_table_->DeleteKey("Default Search Provider ID Backup Signature"))
    return false;

  if (!db_->Execute("DROP TABLE keywords_backup"))
    return false;

  return transaction.Commit();
}

bool KeywordTable::MigrateToVersion49AddSearchTermsReplacementKeyColumn() {
  sql::Transaction transaction(db_);

  // Fill the |search_terms_replacement_key| column with empty strings;
  // See comments in MigrateToVersion47AddAlternateURLsColumn().
  if (!transaction.Begin() ||
      !db_->Execute("ALTER TABLE keywords ADD COLUMN "
                    "search_terms_replacement_key VARCHAR DEFAULT ''"))
    return false;

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

  data->alternate_urls.clear();
  base::JSONReader json_reader;
  scoped_ptr<Value> value(json_reader.ReadToValue(s.ColumnString(17)));
  ListValue* alternate_urls_value;
  if (value.get() && value->GetAsList(&alternate_urls_value)) {
    std::string alternate_url;
    for (size_t i = 0; i < alternate_urls_value->GetSize(); ++i) {
      if (alternate_urls_value->GetString(i, &alternate_url))
        data->alternate_urls.push_back(alternate_url);
    }
  }

  data->search_terms_replacement_key = s.ColumnString(18);

  return true;
}

bool KeywordTable::GetTableContents(const char* table_name,
                                    int table_version,
                                    std::string* contents) {
  DCHECK(contents);

  if (!db_->DoesTableExist(table_name))
    return false;

  contents->clear();
  std::string query("SELECT " + ColumnsForVersion(table_version, true) +
      " FROM " + std::string(table_name) + " ORDER BY id ASC");
  sql::Statement s((table_version == WebDatabase::kCurrentVersionNumber) ?
      db_->GetCachedStatement(sql::StatementID(table_name), query.c_str()) :
      db_->GetUniqueStatement(query.c_str()));
  while (s.Step())
    *contents += s.ColumnString(0);
  return s.Succeeded();
}

bool KeywordTable::GetKeywordAsString(TemplateURLID id,
                                      const std::string& table_name,
                                      std::string* result) {
  std::string query("SELECT " +
      ColumnsForVersion(WebDatabase::kCurrentVersionNumber, true) +
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
                  ColumnsForVersion(46, false) + " FROM " + name);
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
