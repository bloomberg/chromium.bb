// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/token_service_table.h"

#include <map>
#include <string>

#include "app/sql/statement.h"
#include "base/logging.h"
#include "chrome/browser/password_manager/encryptor.h"

bool TokenServiceTable::Init() {
  if (!db_->DoesTableExist("token_service")) {
    if (!db_->Execute("CREATE TABLE token_service ("
                      "service VARCHAR PRIMARY KEY NOT NULL,"
                      "encrypted_token BLOB)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool TokenServiceTable::IsSyncable() {
  return true;
}

bool TokenServiceTable::RemoveAllTokens() {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM token_service"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  return s.Run();
}

bool TokenServiceTable::SetTokenForService(const std::string& service,
                                           const std::string& token) {
  // Don't bother with a cached statement since this will be a relatively
  // infrequent operation.
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO token_service "
      "(service, encrypted_token) VALUES (?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::string encrypted_token;

  bool encrypted = Encryptor::EncryptString(token, &encrypted_token);
  if (!encrypted) {
    return false;
  }

  s.BindString(0, service);
  s.BindBlob(1, encrypted_token.data(),
             static_cast<int>(encrypted_token.length()));
  return s.Run();
}

bool TokenServiceTable::GetAllTokens(
    std::map<std::string, std::string>* tokens) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT service, encrypted_token FROM token_service"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  while (s.Step()) {
    std::string encrypted_token;
    std::string decrypted_token;
    std::string service;
    service = s.ColumnString(0);
    bool entry_ok = !service.empty() &&
                    s.ColumnBlobAsString(1, &encrypted_token);
    if (entry_ok) {
      Encryptor::DecryptString(encrypted_token, &decrypted_token);
      (*tokens)[service] = decrypted_token;
    } else {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

