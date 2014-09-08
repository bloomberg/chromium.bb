// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/v8_schema_registry.h"

#include "base/logging.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/script_context.h"

using content::V8ValueConverter;

namespace extensions {

namespace {

class SchemaRegistryNativeHandler : public ObjectBackedNativeHandler {
 public:
  SchemaRegistryNativeHandler(V8SchemaRegistry* registry,
                              scoped_ptr<ScriptContext> context)
      : ObjectBackedNativeHandler(context.get()),
        context_(context.Pass()),
        registry_(registry) {
    RouteFunction("GetSchema",
                  base::Bind(&SchemaRegistryNativeHandler::GetSchema,
                             base::Unretained(this)));
  }

 private:
  void GetSchema(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(
        registry_->GetSchema(*v8::String::Utf8Value(args[0])));
  }

  scoped_ptr<ScriptContext> context_;
  V8SchemaRegistry* registry_;
};

}  // namespace

V8SchemaRegistry::V8SchemaRegistry() {
}

V8SchemaRegistry::~V8SchemaRegistry() {
}

scoped_ptr<NativeHandler> V8SchemaRegistry::AsNativeHandler() {
  scoped_ptr<ScriptContext> context(
      new ScriptContext(GetOrCreateContext(v8::Isolate::GetCurrent()),
                        NULL,  // no frame
                        NULL,  // no extension
                        Feature::UNSPECIFIED_CONTEXT,
                        NULL,  // no effective extension
                        Feature::UNSPECIFIED_CONTEXT));
  return scoped_ptr<NativeHandler>(
      new SchemaRegistryNativeHandler(this, context.Pass()));
}

v8::Handle<v8::Array> V8SchemaRegistry::GetSchemas(
    const std::vector<std::string>& apis) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(GetOrCreateContext(isolate));

  v8::Local<v8::Array> v8_apis(v8::Array::New(isolate, apis.size()));
  size_t api_index = 0;
  for (std::vector<std::string>::const_iterator i = apis.begin();
       i != apis.end();
       ++i) {
    v8_apis->Set(api_index++, GetSchema(*i));
  }
  return handle_scope.Escape(v8_apis);
}

v8::Handle<v8::Object> V8SchemaRegistry::GetSchema(const std::string& api) {
  if (schema_cache_ != NULL) {
    v8::Local<v8::Object> cached_schema = schema_cache_->Get(api);
    if (!cached_schema.IsEmpty()) {
      return cached_schema;
    }
  }

  // Slow path: Need to build schema first.

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = GetOrCreateContext(isolate);
  v8::Context::Scope context_scope(context);

  const base::DictionaryValue* schema =
      ExtensionAPI::GetSharedInstance()->GetSchema(api);
  CHECK(schema) << api;
  scoped_ptr<V8ValueConverter> v8_value_converter(V8ValueConverter::create());
  v8::Handle<v8::Value> value = v8_value_converter->ToV8Value(schema, context);
  CHECK(!value.IsEmpty());

  v8::Local<v8::Object> v8_schema(v8::Handle<v8::Object>::Cast(value));
  schema_cache_->Set(api, v8_schema);

  return handle_scope.Escape(v8_schema);
}

v8::Handle<v8::Context> V8SchemaRegistry::GetOrCreateContext(
    v8::Isolate* isolate) {
  // It's ok to create local handles in this function, since this is only called
  // when we have a HandleScope.
  if (!context_holder_) {
    context_holder_.reset(new gin::ContextHolder(isolate));
    context_holder_->SetContext(v8::Context::New(isolate));
    schema_cache_.reset(new SchemaCache(isolate));
    return context_holder_->context();
  }
  return context_holder_->context();
}

}  // namespace extensions
