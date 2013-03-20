// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_KEYWORD_TABLE_H_
#define CHROME_BROWSER_WEBDATA_KEYWORD_TABLE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/webdata/web_database_table.h"
#include "chrome/browser/search_engines/template_url_id.h"

struct TemplateURLData;
class WebDatabase;

namespace sql {
class Statement;
}  // namespace sql

// This class manages the |keywords| MetaTable within the SQLite database
// passed to the constructor. It expects the following schema:
//
// Note: The database stores time in seconds, UTC.
//
// keywords                 Most of the columns mirror that of a field in
//                          TemplateURLData.  See that struct for more details.
//   id
//   short_name
//   keyword
//   favicon_url
//   url
//   show_in_default_list
//   safe_for_autoreplace
//   originating_url
//   date_created           This column was added after we allowed keywords.
//                          Keywords created before we started tracking
//                          creation date have a value of 0 for this.
//   usage_count
//   input_encodings        Semicolon separated list of supported input
//                          encodings, may be empty.
//   suggest_url
//   prepopulate_id         See TemplateURLData::prepopulate_id.
//   created_by_policy      See TemplateURLData::created_by_policy.  This was
//                          added in version 26.
//   instant_url            See TemplateURLData::instant_url.  This was added in
//                          version 29.
//   last_modified          See TemplateURLData::last_modified.  This was added
//                          in version 38.
//   sync_guid              See TemplateURLData::sync_guid. This was added in
//                          version 39.
//   alternate_urls         See TemplateURLData::alternate_urls. This was added
//                          in version 47.
//   search_terms_replacement_key
//                          See TemplateURLData::search_terms_replacement_key.
//                          This was added in version 49.
//
//
// This class also manages some fields in the |meta| table:
//
// Default Search Provider ID        The id of the default search provider.
// Builtin Keyword Version           The version of builtin keywords data.
//
class KeywordTable : public WebDatabaseTable {
 public:
  typedef std::vector<TemplateURLData> Keywords;

  // Constants exposed for the benefit of test code:

  static const char kDefaultSearchProviderKey[];

  KeywordTable();
  virtual ~KeywordTable();

  // Retrieves the KeywordTable* owned by |database|.
  static KeywordTable* FromWebDatabase(WebDatabase* db);

  virtual WebDatabaseTable::TypeKey GetTypeKey() const OVERRIDE;
  virtual bool Init(sql::Connection* db, sql::MetaTable* meta_table) OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;
  virtual bool MigrateToVersion(int version,
                                const std::string& app_locale,
                                bool* update_compatible_version) OVERRIDE;

  // Adds a new keyword, updating the id field on success.
  // Returns true if successful.
  bool AddKeyword(const TemplateURLData& data);

  // Removes the specified keyword.
  // Returns true if successful.
  bool RemoveKeyword(TemplateURLID id);

  // Loads the keywords into the specified vector. It's up to the caller to
  // delete the returned objects.
  // Returns true on success.
  bool GetKeywords(Keywords* keywords);

  // Updates the database values for the specified url.
  // Returns true on success.
  bool UpdateKeyword(const TemplateURLData& data);

  // ID (TemplateURLData->id) of the default search provider.
  bool SetDefaultSearchProviderID(int64 id);
  int64 GetDefaultSearchProviderID();

  // Version of the built-in keywords.
  bool SetBuiltinKeywordVersion(int version);
  int GetBuiltinKeywordVersion();

  // Returns a comma-separated list of the keyword columns for the current
  // version of the table.
  static std::string GetKeywordColumns();

  // Table migration functions.
  bool MigrateToVersion21AutoGenerateKeywordColumn();
  bool MigrateToVersion25AddLogoIDColumn();
  bool MigrateToVersion26AddCreatedByPolicyColumn();
  bool MigrateToVersion28SupportsInstantColumn();
  bool MigrateToVersion29InstantURLToSupportsInstant();
  bool MigrateToVersion38AddLastModifiedColumn();
  bool MigrateToVersion39AddSyncGUIDColumn();
  bool MigrateToVersion44AddDefaultSearchProviderBackup();
  bool MigrateToVersion45RemoveLogoIDAndAutogenerateColumns();
  bool MigrateToVersion47AddAlternateURLsColumn();
  bool MigrateToVersion48RemoveKeywordsBackup();
  bool MigrateToVersion49AddSearchTermsReplacementKeyColumn();

 private:
  FRIEND_TEST_ALL_PREFIXES(KeywordTableTest, GetTableContents);
  FRIEND_TEST_ALL_PREFIXES(KeywordTableTest, GetTableContentsOrdering);
  FRIEND_TEST_ALL_PREFIXES(KeywordTableTest, SanitizeURLs);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseMigrationTest, MigrateVersion44ToCurrent);

  // NOTE: Since the table columns have changed in different versions, many
  // functions below take a |table_version| argument which dictates which
  // version number's column set to use.

  // Fills |data| with the data in |s|.  Returns false if we couldn't fill
  // |data| for some reason, e.g. |s| tried to set one of the fields to an
  // illegal value.
  static bool GetKeywordDataFromStatement(const sql::Statement& s,
                                          TemplateURLData* data);

  // Returns contents of selected table as a string in |contents| parameter.
  // Returns true on success, false otherwise.
  bool GetTableContents(const char* table_name,
                        int table_version,
                        std::string* contents);

  // Gets a string representation for keyword with id specified.
  // Used to store its result in |meta| table or to compare with another
  // keyword. Returns true on success, false otherwise.
  bool GetKeywordAsString(TemplateURLID id,
                          const std::string& table_name,
                          std::string* result);

  // Migrates table |name| (which should be either "keywords" or
  // "keywords_backup") from version 44 to version 45.
  bool MigrateKeywordsTableForVersion45(const std::string& name);

  DISALLOW_COPY_AND_ASSIGN(KeywordTable);
};

#endif  // CHROME_BROWSER_WEBDATA_KEYWORD_TABLE_H_
