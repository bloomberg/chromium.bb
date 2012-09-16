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
}  // namespace content

V8ValueConverterImpl::V8ValueConverterImpl()
    : undefined_allowed_(false),
      date_allowed_(false),
      regexp_allowed_(false),
      strip_null_from_objects_(false) {
}

bool V8ValueConverterImpl::GetUndefinedAllowed() const {
  return undefined_allowed_;
}

void V8ValueConverterImpl::SetUndefinedAllowed(bool val) {
  undefined_allowed_ = val;
}

bool V8ValueConverterImpl::GetDateAllowed() const {
  return date_allowed_;
}

void V8ValueConverterImpl::SetDateAllowed(bool val) {
  date_allowed_ = val;
}

bool V8ValueConverterImpl::GetRegexpAllowed() const {
  return regexp_allowed_;
}

void V8ValueConverterImpl::SetRegexpAllowed(bool val) {
  regexp_allowed_ = val;
}

bool V8ValueConverterImpl::GetStripNullFromObjects() const {
  return strip_null_from_objects_;
}

void V8ValueConverterImpl::SetStripNullFromObjects(bool val) {
  strip_null_from_objects_ = val;
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
  std::set<int> unique_set;
  return FromV8ValueImpl(val, &unique_set);
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
    const Value* child = NULL;
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
    const Value* child = NULL;
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

Value* V8ValueConverterImpl::FromV8ValueImpl(v8::Handle<v8::Value> val,
    std::set<int>* unique_set) const {
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

  if (undefined_allowed_ && val->IsUndefined())
    return Value::CreateNullValue();

  if (date_allowed_ && val->IsDate()) {
    v8::Date* date = v8::Date::Cast(*val);
    return Value::CreateDoubleValue(date->NumberValue() / 1000.0);
  }

  if (regexp_allowed_ && val->IsRegExp()) {
    return Value::CreateStringValue(
        *v8::String::Utf8Value(val->ToString()));
  }

  // v8::Value doesn't have a ToArray() method for some reason.
  if (val->IsArray())
    return FromV8Array(val.As<v8::Array>(), unique_set);

  if (val->IsObject()) {
    BinaryValue* binary_value = FromV8Buffer(val);
    if (binary_value) {
      return binary_value;
    } else {
      return FromV8Object(val->ToObject(), unique_set);
    }
  }
  LOG(ERROR) << "Unexpected v8 value type encountered.";
  return Value::CreateNullValue();
}

Value* V8ValueConverterImpl::FromV8Array(v8::Handle<v8::Array> val,
    std::set<int>* unique_set) const {
  if (unique_set && unique_set->count(val->GetIdentityHash()))
    return Value::CreateNullValue();

  scoped_ptr<v8::Context::Scope> scope;
  // If val was created in a different context than our current one, change to
  // that context, but change back after val is converted.
  if (!val->CreationContext().IsEmpty() &&
      val->CreationContext() != v8::Context::GetCurrent())
    scope.reset(new v8::Context::Scope(val->CreationContext()));

  ListValue* result = new ListValue();

  if (unique_set)
    unique_set->insert(val->GetIdentityHash());
  // Only fields with integer keys are carried over to the ListValue.
  for (uint32 i = 0; i < val->Length(); ++i) {
    v8::TryCatch try_catch;
    v8::Handle<v8::Value> child_v8 = val->Get(i);
    if (try_catch.HasCaught()) {
      LOG(ERROR) << "Getter for index " << i << " threw an exception.";
      child_v8 = v8::Null();
    }

    if (!val->HasRealIndexedProperty(i))
      continue;

    Value* child = FromV8ValueImpl(child_v8, unique_set);
    CHECK(child);

    result->Append(child);
  }
  return result;
}

base::BinaryValue* V8ValueConverterImpl::FromV8Buffer(
    v8::Handle<v8::Value> val) const {
  char* data = NULL;
  size_t length = 0;

  scoped_ptr<WebKit::WebArrayBuffer> array_buffer(
      WebKit::WebArrayBuffer::createFromV8Value(val));
  scoped_ptr<WebKit::WebArrayBufferView> view;
  if (array_buffer.get()) {
    data = reinterpret_cast<char*>(array_buffer->data());
    length = array_buffer->byteLength();
  } else {
    view.reset(WebKit::WebArrayBufferView::createFromV8Value(val));
    if (view.get()) {
      data = reinterpret_cast<char*>(view->baseAddress()) + view->byteOffset();
      length = view->byteLength();
    }
  }

  if (data)
    return base::BinaryValue::CreateWithCopiedBuffer(data, length);
  else
    return NULL;
}

Value* V8ValueConverterImpl::FromV8Object(
    v8::Handle<v8::Object> val,
    std::set<int>* unique_set) const {
  if (unique_set && unique_set->count(val->GetIdentityHash()))
    return Value::CreateNullValue();
  scoped_ptr<v8::Context::Scope> scope;
  // If val was created in a different context than our current one, change to
  // that context, but change back after val is converted.
  if (!val->CreationContext().IsEmpty() &&
      val->CreationContext() != v8::Context::GetCurrent())
    scope.reset(new v8::Context::Scope(val->CreationContext()));

  scoped_ptr<DictionaryValue> result(new DictionaryValue());
  v8::Handle<v8::Array> property_names(val->GetOwnPropertyNames());

  if (unique_set)
    unique_set->insert(val->GetIdentityHash());

  for (uint32 i = 0; i < property_names->Length(); ++i) {
    v8::Handle<v8::Value> key(property_names->Get(i));

    // Extend this test to cover more types as necessary and if sensible.
    if (!key->IsString() &&
        !key->IsNumber()) {
      NOTREACHED() << "Key \"" << *v8::String::AsciiValue(key) << "\" "
                      "is neither a string nor a number";
      continue;
    }

    // Skip all callbacks: crbug.com/139933
    if (val->HasRealNamedCallbackProperty(key->ToString()))
      continue;

    v8::String::Utf8Value name_utf8(key->ToString());

    v8::TryCatch try_catch;
    v8::Handle<v8::Value> child_v8 = val->Get(key);

    if (try_catch.HasCaught()) {
      LOG(ERROR) << "Getter for property " << *name_utf8
                 << " threw an exception.";
      child_v8 = v8::Null();
    }

    scoped_ptr<Value> child(FromV8ValueImpl(child_v8, unique_set));
    CHECK(child.get());

    // Strip null if asked (and since undefined is turned into null, undefined
    // too). The use case for supporting this is JSON-schema support,
    // specifically for extensions, where "optional" JSON properties may be
    // represented as null, yet due to buggy legacy code elsewhere isn't
    // treated as such (potentially causing crashes). For example, the
    // "tabs.create" function takes an object as its first argument with an
    // optional "windowId" property.
    //
    // Given just
    //
    //   tabs.create({})
    //
    // this will work as expected on code that only checks for the existence of
    // a "windowId" property (such as that legacy code). However given
    //
    //   tabs.create({windowId: null})
    //
    // there *is* a "windowId" property, but since it should be an int, code
    // on the browser which doesn't additionally check for null will fail.
    // We can avoid all bugs related to this by stripping null.
    if (strip_null_from_objects_ && child->IsType(Value::TYPE_NULL))
      continue;

    result->SetWithoutPathExpansion(std::string(*name_utf8, name_utf8.length()),
                                    child.release());
  }

  return result.release();
}
