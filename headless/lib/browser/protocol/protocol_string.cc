// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/protocol_string.h"

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "headless/lib/browser/protocol/protocol.h"

namespace headless {
namespace protocol {

std::unique_ptr<Value> toProtocolValue(const base::Value* value, int depth) {
  if (!value || !depth)
    return nullptr;
  if (value->is_none())
    return Value::null();
  if (value->is_bool()) {
    bool inner;
    value->GetAsBoolean(&inner);
    return FundamentalValue::create(inner);
  }
  if (value->is_int()) {
    int inner;
    value->GetAsInteger(&inner);
    return FundamentalValue::create(inner);
  }
  if (value->is_double()) {
    double inner;
    value->GetAsDouble(&inner);
    return FundamentalValue::create(inner);
  }
  if (value->is_string()) {
    std::string inner;
    value->GetAsString(&inner);
    return StringValue::create(inner);
  }
  if (value->is_list()) {
    const base::ListValue* list = nullptr;
    value->GetAsList(&list);
    std::unique_ptr<ListValue> result = ListValue::create();
    for (size_t i = 0; i < list->GetSize(); i++) {
      const base::Value* item = nullptr;
      list->Get(i, &item);
      std::unique_ptr<Value> converted = toProtocolValue(item, depth - 1);
      if (converted)
        result->pushValue(std::move(converted));
    }
    return std::move(result);
  }
  if (value->is_dict()) {
    const base::DictionaryValue* dictionary = nullptr;
    value->GetAsDictionary(&dictionary);
    std::unique_ptr<DictionaryValue> result = DictionaryValue::create();
    for (base::DictionaryValue::Iterator it(*dictionary); !it.IsAtEnd();
         it.Advance()) {
      std::unique_ptr<Value> converted =
          toProtocolValue(&it.value(), depth - 1);
      if (converted)
        result->setValue(it.key(), std::move(converted));
    }
    return std::move(result);
  }
  return nullptr;
}

std::unique_ptr<base::Value> toBaseValue(Value* value, int depth) {
  if (!value || !depth)
    return nullptr;
  if (value->type() == Value::TypeNull)
    return std::make_unique<base::Value>();
  if (value->type() == Value::TypeBoolean) {
    bool inner;
    value->asBoolean(&inner);
    return base::WrapUnique(new base::Value(inner));
  }
  if (value->type() == Value::TypeInteger) {
    int inner;
    value->asInteger(&inner);
    return base::WrapUnique(new base::Value(inner));
  }
  if (value->type() == Value::TypeDouble) {
    double inner;
    value->asDouble(&inner);
    return base::WrapUnique(new base::Value(inner));
  }
  if (value->type() == Value::TypeString) {
    std::string inner;
    value->asString(&inner);
    return base::WrapUnique(new base::Value(inner));
  }
  if (value->type() == Value::TypeArray) {
    ListValue* list = ListValue::cast(value);
    std::unique_ptr<base::ListValue> result(new base::ListValue());
    for (size_t i = 0; i < list->size(); i++) {
      std::unique_ptr<base::Value> converted =
          toBaseValue(list->at(i), depth - 1);
      if (converted)
        result->Append(std::move(converted));
    }
    return std::move(result);
  }
  if (value->type() == Value::TypeObject) {
    DictionaryValue* dict = DictionaryValue::cast(value);
    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    for (size_t i = 0; i < dict->size(); i++) {
      DictionaryValue::Entry entry = dict->at(i);
      std::unique_ptr<base::Value> converted =
          toBaseValue(entry.second, depth - 1);
      if (converted)
        result->SetWithoutPathExpansion(entry.first, std::move(converted));
    }
    return std::move(result);
  }
  return nullptr;
}

// static
std::unique_ptr<Value> StringUtil::parseJSON(const std::string& json) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(json);
  return toProtocolValue(value.get(), 1000);
}

StringBuilder::StringBuilder() {}

StringBuilder::~StringBuilder() {}

void StringBuilder::append(const std::string& s) {
  string_ += s;
}

void StringBuilder::append(char c) {
  string_ += c;
}

void StringBuilder::append(const char* characters, size_t length) {
  string_.append(characters, length);
}

// static
void StringUtil::builderAppendQuotedString(StringBuilder& builder,
                                           const String& str) {
  builder.append('"');
  base::string16 str16 = base::UTF8ToUTF16(str);
  escapeWideStringForJSON(reinterpret_cast<const uint16_t*>(&str16[0]),
                          str16.length(), &builder);
  builder.append('"');
}

std::string StringBuilder::toString() {
  return string_;
}

void StringBuilder::reserveCapacity(size_t capacity) {
  string_.reserve(capacity);
}

}  // namespace protocol
}  // namespace headless
