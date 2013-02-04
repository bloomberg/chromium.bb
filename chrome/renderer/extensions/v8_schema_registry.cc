// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/v8_schema_registry.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "content/public/renderer/v8_value_converter.h"

using content::V8ValueConverter;

namespace extensions {

V8SchemaRegistry::V8SchemaRegistry() : context_(v8::Context::New()) {}

V8SchemaRegistry::~V8SchemaRegistry() {
  v8::Isolate* isolate = context_->GetIsolate();
  for (SchemaCache::iterator i = schema_cache_.begin();
      i != schema_cache_.end(); ++i) {
    i->second.Dispose(isolate);
  }
  context_.Dispose(isolate);
}

v8::Handle<v8::Array> V8SchemaRegistry::GetSchemas(
    const std::set<std::string>& apis) {
  v8::Context::Scope context_scope(context_);
  v8::Handle<v8::Array> v8_apis(v8::Array::New(apis.size()));
  size_t api_index = 0;
  for (std::set<std::string>::const_iterator i = apis.begin(); i != apis.end();
      ++i) {
    v8_apis->Set(api_index++, GetSchema(*i));
  }
  return v8_apis;
}

v8::Handle<v8::Object> V8SchemaRegistry::GetSchema(const std::string& api) {
  SchemaCache::iterator maybe_schema = schema_cache_.find(api);
  if (maybe_schema != schema_cache_.end())
    return maybe_schema->second;

  const base::DictionaryValue* schema =
      ExtensionAPI::GetSharedInstance()->GetSchema(api);
  CHECK(schema) << api;

  scoped_ptr<V8ValueConverter> v8_value_converter(V8ValueConverter::create());
  v8::Persistent<v8::Object> v8_schema =
      v8::Persistent<v8::Object>::New(
          context_->GetIsolate(),
          v8::Handle<v8::Object>::Cast(
              v8_value_converter->ToV8Value(schema, context_)));
  CHECK(!v8_schema.IsEmpty());
  schema_cache_[api] = v8_schema;
  return v8_schema;
}

}  // namespace extensions
