// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/v8_value_converter_impl.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebArrayBuffer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebArrayBufferView.h"
#include "v8/include/v8.h"

using base::BinaryValue;
using base::DictionaryValue;
using base::ListValue;
using base::StringValue;
using base::Value;


namespace content {

V8ValueConverter* V8ValueConverter::create() {
  return new V8ValueConverterImpl();
}

}

V8ValueConverterImpl::V8ValueConverterImpl()
    : allow_undefined_(false),
      allow_date_(false),
      allow_regexp_(false) {
}

v8::Handle<v8::Value> V8ValueConverterImpl::ToV8Value(
    const Value* value, v8::Handle<v8::Context> context) const {
  v8::Context::Scope context_scope(context);
  v8::HandleScope handle_scope;
  return handle_scope.Close(ToV8ValueImpl(value));
}

Value* V8ValueConverterImpl::FromV8Value(
    v8::Handle<v8::Value> val,
    v8::Handle<v8::Context> context) const {
  v8::Context::Scope context_scope(context);
  v8::HandleScope handle_scope;
  return FromV8ValueImpl(val);
}

v8::Handle<v8::Value> V8ValueConverterImpl::ToV8ValueImpl(
     const Value* value) const {
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
      return ToV8Array(static_cast<const ListValue*>(value));

    case Value::TYPE_DICTIONARY:
      return ToV8Object(static_cast<const DictionaryValue*>(value));

    case Value::TYPE_BINARY:
      return ToArrayBuffer(static_cast<const BinaryValue*>(value));

    default:
      LOG(ERROR) << "Unexpected value type: " << value->GetType();
      return v8::Null();
  }
}

v8::Handle<v8::Value> V8ValueConverterImpl::ToV8Array(
    const ListValue* val) const {
  v8::Handle<v8::Array> result(v8::Array::New(val->GetSize()));

  for (size_t i = 0; i < val->GetSize(); ++i) {
    Value* child = NULL;
    CHECK(val->Get(i, &child));

    v8::Handle<v8::Value> child_v8 = ToV8ValueImpl(child);
    CHECK(!child_v8.IsEmpty());

    v8::TryCatch try_catch;
    result->Set(static_cast<uint32>(i), child_v8);
    if (try_catch.HasCaught())
      LOG(ERROR) << "Setter for index " << i << " threw an exception.";
  }

  return result;
}

v8::Handle<v8::Value> V8ValueConverterImpl::ToV8Object(
    const DictionaryValue* val) const {
  v8::Handle<v8::Object> result(v8::Object::New());

  for (DictionaryValue::key_iterator iter = val->begin_keys();
       iter != val->end_keys(); ++iter) {
    Value* child = NULL;
    CHECK(val->GetWithoutPathExpansion(*iter, &child));

    const std::string& key = *iter;
    v8::Handle<v8::Value> child_v8 = ToV8ValueImpl(child);
    CHECK(!child_v8.IsEmpty());

    v8::TryCatch try_catch;
    result->Set(v8::String::New(key.c_str(), key.length()), child_v8);
    if (try_catch.HasCaught()) {
      LOG(ERROR) << "Setter for property " << key.c_str() << " threw an "
                 << "exception.";
    }
  }

  return result;
}

v8::Handle<v8::Value> V8ValueConverterImpl::ToArrayBuffer(
    const BinaryValue* value) const {
  WebKit::WebArrayBuffer buffer =
      WebKit::WebArrayBuffer::create(value->GetSize(), 1);
  memcpy(buffer.data(), value->GetBuffer(), value->GetSize());
  return buffer.toV8Value();
}

Value* V8ValueConverterImpl::FromV8ValueImpl(v8::Handle<v8::Value> val) const {
  CHECK(!val.IsEmpty());

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

  if (allow_undefined_ && val->IsUndefined())
    return Value::CreateNullValue();

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
    return FromV8Array(val.As<v8::Array>());

  if (val->IsObject()) {
    BinaryValue* binary_value = FromV8Buffer(val);
    if (binary_value) {
      return binary_value;
    } else {
      return FromV8Object(val->ToObject());
    }
  }
  LOG(ERROR) << "Unexpected v8 value type encountered.";
  return Value::CreateNullValue();
}

ListValue* V8ValueConverterImpl::FromV8Array(v8::Handle<v8::Array> val) const {
  ListValue* result = new ListValue();
  for (uint32 i = 0; i < val->Length(); ++i) {
    v8::TryCatch try_catch;
    v8::Handle<v8::Value> child_v8 = val->Get(i);
    if (try_catch.HasCaught()) {
      LOG(ERROR) << "Getter for index " << i << " threw an exception.";
      child_v8 = v8::Null();
    }

    // TODO(aa): It would be nice to support getters, but we need
    // http://code.google.com/p/v8/issues/detail?id=1342 to do it properly.
    if (!val->HasRealIndexedProperty(i))
      continue;

    Value* child = FromV8ValueImpl(child_v8);
    CHECK(child);

    result->Append(child);
  }
  return result;
}

base::BinaryValue* V8ValueConverterImpl::FromV8Buffer(
    v8::Handle<v8::Value> val) const {
  char* data = NULL;
  size_t length = 0;

  WebKit::WebArrayBuffer* array_buffer =
      WebKit::WebArrayBuffer::createFromV8Value(val);
  if (array_buffer) {
    data = reinterpret_cast<char*>(array_buffer->data());
    length = array_buffer->byteLength();
  } else {
    WebKit::WebArrayBufferView* view =
        WebKit::WebArrayBufferView::createFromV8Value(val);
    if (view) {
      data = reinterpret_cast<char*>(view->baseAddress()) + view->byteOffset();
      length = view->byteLength();
    }
  }

  if (data)
    return base::BinaryValue::CreateWithCopiedBuffer(data, length);
  else
    return NULL;
}

DictionaryValue* V8ValueConverterImpl::FromV8Object(
    v8::Handle<v8::Object> val) const {
  DictionaryValue* result = new DictionaryValue();
  v8::Handle<v8::Array> property_names(val->GetPropertyNames());
  for (uint32 i = 0; i < property_names->Length(); ++i) {
    v8::Handle<v8::String> name(property_names->Get(i).As<v8::String>());

    // TODO(aa): It would be nice to support getters, but we need
    // http://code.google.com/p/v8/issues/detail?id=1342 to do it properly.
    if (!val->HasRealNamedProperty(name))
      continue;

    v8::String::Utf8Value name_utf8(name->ToString());

    v8::TryCatch try_catch;
    v8::Handle<v8::Value> child_v8 = val->Get(name);
    if (try_catch.HasCaught()) {
      LOG(ERROR) << "Getter for property " << *name_utf8
                 << " threw an exception.";
      child_v8 = v8::Null();
    }

    Value* child = FromV8ValueImpl(child_v8);
    CHECK(child);

    result->SetWithoutPathExpansion(std::string(*name_utf8, name_utf8.length()),
                                    child);
  }
  return result;
}
