// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/screen_context.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

namespace chromeos {

ScreenContext::ScreenContext() {
}

ScreenContext::~ScreenContext() {
}

bool ScreenContext::SetBoolean(const KeyType& key, bool value) {
  return Set(key, new base::FundamentalValue(value));
}

bool ScreenContext::SetInteger(const KeyType& key, int value) {
  return Set(key, new base::FundamentalValue(value));
}

bool ScreenContext::SetDouble(const KeyType& key, double value) {
  return Set(key, new base::FundamentalValue(value));
}

bool ScreenContext::SetString(const KeyType& key, const std::string& value) {
  return Set(key, new base::StringValue(value));
}

bool ScreenContext::SetString(const KeyType& key, const base::string16& value) {
  return Set(key, new base::StringValue(value));
}

bool ScreenContext::GetBoolean(const KeyType& key) {
  return Get<bool>(key);
}

bool ScreenContext::GetBoolean(const KeyType& key, bool default_value) {
  return Get(key, default_value);
}

int ScreenContext::GetInteger(const KeyType& key) {
  return Get<int>(key);
}

int ScreenContext::GetInteger(const KeyType& key, int default_value) {
  return Get(key, default_value);
}

double ScreenContext::GetDouble(const KeyType& key) {
  return Get<double>(key);
}

double ScreenContext::GetDouble(const KeyType& key, double default_value) {
  return Get(key, default_value);
}

std::string ScreenContext::GetString(const KeyType& key) {
  return Get<std::string>(key);
}

std::string ScreenContext::GetString(const KeyType& key,
                                     const std::string& default_value) {
  return Get(key, default_value);
}

base::string16 ScreenContext::GetString16(const KeyType& key) {
  return Get<base::string16>(key);
}

base::string16 ScreenContext::GetString16(const KeyType& key,
                                          const base::string16& default_value) {
  return Get(key, default_value);
}

bool ScreenContext::HasKey(const KeyType& key) const {
  DCHECK(CalledOnValidThread());
  return storage_.HasKey(key);
}

bool ScreenContext::HasChanges() const {
  DCHECK(CalledOnValidThread());
  return !changes_.empty();
}

void ScreenContext::GetChangesAndReset(DictionaryValue* diff) {
  DCHECK(CalledOnValidThread());
  DCHECK(diff);
  changes_.Swap(diff);
  changes_.Clear();
}

void ScreenContext::ApplyChanges(const DictionaryValue& diff,
                                 std::vector<std::string>* keys) {
  DCHECK(CalledOnValidThread());
  DCHECK(!HasChanges());
  DCHECK(keys);
  keys->clear();
  keys->reserve(diff.size());
  base::DictionaryValue::Iterator it(diff);
  while (!it.IsAtEnd()) {
    Set(it.key(), it.value().DeepCopy());
    keys->push_back(it.key());
    it.Advance();
  }
  changes_.Clear();
}

bool ScreenContext::Set(const KeyType& key, Value* value) {
  DCHECK(CalledOnValidThread());
  DCHECK(value);
  scoped_ptr<Value> new_value(value);

  Value* current_value;
  bool in_storage = storage_.Get(key, &current_value);

  // Don't do anything if |storage_| already contains <|key|, |new_value|> pair.
  if (in_storage && new_value->Equals(current_value))
    return false;

  changes_.Set(key, new_value->DeepCopy());
  storage_.Set(key, new_value.release());
  return true;
}

}  // namespace chromeos
