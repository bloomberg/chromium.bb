// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_KEYWORD_TABLE_H_
#define CHROME_BROWSER_WEBDATA_KEYWORD_TABLE_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/webdata/web_database_table.h"
#include "chrome/browser/search_engines/template_url_id.h"

class TemplateURL;

// This class manages the |keywords| MetaTable within the SQLite database
// passed to the constructor. It expects the following schema:
//
// Note: The database stores time in seconds, UTC.
//
// keywords                 Most of the columns mirror that of a field in
//                          TemplateURL. See TemplateURL for more details.
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
//   prepopulate_id         See TemplateURL::prepopulate_id.
//   autogenerate_keyword
//   logo_id                See TemplateURL::logo_id
//   created_by_policy      See TemplateURL::created_by_policy.  This was added
//                          in version 26.
//   instant_url            See TemplateURL::instant_url.  This was added
//                          in version 29.
//   last_modified          See TemplateURL::last_modified.  This was added in
//                          version 38.
//   sync_guid              See TemplateURL::sync_guid. This was added in
//                          version 39.
//
class KeywordTable : public WebDatabaseTable {
 public:
  KeywordTable(sql::Connection* db, sql::MetaTable* meta_table)
      : WebDatabaseTable(db, meta_table) {}
  virtual ~KeywordTable();
  virtual bool Init() OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;

  // Adds a new keyword, updating the id field on success.
  // Returns true if successful.
  bool AddKeyword(const TemplateURL& url);

  // Removes the specified keyword.
  // Returns true if successful.
  bool RemoveKeyword(TemplateURLID id);

  // Loads the keywords into the specified vector. It's up to the caller to
  // delete the returned objects.
  // Returns true on success.
  bool GetKeywords(std::vector<TemplateURL*>* urls);

  // Updates the database values for the specified url.
  // Returns true on success.
  bool UpdateKeyword(const TemplateURL& url);

  // ID (TemplateURL->id) of the default search provider.
  bool SetDefaultSearchProviderID(int64 id);
  int64 GetDefaultSearchProviderID();

  // Backup of the default search provider. 0 if the setting can't be verified.
  int64 GetDefaultSearchProviderIDBackup();

  // Returns true if the default search provider has been changed out under
  // us. This can happen if another process modifies our database or the
  // file was corrupted.
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
  bool MigrateToVersion40AddDefaultSearchProviderBackup();
  bool MigrateToVersion41RewriteDefaultSearchProviderBackup();

 private:
  FRIEND_TEST_ALL_PREFIXES(KeywordTableTest, DefaultSearchProviderBackup);

  // Sets backup for id of the default search provider.
  // Backup value is used to notice when unexpected change of the id
  // occurred (i.e. by third-party process trying to modify user settings).
  bool SetDefaultSearchProviderBackupID(int64 id);

  // Sets signature for the backup field.
  bool SetDefaultSearchProviderBackupIDSignature(int64 id);

  DISALLOW_COPY_AND_ASSIGN(KeywordTable);
};

#endif  // CHROME_BROWSER_WEBDATA_KEYWORD_TABLE_H_
