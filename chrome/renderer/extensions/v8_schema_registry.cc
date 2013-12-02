// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dcarney): Remove this when UnsafePersistent is removed.
#define V8_ALLOW_ACCESS_TO_RAW_HANDLE_CONSTRUCTOR

#include "chrome/renderer/extensions/v8_schema_registry.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/object_backed_native_handler.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_api.h"

using content::V8ValueConverter;

namespace extensions {

namespace {

class SchemaRegistryNativeHandler : public ObjectBackedNativeHandler {
 public:
  SchemaRegistryNativeHandler(V8SchemaRegistry* registry,
                              scoped_ptr<ChromeV8Context> context)
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

  scoped_ptr<ChromeV8Context> context_;
  V8SchemaRegistry* registry_;
};

}  // namespace

V8SchemaRegistry::V8SchemaRegistry() {}

V8SchemaRegistry::~V8SchemaRegistry() {
  for (SchemaCache::iterator i = schema_cache_.begin();
       i != schema_cache_.end(); ++i) {
    i->second.dispose();
  }
}

scoped_ptr<NativeHandler> V8SchemaRegistry::AsNativeHandler() {
  scoped_ptr<ChromeV8Context> context(new ChromeV8Context(
      GetOrCreateContext(v8::Isolate::GetCurrent()),
      NULL,  // no frame
      NULL,  // no extension
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
       i != apis.end(); ++i) {
    v8_apis->Set(api_index++, GetSchema(*i));
  }
  return handle_scope.Escape(v8_apis);
}

v8::Handle<v8::Object> V8SchemaRegistry::GetSchema(const std::string& api) {

  SchemaCache::iterator maybe_schema = schema_cache_.find(api);
  if (maybe_schema != schema_cache_.end())
    return maybe_schema->second.newLocal(v8::Isolate::GetCurrent());

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

  v8::Persistent<v8::Object> v8_schema(context->GetIsolate(),
                                       v8::Handle<v8::Object>::Cast(value));
  v8::Local<v8::Object> to_return =
      v8::Local<v8::Object>::New(isolate, v8_schema);
  schema_cache_[api] = UnsafePersistent<v8::Object>(&v8_schema);
  return handle_scope.Escape(to_return);
}

v8::Handle<v8::Context> V8SchemaRegistry::GetOrCreateContext(
    v8::Isolate* isolate) {
  // It's ok to create local handles in this function, since this is only called
  // when we have a HandleScope.
  if (context_.IsEmpty()) {
    v8::Handle<v8::Context> context = v8::Context::New(isolate);
    context_.reset(context);
    return context;
  }
  return context_.NewHandle(isolate);
}

}  // namespace extensions
