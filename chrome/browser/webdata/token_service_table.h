// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_TOKEN_SERVICE_TABLE_H_
#define CHROME_BROWSER_WEBDATA_TOKEN_SERVICE_TABLE_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/webdata/web_database_table.h"

class WebDatabase;

class TokenServiceTable : public WebDatabaseTable {
 public:
  TokenServiceTable() {}
  virtual ~TokenServiceTable() {}

  // Retrieves the TokenServiceTable* owned by |database|.
  static TokenServiceTable* FromWebDatabase(WebDatabase* db);

  virtual WebDatabaseTable::TypeKey GetTypeKey() const OVERRIDE;
  virtual bool Init(sql::Connection* db, sql::MetaTable* meta_table) OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;
  virtual bool MigrateToVersion(int version,
                                const std::string& app_locale,
                                bool* update_compatible_version) OVERRIDE;

  // Remove all tokens previously set with SetTokenForService.
  bool RemoveAllTokens();

  // Retrieves all tokens previously set with SetTokenForService.
  // Returns true if there were tokens and we decrypted them,
  // false if there was a failure somehow
  bool GetAllTokens(std::map<std::string, std::string>* tokens);

  // Store a token in the token_service table. Stored encrypted. May cause
  // a mac keychain popup.
  // True if we encrypted a token and stored it, false otherwise.
  bool SetTokenForService(const std::string& service,
                          const std::string& token);

 private:
  DISALLOW_COPY_AND_ASSIGN(TokenServiceTable);
};

#endif  // CHROME_BROWSER_WEBDATA_TOKEN_SERVICE_TABLE_H_
