// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/html5_storage_commands.h"

#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"

namespace webdriver {

LocalStorageCommand::LocalStorageCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

LocalStorageCommand::~LocalStorageCommand() {}

bool LocalStorageCommand::DoesGet() {
  return true;
}

bool LocalStorageCommand::DoesPost() {
  return true;
}

bool LocalStorageCommand::DoesDelete() {
  return true;
}

void LocalStorageCommand::ExecuteGet(Response* const response) {
  base::ListValue* keys;
  Error* error = session_->GetStorageKeys(kLocalStorageType, &keys);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(keys);
}

void LocalStorageCommand::ExecutePost(Response* const response) {
  // "/session/$sessionId/local_storage"
  std::string key;
  std::string value;
  if (!GetStringParameter("key", &key) ||
      !GetStringParameter("value", &value)) {
    response->SetError(new Error(
        kBadRequest, "('key', 'value') parameter is missing or invalid"));
    return;
  }

  Error* error = session_->SetStorageItem(kLocalStorageType, key, value);
  if (error)
    response->SetError(error);
}

void LocalStorageCommand::ExecuteDelete(Response* const response) {
  Error* error = session_->ClearStorage(kLocalStorageType);
  if (error)
    response->SetError(error);
}

LocalStorageKeyCommand::LocalStorageKeyCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

LocalStorageKeyCommand::~LocalStorageKeyCommand() {}

bool LocalStorageKeyCommand::DoesGet() {
  return true;
}

bool LocalStorageKeyCommand::DoesDelete() {
  return true;
}

void LocalStorageKeyCommand::ExecuteGet(Response* const response) {
  // "/session/$sessionId/local_storage/key/$key"
  std::string key = GetPathVariable(5);
  std::string value;
  Error* error = session_->GetStorageItem(kLocalStorageType, key, &value);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::StringValue(value));
}

void LocalStorageKeyCommand::ExecuteDelete(Response* const response) {
  // "/session/$sessionId/local_storage/key/$key"
  std::string key = GetPathVariable(5);
  std::string value;
  Error* error = session_->RemoveStorageItem(kLocalStorageType, key, &value);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::StringValue(value));
}

LocalStorageSizeCommand::LocalStorageSizeCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

LocalStorageSizeCommand::~LocalStorageSizeCommand() {}

bool LocalStorageSizeCommand::DoesGet() {
  return true;
}

void LocalStorageSizeCommand::ExecuteGet(Response* const response) {
  int size;
  Error* error = session_->GetStorageSize(kLocalStorageType, &size);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::FundamentalValue(size));
}

SessionStorageCommand::SessionStorageCommand(
    const std::vector<std::string>& path_segments,
    base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

SessionStorageCommand::~SessionStorageCommand() {}

bool SessionStorageCommand::DoesGet() {
  return true;
}

bool SessionStorageCommand::DoesPost() {
  return true;
}

bool SessionStorageCommand::DoesDelete() {
  return true;
}

void SessionStorageCommand::ExecuteGet(Response* const response) {
  base::ListValue* keys;
  Error* error = session_->GetStorageKeys(kSessionStorageType, &keys);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(keys);
}

void SessionStorageCommand::ExecutePost(Response* const response) {
  // "/session/$sessionId/session_storage"
  std::string key;
  std::string value;
  if (!GetStringParameter("key", &key) ||
      !GetStringParameter("value", &value)) {
    response->SetError(new Error(
        kBadRequest, "('key', 'value') parameter is missing or invalid"));
    return;
  }

  Error* error = session_->SetStorageItem(kSessionStorageType, key, value);
  if (error)
    response->SetError(error);
}

void SessionStorageCommand::ExecuteDelete(Response* const response) {
  Error* error = session_->ClearStorage(kSessionStorageType);
  if (error)
    response->SetError(error);
}

SessionStorageKeyCommand::SessionStorageKeyCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

SessionStorageKeyCommand::~SessionStorageKeyCommand() {}

bool SessionStorageKeyCommand::DoesGet() {
  return true;
}

bool SessionStorageKeyCommand::DoesDelete() {
  return true;
}

void SessionStorageKeyCommand::ExecuteGet(Response* const response) {
  // "/session/$sessionId/session_storage/key/$key"
  std::string key = GetPathVariable(5);
  std::string value;
  Error* error = session_->GetStorageItem(kSessionStorageType, key, &value);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::StringValue(value));
}

void SessionStorageKeyCommand::ExecuteDelete(Response* const response) {
  // "/session/$sessionId/session_storage/key/$key"
  std::string key = GetPathVariable(5);
  std::string value;
  Error* error = session_->RemoveStorageItem(kSessionStorageType, key, &value);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::StringValue(value));
}

SessionStorageSizeCommand::SessionStorageSizeCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

SessionStorageSizeCommand::~SessionStorageSizeCommand() {}

bool SessionStorageSizeCommand::DoesGet() {
  return true;
}

void SessionStorageSizeCommand::ExecuteGet(Response* const response) {
  int size;
  Error* error = session_->GetStorageSize(kSessionStorageType, &size);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::FundamentalValue(size));
}

}  // namespace webdriver
