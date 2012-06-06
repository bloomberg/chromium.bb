// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/value_store.h"

#include "base/logging.h"

// Implementation of ReadResultType.

ValueStore::ReadResultType::ReadResultType(DictionaryValue* settings)
    : settings_(settings) {
  DCHECK(settings);
}

ValueStore::ReadResultType::ReadResultType(const std::string& error)
    : error_(error) {
  DCHECK(!error.empty());
}

ValueStore::ReadResultType::~ReadResultType() {}

bool ValueStore::ReadResultType::HasError() const {
  return !error_.empty();
}

scoped_ptr<DictionaryValue>& ValueStore::ReadResultType::settings() {
  DCHECK(!HasError());
  return settings_;
}

const std::string& ValueStore::ReadResultType::error() const {
  DCHECK(HasError());
  return error_;
}

// Implementation of WriteResultType.

ValueStore::WriteResultType::WriteResultType(ValueStoreChangeList* changes)
    : changes_(changes) {
  DCHECK(changes);
}

ValueStore::WriteResultType::WriteResultType(const std::string& error)
    : error_(error) {
  DCHECK(!error.empty());
}

ValueStore::WriteResultType::~WriteResultType() {}

bool ValueStore::WriteResultType::HasError() const {
  return !error_.empty();
}

const ValueStoreChangeList& ValueStore::WriteResultType::changes() const {
  DCHECK(!HasError());
  return *changes_;
}

const std::string& ValueStore::WriteResultType::error() const {
  DCHECK(HasError());
  return error_;
}
