// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_KEYWORD_TABLE_H_
#define CHROME_BROWSER_WEBDATA_KEYWORD_TABLE_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/webdata/web_database_table.h"
#include "chrome/browser/search_engines/template_url_id.h"

struct TemplateURLData;

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
//   autogenerate_keyword
//   created_by_policy      See TemplateURLData::created_by_policy.  This was
//                          added in version 26.
//   instant_url            See TemplateURLData::instant_url.  This was added in
//                          version 29.
//   last_modified          See TemplateURLData::last_modified.  This was added
//                          in version 38.
//   sync_guid              See TemplateURLData::sync_guid. This was added in
//                          version 39.
//
// keywords_backup          The full copy of the |keywords| table. Added in
//                          version 43. Must be in sync with |keywords|
//                          table otherwise verification of default search
//                          provider settings will fail.
//
// This class also manages some fields in the |meta| table:
//
// Default Search Provider ID        The id of the default search provider.
// Default Search Provider ID Backup
//                                   Backup copy of the above for restoring it
//                                   in case the setting was hijacked or
//                                   corrupted. This was added in version 40.
// Default Search Provider Backup  Backup copy of the raw in |keywords|
//                                   with the default search provider ID to
//                                   restore all provider info. This was added
//                                   in version 42. Not used in 43.
// Default Search Provider ID Backup Signature
//                                   The signature of backup data and
//                                   |keywords| table contents to be able to
//                                   verify the backup and understand when the
//                                   settings were changed. This was added
//                                   in version 40.
// Builtin Keyword Version           The version of builtin keywords data.
//
class KeywordTable : public WebDatabaseTable {
 public:
  typedef std::vector<TemplateURLData> Keywords;

  // Constants exposed for the benefit of test code:

  static const char kDefaultSearchProviderKey[];
  // Meta table key to store backup value for the default search provider id.
  static const char kDefaultSearchIDBackupKey[];
  // Meta table key to store backup value signature for the default search
  // provider.  The default search provider ID and the |keywords_backup| table
  // are signed.
  static const char kBackupSignatureKey[];
  // Comma-separated list of keyword table column names, in order.
  static const char kKeywordColumns[];

  KeywordTable(sql::Connection* db, sql::MetaTable* meta_table)
      : WebDatabaseTable(db, meta_table) {}
  virtual ~KeywordTable();
  virtual bool Init() OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;

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

  // If the default search provider backup is valid, returns true and copies the
  // backup into |backup|.  Otherwise returns false.
  bool GetDefaultSearchProviderBackup(TemplateURLData* backup);

  // Returns true if the default search provider has been changed out from under
  // us. This can happen if another process modifies our database or the file
  // was corrupted.
  bool DidDefaultSearchProviderChange();

  // Version of the built-in keywords.
  bool SetBuiltinKeywordVersion(int version);
  int GetBuiltinKeywordVersion();

  // Table migration functions.
  bool MigrateToVersion21AutoGenerateKeywordColumn();
  bool MigrateToVersion25AddLogoIDColumn();
  bool MigrateToVersion26AddCreatedByPolicyColumn();
  bool MigrateToVersion28SupportsInstantColumn();
  bool MigrateToVersion29InstantUrlToSupportsInstant();
  bool MigrateToVersion38AddLastModifiedColumn();
  bool MigrateToVersion39AddSyncGUIDColumn();
  bool MigrateToVersion44AddDefaultSearchProviderBackup();
  bool MigrateToVersion45RemoveLogoIDAndAutogenerateColumns();

 private:
  FRIEND_TEST_ALL_PREFIXES(KeywordTableTest, DefaultSearchProviderBackup);
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

  // Returns contents of |keywords_backup| table and default search provider
  // id backup as a string through |data|. Return value is true on success,
  // false otherwise.
  bool GetSignatureData(int table_version, std::string* data);

  // Returns contents of selected table as a string in |contents| parameter.
  // Returns true on success, false otherwise.
  bool GetTableContents(const char* table_name,
                        int table_version,
                        std::string* contents);

  // Updates settings backup, signs it and stores the signature in the
  // database. Returns true on success.
  bool UpdateBackupSignature(int table_version);

  // Signs the backup table.  This is a subset of what UpdateBackupSignature()
  // does.
  bool SignBackup(int table_version);

  // Checks the signature for the current settings backup. Returns true
  // if signature is valid, false otherwise.
  bool IsBackupSignatureValid(int table_version);

  // Gets a string representation for keyword with id specified.
  // Used to store its result in |meta| table or to compare with another
  // keyword. Returns true on success, false otherwise.
  bool GetKeywordAsString(TemplateURLID id,
                          const std::string& table_name,
                          std::string* result);

  // Updates default search provider id backup in |meta| table. Returns
  // true on success. The id is returned back via |id| parameter.
  bool UpdateDefaultSearchProviderIDBackup(TemplateURLID* id);

  // Migrates table |name| (which should be either "keywords" or
  // "keywords_backup" from version 44 to version 45.
  bool MigrateKeywordsTableForVersion45(const std::string& name);

  DISALLOW_COPY_AND_ASSIGN(KeywordTable);
};

#endif  // CHROME_BROWSER_WEBDATA_KEYWORD_TABLE_H_
