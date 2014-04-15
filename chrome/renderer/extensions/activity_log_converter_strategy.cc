// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/activity_log_converter_strategy.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/extensions/ad_injection_constants.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

typedef ActivityLogConverterStrategy::FromV8ValueCallback FromV8ValueCallback;

namespace constants = ad_injection_constants;
namespace keys = constants::keys;

const char kFirstChildProperty[] = "firstElementChild";
const char kNextElementSiblingProperty[] = "nextElementSibling";

scoped_ptr<base::DictionaryValue> ParseV8Object(
    v8::Isolate* isolate,
    v8::Object* object,
    const FromV8ValueCallback& callback);

// Get a property from a V8 object without entering javascript. We use this
// in order to examine the objects, while ensuring that we don't cause any
// change in the running program.
v8::Local<v8::Value> SafeGetProperty(v8::Isolate* isolate,
                                     v8::Object* object,
                                     const char* key) {
  v8::TryCatch try_catch;
  v8::Isolate::DisallowJavascriptExecutionScope scope(
      isolate, v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  v8::Local<v8::String> key_string = v8::String::NewFromUtf8(isolate, key);
  v8::Local<v8::Value> value = object->Get(key_string);
  if (try_catch.HasCaught() || value.IsEmpty() || value->IsUndefined() ||
      value->IsNull()) {
    return v8::Local<v8::Value>();
  }
  return value;
}

// Append a property to the given |dict| from the given |object| if the
// property exists on |object| and can be accessed safely (i.e., without
// triggering any javascript execution).
void MaybeAppendV8Property(v8::Isolate* isolate,
                           v8::Object* object,
                           const char* property_name,
                           base::DictionaryValue* dict,
                           const FromV8ValueCallback& callback) {
  v8::Handle<v8::Value> value = SafeGetProperty(isolate, object, property_name);
  if (!value.IsEmpty()) {
    scoped_ptr<base::Value> parsed_value(callback.Run(value, isolate));
    if (parsed_value.get())
      dict->Set(property_name, parsed_value.release());
  }
}

// Parse the children of a V8 |object| and return them as a list. This will
// return an empty scoped_ptr if no children are present, or if the children
// cannot be read safely (without triggering javascript).
scoped_ptr<base::ListValue> MaybeParseV8Children(
    v8::Isolate* isolate,
    v8::Object* object,
    const FromV8ValueCallback& callback) {
  scoped_ptr<base::ListValue> parsed_children(new base::ListValue());
  v8::Local<v8::Value> child_value =
      SafeGetProperty(isolate, object, kFirstChildProperty);
  size_t checked_children = 0u;
  while (!child_value.IsEmpty() &&
         child_value->IsObject() &&
         checked_children < constants::kMaximumChildrenToCheck) {
    ++checked_children;
    v8::Handle<v8::Object> child_object = child_value->ToObject();
    scoped_ptr<base::Value> parsed_child(
        callback.Run(child_object, isolate));
    if (parsed_child.get())
      parsed_children->Append(parsed_child.release());
    child_value =
        SafeGetProperty(isolate, *child_object, kNextElementSiblingProperty);
  }

  return parsed_children->GetSize() > 0 ? parsed_children.Pass()
                                        : scoped_ptr<base::ListValue>();
}

// Parse a V8 |object| into a DictionaryValue. This will examine the object
// for a few important properties, including:
// - href
// - src
// - children
// These properties are necessary to analyze whether or not the object contains
// ads, which may have been injected.
scoped_ptr<base::DictionaryValue> ParseV8Object(
    v8::Isolate* isolate,
    v8::Object* object,
    const FromV8ValueCallback& callback) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString(keys::kType,
                  *v8::String::Utf8Value(object->GetConstructorName()));

  MaybeAppendV8Property(isolate, object, keys::kHref, dict.get(), callback);
  MaybeAppendV8Property(isolate, object, keys::kSrc, dict.get(), callback);

  scoped_ptr<base::ListValue> maybe_children =
      MaybeParseV8Children(isolate, object, callback);
  if (maybe_children.get())
    dict->Set(keys::kChildren, maybe_children.release());

  return dict.Pass();
}

// Summarize a V8 value. This performs a shallow conversion in all cases, and
// returns only a string with a description of the value (e.g.,
// "[HTMLElement]").
scoped_ptr<base::Value> SummarizeV8Value(v8::Isolate* isolate,
                                         v8::Handle<v8::Object> object) {
  v8::TryCatch try_catch;
  v8::Isolate::DisallowJavascriptExecutionScope scope(
      isolate, v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  v8::Local<v8::String> name = v8::String::NewFromUtf8(isolate, "[");
  if (object->IsFunction()) {
    name =
        v8::String::Concat(name, v8::String::NewFromUtf8(isolate, "Function"));
    v8::Local<v8::Value> fname =
        v8::Handle<v8::Function>::Cast(object)->GetName();
    if (fname->IsString() && v8::Handle<v8::String>::Cast(fname)->Length()) {
      name = v8::String::Concat(name, v8::String::NewFromUtf8(isolate, " "));
      name = v8::String::Concat(name, v8::Handle<v8::String>::Cast(fname));
      name = v8::String::Concat(name, v8::String::NewFromUtf8(isolate, "()"));
    }
  } else {
    name = v8::String::Concat(name, object->GetConstructorName());
  }
  name = v8::String::Concat(name, v8::String::NewFromUtf8(isolate, "]"));

  if (try_catch.HasCaught()) {
    return scoped_ptr<base::Value>(
        new base::StringValue("[JS Execution Exception]"));
  }

  return scoped_ptr<base::Value>(
      new base::StringValue(std::string(*v8::String::Utf8Value(name))));
}

}  // namespace

ActivityLogConverterStrategy::ActivityLogConverterStrategy()
    : enable_detailed_parsing_(false) {}

ActivityLogConverterStrategy::~ActivityLogConverterStrategy() {}

bool ActivityLogConverterStrategy::FromV8Object(
    v8::Handle<v8::Object> value,
    base::Value** out,
    v8::Isolate* isolate,
    const FromV8ValueCallback& callback) const {
  return FromV8Internal(value, out, isolate, callback);
}

bool ActivityLogConverterStrategy::FromV8Array(
    v8::Handle<v8::Array> value,
    base::Value** out,
    v8::Isolate* isolate,
    const FromV8ValueCallback& callback) const {
  return FromV8Internal(value, out, isolate, callback);
}

bool ActivityLogConverterStrategy::FromV8Internal(
    v8::Handle<v8::Object> value,
    base::Value** out,
    v8::Isolate* isolate,
    const FromV8ValueCallback& callback) const {
  scoped_ptr<base::Value> parsed_value;
  if (enable_detailed_parsing_)
    parsed_value = ParseV8Object(isolate, *value, callback);
  if (!parsed_value.get())
    parsed_value = SummarizeV8Value(isolate, value);
  *out = parsed_value.release();

  return true;
}

}  // namespace extensions
