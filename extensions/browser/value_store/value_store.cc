// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store.h"

#include "base/logging.h"

// Implementation of Error.

ValueStore::Error::Error(ErrorCode code,
                         const std::string& message,
                         scoped_ptr<std::string> key)
    : code(code), message(message), key(key.Pass()) {}

ValueStore::Error::~Error() {}

// Implementation of ReadResultType.

ValueStore::ReadResultType::ReadResultType(
    scoped_ptr<base::DictionaryValue> settings) : settings_(settings.Pass()) {
  CHECK(settings_);
}

ValueStore::ReadResultType::ReadResultType(scoped_ptr<Error> error)
    : error_(error.Pass()) {
  CHECK(error_);
}

ValueStore::ReadResultType::~ReadResultType() {}

// Implementation of WriteResultType.

ValueStore::WriteResultType::WriteResultType(
    scoped_ptr<ValueStoreChangeList> changes)
    : changes_(changes.Pass()) {
  CHECK(changes_);
}

ValueStore::WriteResultType::WriteResultType(scoped_ptr<Error> error)
    : error_(error.Pass()) {
  CHECK(error_);
}

ValueStore::WriteResultType::~WriteResultType() {}
