// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/v8_value_converter.h"

#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "v8/include/v8.h"

V8ValueConverter::V8ValueConverter() {
}

v8::Handle<v8::Value> V8ValueConverter::ToV8Value(
    Value* value, v8::Handle<v8::Context> context) {
  v8::Context::Scope context_scope(context);
  return ToV8ValueImpl(value);
}

Value* V8ValueConverter::FromV8Value(v8::Handle<v8::Value> val,
                                     v8::Handle<v8::Context> context) {
  v8::Context::Scope context_scope(context);
  return FromV8ValueImpl(val);
}

v8::Handle<v8::Value> V8ValueConverter::ToV8ValueImpl(Value* value) {
  CHECK(value);
  switch (value->GetType()) {
    case Value::TYPE_NULL:
      return v8::Null();

    case Value::TYPE_BOOLEAN: {
      bool val = false;
      CHECK(value->GetAsBoolean(&val));
      return v8::Boolean::New(val);
    }

    case Value::TYPE_INTEGER: {
      int val = 0;
      CHECK(value->GetAsInteger(&val));
      return v8::Integer::New(val);
    }

    case Value::TYPE_DOUBLE: {
      double val = 0.0;
      CHECK(value->GetAsDouble(&val));
      return v8::Number::New(val);
    }

    case Value::TYPE_STRING: {
      std::string val;
      CHECK(value->GetAsString(&val));
      return v8::String::New(val.c_str(), val.length());
    }

    case Value::TYPE_LIST:
      return ToV8Array(static_cast<ListValue*>(value));

    case Value::TYPE_DICTIONARY:
      return ToV8Object(static_cast<DictionaryValue*>(value));

    default:
      NOTREACHED() << "Unexpected value type: " << value->GetType();
      return v8::Null();
  }
}

v8::Handle<v8::Value> V8ValueConverter::ToV8Array(ListValue* val) {
  v8::Handle<v8::Array> result(v8::Array::New(val->GetSize()));

  for (size_t i = 0; i < val->GetSize(); ++i) {
    Value* child = NULL;
    CHECK(val->Get(i, &child));
    result->Set(static_cast<uint32>(i), ToV8ValueImpl(child));
  }

  return result;
}

v8::Handle<v8::Value> V8ValueConverter::ToV8Object(DictionaryValue* val) {
  v8::Handle<v8::Object> result(v8::Object::New());

  for (DictionaryValue::key_iterator iter = val->begin_keys();
       iter != val->end_keys(); ++iter) {
    Value* child = NULL;
    CHECK(val->GetWithoutPathExpansion(*iter, &child));
    const std::string& key = *iter;
    result->Set(v8::String::New(key.c_str(), key.length()),
                ToV8ValueImpl(child));
  }

  return result;
}

Value* V8ValueConverter::FromV8ValueImpl(v8::Handle<v8::Value> val) {
  if (val->IsNull())
    return Value::CreateNullValue();

  if (val->IsBoolean())
    return Value::CreateBooleanValue(val->ToBoolean()->Value());

  if (val->IsInt32())
    return Value::CreateIntegerValue(val->ToInt32()->Value());

  if (val->IsNumber())
    return Value::CreateDoubleValue(val->ToNumber()->Value());

  if (val->IsString()) {
    v8::String::Utf8Value utf8(val->ToString());
    return Value::CreateStringValue(std::string(*utf8, utf8.length()));
  }

  if (allow_date_ && val->IsDate()) {
    v8::Date* date = v8::Date::Cast(*val);
    return Value::CreateDoubleValue(date->NumberValue() / 1000.0);
  }

  if (allow_regexp_ && val->IsRegExp()) {
    return Value::CreateStringValue(
        *v8::String::Utf8Value(val->ToString()));
  }

  // v8::Value doesn't have a ToArray() method for some reason.
  if (val->IsArray())
    return FromV8Array(v8::Handle<v8::Array>::Cast(val));

  if (val->IsObject())
    return FromV8Object(val->ToObject());

  NOTREACHED() << "Unexpected v8::Value type.";
  return Value::CreateNullValue();
}

ListValue* V8ValueConverter::FromV8Array(v8::Handle<v8::Array> val) {
  ListValue* result = new ListValue();
  for (uint32 i = 0; i < val->Length(); ++i) {
    result->Append(FromV8ValueImpl(val->Get(i)));
  }
  return result;
}

DictionaryValue* V8ValueConverter::FromV8Object(v8::Handle<v8::Object> val) {
  DictionaryValue* result = new DictionaryValue();
  v8::Handle<v8::Array> property_names(val->GetPropertyNames());
  for (uint32 i = 0; i < property_names->Length(); ++i) {
    v8::Handle<v8::String> name(
        v8::Handle<v8::String>::Cast(property_names->Get(i)));
    v8::String::Utf8Value name_utf8(name->ToString());
    result->SetWithoutPathExpansion(std::string(*name_utf8, name_utf8.length()),
                                    FromV8ValueImpl(val->Get(name)));
  }
  return result;
}
