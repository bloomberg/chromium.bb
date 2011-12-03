// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/keyword_table.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/protector/histograms.h"
#include "chrome/browser/protector/protector.h"
#include "chrome/browser/search_engines/template_url.h"
#include "googleurl/src/gurl.h"
#include "sql/statement.h"
#include "sql/transaction.h"

using base::Time;

namespace {

// ID of the url column in keywords.
const int kUrlIdPosition = 18;

// Keys used in the meta table.
const char kDefaultSearchProviderKey[] = "Default Search Provider ID";
const char kBuiltinKeywordVersion[] = "Builtin Keyword Version";

// Meta table key to store backup value for the default search provider.
const char kDefaultSearchBackupKey[] = "Default Search Provider Backup";

// Meta table key to store backup value for the default search provider id.
const char kDefaultSearchIDBackupKey[] =
    "Default Search Provider ID Backup";

// Meta table key to store backup value signature for the default search
// provider. Default search provider id, its row in |keywords| table and
// the whole |keywords| table are signed.
const char kBackupSignatureKey[] =
    "Default Search Provider ID Backup Signature";

void BindURLToStatement(const TemplateURL& url, sql::Statement* s) {
  s->BindString(0, UTF16ToUTF8(url.short_name()));
  s->BindString(1, UTF16ToUTF8(url.keyword()));
  GURL favicon_url = url.GetFaviconURL();
  if (!favicon_url.is_valid()) {
    s->BindString(2, std::string());
  } else {
    s->BindString(2, history::HistoryDatabase::GURLToDatabaseURL(
        url.GetFaviconURL()));
  }
  s->BindString(3, url.url() ? url.url()->url() : std::string());
  s->BindInt(4, url.safe_for_autoreplace() ? 1 : 0);
  if (!url.originating_url().is_valid()) {
    s->BindString(5, std::string());
  } else {
    s->BindString(5, history::HistoryDatabase::GURLToDatabaseURL(
        url.originating_url()));
  }
  s->BindInt64(6, url.date_created().ToTimeT());
  s->BindInt(7, url.usage_count());
  s->BindString(8, JoinString(url.input_encodings(), ';'));
  s->BindInt(9, url.show_in_default_list() ? 1 : 0);
  s->BindString(10, url.suggestions_url() ? url.suggestions_url()->url() :
                std::string());
  s->BindInt(11, url.prepopulate_id());
  s->BindInt(12, url.autogenerate_keyword() ? 1 : 0);
  s->BindInt(13, url.logo_id());
  s->BindBool(14, url.created_by_policy());
  s->BindString(15, url.instant_url() ? url.instant_url()->url() :
                std::string());
  s->BindInt64(16, url.last_modified().ToTimeT());
  s->BindString(17, url.sync_guid());
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
  if (!db_->DoesTableExist("keywords")) {
    if (!db_->Execute(
        "CREATE TABLE keywords ("
        "id INTEGER PRIMARY KEY,"
        "short_name VARCHAR NOT NULL,"
        "keyword VARCHAR NOT NULL,"
        "favicon_url VARCHAR NOT NULL,"
        "url VARCHAR NOT NULL,"
        "show_in_default_list INTEGER,"
        "safe_for_autoreplace INTEGER,"
        "originating_url VARCHAR,"
        "date_created INTEGER DEFAULT 0,"
        "usage_count INTEGER DEFAULT 0,"
        "input_encodings VARCHAR,"
        "suggest_url VARCHAR,"
        "prepopulate_id INTEGER DEFAULT 0,"
        "autogenerate_keyword INTEGER DEFAULT 0,"
        "logo_id INTEGER DEFAULT 0,"
        "created_by_policy INTEGER DEFAULT 0,"
        "instant_url VARCHAR,"
        "last_modified INTEGER DEFAULT 0,"
        "sync_guid VARCHAR)")) {
      NOTREACHED();
      return false;
    }
    if (!UpdateBackupSignature())
      return false;
  }
  return true;
}

bool KeywordTable::IsSyncable() {
  return true;
}

bool KeywordTable::AddKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  // Be sure to change kUrlIdPosition if you add columns
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO keywords "
      "(short_name, keyword, favicon_url, url, safe_for_autoreplace, "
      "originating_url, date_created, usage_count, input_encodings, "
      "show_in_default_list, suggest_url, prepopulate_id, "
      "autogenerate_keyword, logo_id, created_by_policy, instant_url, "
      "last_modified, sync_guid, id) VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.BindInt64(kUrlIdPosition, url.id());
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return UpdateBackupSignature();
}

bool KeywordTable::RemoveKeyword(TemplateURLID id) {
  DCHECK(id);
  sql::Statement s(db_->GetUniqueStatement("DELETE FROM keywords WHERE id=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, id);
  return s.Run() && UpdateBackupSignature();
}

bool KeywordTable::GetKeywords(std::vector<TemplateURL*>* urls) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT id, short_name, keyword, favicon_url, url, "
      "safe_for_autoreplace, originating_url, date_created, "
      "usage_count, input_encodings, show_in_default_list, "
      "suggest_url, prepopulate_id, autogenerate_keyword, logo_id, "
      "created_by_policy, instant_url, last_modified, sync_guid "
      "FROM keywords ORDER BY id ASC"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  while (s.Step()) {
    TemplateURL* template_url = new TemplateURL();
    GetURLFromStatement(s, template_url);
    urls->push_back(template_url);
  }
  return s.Succeeded();
}

bool KeywordTable::UpdateKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  // Be sure to change kUrlIdPosition if you add columns
  sql::Statement s(db_->GetUniqueStatement(
      "UPDATE keywords "
      "SET short_name=?, keyword=?, favicon_url=?, url=?, "
      "safe_for_autoreplace=?, originating_url=?, date_created=?, "
      "usage_count=?, input_encodings=?, show_in_default_list=?, "
      "suggest_url=?, prepopulate_id=?, autogenerate_keyword=?, "
      "logo_id=?, created_by_policy=?, instant_url=?, last_modified=?, "
      "sync_guid=? WHERE id=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.BindInt64(kUrlIdPosition, url.id());
  return s.Run() && UpdateBackupSignature();
}

bool KeywordTable::SetDefaultSearchProviderID(int64 id) {
  return meta_table_->SetValue(kDefaultSearchProviderKey, id) &&
         UpdateBackupSignature();
}

int64 KeywordTable::GetDefaultSearchProviderID() {
  int64 value = 0;
  meta_table_->GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

int64 KeywordTable::GetDefaultSearchProviderIDBackup() {
  if (!IsBackupSignatureValid())
    return 0;
  int64 backup_value = 0;
  meta_table_->GetValue(kDefaultSearchIDBackupKey, &backup_value);
  return backup_value;
}

bool KeywordTable::DidDefaultSearchProviderChange() {
  if (!IsBackupSignatureValid()) {
    UMA_HISTOGRAM_ENUMERATION(
        protector::kProtectorHistogramDefaultSearchProvider,
        protector::kProtectorErrorBackupInvalid,
        protector::kProtectorErrorCount);
    return true;
  }

  int64 backup_id = 0;
  meta_table_->GetValue(kDefaultSearchIDBackupKey, &backup_id);
  int64 current_id = GetDefaultSearchProviderID();
  if (backup_id == current_id) {
    std::string backup_url;
    std::string current_url;
    // Either this is a new profile and both IDs are zero or the search
    // engines with the ids are equal.
    if (backup_id == 0) {
      UMA_HISTOGRAM_ENUMERATION(
          protector::kProtectorHistogramDefaultSearchProvider,
          protector::kProtectorErrorValueValidZero,
          protector::kProtectorErrorCount);
      return false;
    } else if (meta_table_->GetValue(kDefaultSearchBackupKey, &backup_url) &&
               GetTemplateURLBackup(current_id, &current_url) &&
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
  return true;
}

bool KeywordTable::SetBuiltinKeywordVersion(int version) {
  return meta_table_->SetValue(kBuiltinKeywordVersion, version);
}

int KeywordTable::GetBuiltinKeywordVersion() {
  int version = 0;
  if (!meta_table_->GetValue(kBuiltinKeywordVersion, &version))
    return 0;
  return version;
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
  if (!db_->Execute("ALTER TABLE keywords ADD COLUMN instant_url VARCHAR"))
    return false;

  if (!db_->Execute("CREATE TABLE keywords_temp ("
                    "id INTEGER PRIMARY KEY,"
                    "short_name VARCHAR NOT NULL,"
                    "keyword VARCHAR NOT NULL,"
                    "favicon_url VARCHAR NOT NULL,"
                    "url VARCHAR NOT NULL,"
                    "show_in_default_list INTEGER,"
                    "safe_for_autoreplace INTEGER,"
                    "originating_url VARCHAR,"
                    "date_created INTEGER DEFAULT 0,"
                    "usage_count INTEGER DEFAULT 0,"
                    "input_encodings VARCHAR,"
                    "suggest_url VARCHAR,"
                    "prepopulate_id INTEGER DEFAULT 0,"
                    "autogenerate_keyword INTEGER DEFAULT 0,"
                    "logo_id INTEGER DEFAULT 0,"
                    "created_by_policy INTEGER DEFAULT 0,"
                    "instant_url VARCHAR)")) {
    return false;
  }

  if (!db_->Execute(
      "INSERT INTO keywords_temp "
      "SELECT id, short_name, keyword, favicon_url, url, "
      "show_in_default_list, safe_for_autoreplace, originating_url, "
      "date_created, usage_count, input_encodings, suggest_url, "
      "prepopulate_id, autogenerate_keyword, logo_id, created_by_policy, "
      "instant_url FROM keywords")) {
    return false;
  }

  if (!db_->Execute("DROP TABLE keywords"))
    return false;

  if (!db_->Execute("ALTER TABLE keywords_temp RENAME TO keywords"))
    return false;

  return true;
}

bool KeywordTable::MigrateToVersion38AddLastModifiedColumn() {
  return db_->Execute(
      "ALTER TABLE keywords ADD COLUMN last_modified INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion39AddSyncGUIDColumn() {
  return db_->Execute(
      "ALTER TABLE keywords ADD COLUMN sync_guid VARCHAR");
}

bool KeywordTable::MigrateToVersion40AddDefaultSearchProviderBackup() {
  int64 value = 0;
  if (!meta_table_->GetValue(kDefaultSearchProviderKey, &value)) {
    // Write default search provider id if it's absent. TemplateURLService
    // will replace 0 with some real value.
    if (!meta_table_->SetValue(kDefaultSearchProviderKey, 0))
      return false;
  }
  return meta_table_->SetValue(kDefaultSearchIDBackupKey, value) &&
         meta_table_->SetValue(
             kBackupSignatureKey,
             GetSearchProviderIDSignature(value));
}

bool KeywordTable::MigrateToVersion41RewriteDefaultSearchProviderBackup() {
  // Due to crbug.com/101815 version 40 may contain corrupt or empty
  // signature. So ignore the signature and simply rewrite it.
  return MigrateToVersion40AddDefaultSearchProviderBackup();
}

bool KeywordTable::MigrateToVersion42AddKeywordsBackupTable() {
  return UpdateBackupSignature();
}

std::string KeywordTable::GetSignatureData() {
  int64 backup_value = 0;
  if (!meta_table_->GetValue(kDefaultSearchIDBackupKey, &backup_value)) {
    NOTREACHED() << "Couldn't get id backup.";
    return std::string();
  }
  std::string backup_data = base::Int64ToString(backup_value);

  std::string backup_url;
  if (!meta_table_->GetValue(kDefaultSearchBackupKey, &backup_url)) {
    NOTREACHED() << "Couldn't get backup url";
    return std::string();
  }
  backup_data += backup_url;

  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE,
      "SELECT id || short_name || keyword || favicon_url || url || "
      "safe_for_autoreplace || originating_url || date_created || "
      "usage_count || input_encodings || show_in_default_list || "
      "suggest_url || prepopulate_id || autogenerate_keyword || logo_id || "
      "created_by_policy || instant_url || last_modified || sync_guid "
      "FROM keywords ORDER BY id ASC"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return std::string();
  }
  while (s.Step())
    backup_data += s.ColumnString(0);
  if (!s.Succeeded()) {
    NOTREACHED() << "Statement execution failed";
    return std::string();
  }
  return backup_data;
}

bool KeywordTable::UpdateBackupSignature() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    NOTREACHED() << "Failed to begin transaction.";
    return false;
  }

  // Backup of default search provider id.
  int64 id = GetDefaultSearchProviderID();
  if (!meta_table_->SetValue(kDefaultSearchIDBackupKey, id)) {
    NOTREACHED() << "Failed to write backup id.";
    return false;
  }

  // Backup of the default search provider info.
  if (!UpdateDefaultSearchProviderBackup(id)) {
    NOTREACHED() << "Failed to update backup.";
    return false;
  }

  // Now calculate and update the signature.
  std::string data_to_sign = GetSignatureData();
  if (data_to_sign.empty()) {
    NOTREACHED() << "Can't get data to sign";
    return false;
  }
  std::string signature = protector::SignSetting(data_to_sign);
  if (signature.empty()) {
    NOTREACHED() << "Signature is empty";
    return false;
  }
  if (!meta_table_->SetValue(kBackupSignatureKey, signature)) {
    NOTREACHED() << "Failed to write signature.";
    return false;
  }

  return transaction.Commit();
}

bool KeywordTable::IsBackupSignatureValid() {
  std::string signature;
  return meta_table_->GetValue(kBackupSignatureKey, &signature) &&
         protector::IsSettingValid(GetSignatureData(), signature);
}

void KeywordTable::GetURLFromStatement(
    const sql::Statement& s,
    TemplateURL* url) {
  url->set_id(s.ColumnInt64(0));

  std::string tmp;
  tmp = s.ColumnString(1);
  DCHECK(!tmp.empty());
  url->set_short_name(UTF8ToUTF16(tmp));

  url->set_keyword(UTF8ToUTF16(s.ColumnString(2)));

  tmp = s.ColumnString(3);
  if (!tmp.empty())
    url->SetFaviconURL(GURL(tmp));

  url->SetURL(s.ColumnString(4), 0, 0);

  url->set_safe_for_autoreplace(s.ColumnInt(5) == 1);

  tmp = s.ColumnString(6);
  if (!tmp.empty())
    url->set_originating_url(GURL(tmp));

  url->set_date_created(Time::FromTimeT(s.ColumnInt64(7)));

  url->set_usage_count(s.ColumnInt(8));

  std::vector<std::string> encodings;
  base::SplitString(s.ColumnString(9), ';', &encodings);
  url->set_input_encodings(encodings);

  url->set_show_in_default_list(s.ColumnInt(10) == 1);

  url->SetSuggestionsURL(s.ColumnString(11), 0, 0);

  url->SetPrepopulateId(s.ColumnInt(12));

  url->set_autogenerate_keyword(s.ColumnInt(13) == 1);

  url->set_logo_id(s.ColumnInt(14));

  url->set_created_by_policy(s.ColumnBool(15));

  url->SetInstantURL(s.ColumnString(16), 0, 0);

  url->set_last_modified(Time::FromTimeT(s.ColumnInt64(17)));

  url->set_sync_guid(s.ColumnString(18));
}

bool KeywordTable::GetTemplateURLBackup(TemplateURLID id,
                                        std::string* result) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT id || short_name || keyword || favicon_url || url || "
      "safe_for_autoreplace || originating_url || date_created || "
      "usage_count || input_encodings || show_in_default_list || "
      "suggest_url || prepopulate_id || autogenerate_keyword || logo_id || "
      "created_by_policy || instant_url || last_modified || sync_guid "
      "FROM keywords WHERE id=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, id);
  if (!s.Step()) {
    LOG(WARNING) << "No keyword with id: " << id << ", ignoring.";
    return true;
  }

  if (!s.Succeeded()) {
    NOTREACHED() << "Statement failed.";
    return false;
  }

  *result = s.ColumnString(0);
  return true;
}

bool KeywordTable::UpdateDefaultSearchProviderBackup(TemplateURLID id) {
  std::string backup;
  if (id != 0 && !GetTemplateURLBackup(id, &backup)) {
    NOTREACHED() << "Failed to get the keyword with id " << id;
    return false;
  }
  return meta_table_->SetValue(kDefaultSearchBackupKey, backup);
}
