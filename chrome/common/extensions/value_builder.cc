// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/value_builder.h"

using base::DictionaryValue;
using base::ListValue;

namespace extensions {

// DictionaryBuilder

DictionaryBuilder::DictionaryBuilder() : dict_(new DictionaryValue) {}

DictionaryBuilder::DictionaryBuilder(const DictionaryValue& init)
    : dict_(init.DeepCopy()) {}

DictionaryBuilder::~DictionaryBuilder() {}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          int in_value) {
  dict_->SetWithoutPathExpansion(path, Value::CreateIntegerValue(in_value));
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          double in_value) {
  dict_->SetWithoutPathExpansion(path, Value::CreateDoubleValue(in_value));
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          const std::string& in_value) {
  dict_->SetWithoutPathExpansion(path, Value::CreateStringValue(in_value));
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          const string16& in_value) {
  dict_->SetWithoutPathExpansion(path, Value::CreateStringValue(in_value));
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          DictionaryBuilder& in_value) {
  dict_->SetWithoutPathExpansion(path, in_value.Build().release());
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          ListBuilder& in_value) {
  dict_->SetWithoutPathExpansion(path, in_value.Build().release());
  return *this;
}

DictionaryBuilder& DictionaryBuilder::SetBoolean(
    const std::string& path, bool in_value) {
  dict_->SetWithoutPathExpansion(path, Value::CreateBooleanValue(in_value));
  return *this;
}

// ListBuilder

ListBuilder::ListBuilder() : list_(new ListValue) {}
ListBuilder::ListBuilder(const ListValue& init) : list_(init.DeepCopy()) {}
ListBuilder::~ListBuilder() {}

ListBuilder& ListBuilder::Append(int in_value) {
  list_->Append(Value::CreateIntegerValue(in_value));
  return *this;
}

ListBuilder& ListBuilder::Append(double in_value) {
  list_->Append(Value::CreateDoubleValue(in_value));
  return *this;
}

ListBuilder& ListBuilder::Append(const std::string& in_value) {
  list_->Append(Value::CreateStringValue(in_value));
  return *this;
}

ListBuilder& ListBuilder::Append(const string16& in_value) {
  list_->Append(Value::CreateStringValue(in_value));
  return *this;
}

ListBuilder& ListBuilder::Append(DictionaryBuilder& in_value) {
  list_->Append(in_value.Build().release());
  return *this;
}

ListBuilder& ListBuilder::Append(ListBuilder& in_value) {
  list_->Append(in_value.Build().release());
  return *this;
}

ListBuilder& ListBuilder::AppendBoolean(bool in_value) {
  list_->Append(Value::CreateBooleanValue(in_value));
  return *this;
}

}  // namespace extensions
