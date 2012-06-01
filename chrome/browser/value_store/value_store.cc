// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/value_store.h"

#include "base/logging.h"

// Implementation of ReadResult.

ValueStore::ReadResult::ReadResult(DictionaryValue* settings)
    : inner_(new Inner(settings, "")) {
  DCHECK(settings);
}

ValueStore::ReadResult::ReadResult(const std::string& error)
    : inner_(new Inner(NULL, error)) {
  DCHECK(!error.empty());
}

ValueStore::ReadResult::~ReadResult() {}

bool ValueStore::ReadResult::HasError() const {
  return !inner_->error_.empty();
}

const DictionaryValue& ValueStore::ReadResult::settings() const {
  DCHECK(!HasError());
  return *inner_->settings_;
}

const std::string& ValueStore::ReadResult::error() const {
  DCHECK(HasError());
  return inner_->error_;
}

ValueStore::ReadResult::Inner::Inner(
    DictionaryValue* settings, const std::string& error)
    : settings_(settings), error_(error) {}

ValueStore::ReadResult::Inner::~Inner() {}

// Implementation of WriteResult.

ValueStore::WriteResult::WriteResult(
    ValueStoreChangeList* changes) : inner_(new Inner(changes, "")) {
  DCHECK(changes);
}

ValueStore::WriteResult::WriteResult(const std::string& error)
    : inner_(new Inner(NULL, error)) {
  DCHECK(!error.empty());
}

ValueStore::WriteResult::~WriteResult() {}

bool ValueStore::WriteResult::HasError() const {
  return !inner_->error_.empty();
}

const ValueStoreChangeList&
ValueStore::WriteResult::changes() const {
  DCHECK(!HasError());
  return *inner_->changes_;
}

const std::string& ValueStore::WriteResult::error() const {
  DCHECK(HasError());
  return inner_->error_;
}

ValueStore::WriteResult::Inner::Inner(
    ValueStoreChangeList* changes, const std::string& error)
    : changes_(changes), error_(error) {}

ValueStore::WriteResult::Inner::~Inner() {}
