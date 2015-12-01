// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store.h"

#include "base/logging.h"

// Implementation of Status.

ValueStore::Status::Status() : code(OK) {}

ValueStore::Status::Status(StatusCode code, const std::string& message)
    : code(code), message(message) {}

ValueStore::Status::~Status() {}

// Implementation of ReadResultType.

ValueStore::ReadResultType::ReadResultType(
    scoped_ptr<base::DictionaryValue> settings) : settings_(settings.Pass()) {
  CHECK(settings_);
}

ValueStore::ReadResultType::ReadResultType(const Status& status)
    : status_(status) {}

ValueStore::ReadResultType::~ReadResultType() {}

// Implementation of WriteResultType.

ValueStore::WriteResultType::WriteResultType(
    scoped_ptr<ValueStoreChangeList> changes)
    : changes_(changes.Pass()) {
  CHECK(changes_);
}

ValueStore::WriteResultType::WriteResultType(const Status& status)
    : status_(status) {}

ValueStore::WriteResultType::~WriteResultType() {}
