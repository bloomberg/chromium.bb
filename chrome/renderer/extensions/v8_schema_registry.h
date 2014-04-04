// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_V8_SCHEMA_REGISTRY_H_
#define CHROME_RENDERER_EXTENSIONS_V8_SCHEMA_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/renderer/scoped_persistent.h"
#include "v8/include/v8-util.h"
#include "v8/include/v8.h"

namespace extensions {
class NativeHandler;

// A registry for the v8::Value representations of extension API schemas.
// In a way, the v8 counterpart to ExtensionAPI.
class V8SchemaRegistry {
 public:
  V8SchemaRegistry();
  ~V8SchemaRegistry();

  // Creates a NativeHandler wrapper |this|. Supports GetSchema.
  scoped_ptr<NativeHandler> AsNativeHandler();

  // Returns a v8::Array with all the schemas for the APIs in |apis|.
  v8::Handle<v8::Array> GetSchemas(const std::vector<std::string>& apis);

  // Returns a v8::Object for the schema for |api|, possibly from the cache.
  v8::Handle<v8::Object> GetSchema(const std::string& api);

 private:
  // Gets the separate context that backs the registry, creating a new one if
  // if necessary. Will also initialize schema_cache_.
  v8::Handle<v8::Context> GetOrCreateContext(v8::Isolate* isolate);

  // Cache of schemas. Created lazily by GetOrCreateContext.
  typedef v8::StdPersistentValueMap<std::string, v8::Object> SchemaCache;
  scoped_ptr<SchemaCache> schema_cache_;

  // Single per-instance v8::Context to create v8::Values.
  // Created lazily via GetOrCreateContext.
  ScopedPersistent<v8::Context> context_;

  DISALLOW_COPY_AND_ASSIGN(V8SchemaRegistry);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_V8_SCHEMA_REGISTRY_H_
