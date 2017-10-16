// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store.h"

#include <utility>

#include "base/logging.h"

// Implementation of Status.

ValueStore::Status::Status() : code(OK), restore_status(RESTORE_NONE) {}

ValueStore::Status::Status(StatusCode code, const std::string& message)
    : Status(code, RESTORE_NONE, message) {}

ValueStore::Status::Status(StatusCode code,
                           BackingStoreRestoreStatus restore_status,
                           const std::string& message)
    : code(code), restore_status(restore_status), message(message) {}

ValueStore::Status::Status(Status&& other) = default;

ValueStore::Status::~Status() = default;

ValueStore::Status& ValueStore::Status::operator=(Status&& rhs) = default;

void ValueStore::Status::Merge(const Status& status) {
  if (code == OK)
    code = status.code;
  if (message.empty() && !status.message.empty())
    message = status.message;
  if (restore_status == RESTORE_NONE)
    restore_status = status.restore_status;
}

// Implementation of ReadResult.

ValueStore::ReadResult::ReadResult(
    std::unique_ptr<base::DictionaryValue> settings,
    Status status)
    : settings_(std::move(settings)), status_(std::move(status)) {
  CHECK(settings_);
}

ValueStore::ReadResult::ReadResult(Status status)
    : status_(std::move(status)) {}

ValueStore::ReadResult::ReadResult(ReadResult&& other) = default;

ValueStore::ReadResult::~ReadResult() = default;

ValueStore::ReadResult& ValueStore::ReadResult::operator=(ReadResult&& rhs) =
    default;

// Implementation of WriteResult.

ValueStore::WriteResult::WriteResult(
    std::unique_ptr<ValueStoreChangeList> changes,
    Status status)
    : changes_(std::move(changes)), status_(std::move(status)) {
  CHECK(changes_);
}

ValueStore::WriteResult::WriteResult(Status status)
    : status_(std::move(status)) {}

ValueStore::WriteResult::WriteResult(WriteResult&& other) = default;

ValueStore::WriteResult::~WriteResult() = default;

ValueStore::WriteResult& ValueStore::WriteResult::operator=(WriteResult&& rhs) =
    default;
