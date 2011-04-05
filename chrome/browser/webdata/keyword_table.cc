// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/keyword_table.h"

#include "app/sql/statement.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/search_engines/template_url.h"
#include "googleurl/src/gurl.h"

using base::Time;

namespace {

// ID of the url column in keywords.
const int kUrlIdPosition = 16;

// Keys used in the meta table.
const char* kDefaultSearchProviderKey = "Default Search Provider ID";
const char* kBuiltinKeywordVersion = "Builtin Keyword Version";

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
}
}  // anonymous namespace

KeywordTable::~KeywordTable() {}

bool KeywordTable::Init() {
  if (!db_->DoesTableExist("keywords")) {
    if (!db_->Execute("CREATE TABLE keywords ("
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
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool KeywordTable::IsSyncable() {
  return true;
}

bool KeywordTable::AddKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  // Be sure to change kUrlIdPosition if you add columns
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO keywords "
      "(short_name, keyword, favicon_url, url, safe_for_autoreplace, "
      "originating_url, date_created, usage_count, input_encodings, "
      "show_in_default_list, suggest_url, prepopulate_id, "
      "autogenerate_keyword, logo_id, created_by_policy, instant_url, "
      "id) VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
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
  return true;
}

bool KeywordTable::RemoveKeyword(TemplateURLID id) {
  DCHECK(id);
  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM keywords WHERE id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, id);
  return s.Run();
}

bool KeywordTable::GetKeywords(std::vector<TemplateURL*>* urls) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT id, short_name, keyword, favicon_url, url, "
      "safe_for_autoreplace, originating_url, date_created, "
      "usage_count, input_encodings, show_in_default_list, "
      "suggest_url, prepopulate_id, autogenerate_keyword, logo_id, "
      "created_by_policy, instant_url "
      "FROM keywords ORDER BY id ASC"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  while (s.Step()) {
    TemplateURL* template_url = new TemplateURL();
    template_url->set_id(s.ColumnInt64(0));

    std::string tmp;
    tmp = s.ColumnString(1);
    DCHECK(!tmp.empty());
    template_url->set_short_name(UTF8ToUTF16(tmp));

    template_url->set_keyword(UTF8ToUTF16(s.ColumnString(2)));

    tmp = s.ColumnString(3);
    if (!tmp.empty())
      template_url->SetFaviconURL(GURL(tmp));

    template_url->SetURL(s.ColumnString(4), 0, 0);

    template_url->set_safe_for_autoreplace(s.ColumnInt(5) == 1);

    tmp = s.ColumnString(6);
    if (!tmp.empty())
      template_url->set_originating_url(GURL(tmp));

    template_url->set_date_created(Time::FromTimeT(s.ColumnInt64(7)));

    template_url->set_usage_count(s.ColumnInt(8));

    std::vector<std::string> encodings;
    base::SplitString(s.ColumnString(9), ';', &encodings);
    template_url->set_input_encodings(encodings);

    template_url->set_show_in_default_list(s.ColumnInt(10) == 1);

    template_url->SetSuggestionsURL(s.ColumnString(11), 0, 0);

    template_url->set_prepopulate_id(s.ColumnInt(12));

    template_url->set_autogenerate_keyword(s.ColumnInt(13) == 1);

    template_url->set_logo_id(s.ColumnInt(14));

    template_url->set_created_by_policy(s.ColumnBool(15));

    template_url->SetInstantURL(s.ColumnString(16), 0, 0);

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
      "logo_id=?, created_by_policy=?, instant_url=? WHERE id=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.BindInt64(kUrlIdPosition, url.id());
  return s.Run();
}

bool KeywordTable::SetDefaultSearchProviderID(int64 id) {
  return meta_table_->SetValue(kDefaultSearchProviderKey, id);
}

int64 KeywordTable::GetDefaulSearchProviderID() {
  int64 value = 0;
  meta_table_->GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

bool KeywordTable::SetBuitinKeywordVersion(int version) {
  return meta_table_->SetValue(kBuiltinKeywordVersion, version);
}

int KeywordTable::GetBuitinKeywordVersion() {
  int version = 0;
  meta_table_->GetValue(kBuiltinKeywordVersion, &version);
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
