// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_V8_SCHEMA_REGISTRY_H_
#define CHROME_RENDERER_EXTENSIONS_V8_SCHEMA_REGISTRY_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "v8/include/v8.h"

namespace extensions {

// A registry for the v8::Value representations of extension API schemas.
// In a way, the v8 counterpart to ExtensionAPI.
class V8SchemaRegistry {
 public:
  V8SchemaRegistry();
  ~V8SchemaRegistry();

  // Returns a v8::Array with all the schemas for the APIs in |apis|.
  v8::Handle<v8::Array> GetSchemas(const std::set<std::string>& apis);

 private:
  // Returns a v8::Object for the schema for |api|, possibly from the cache.
  v8::Handle<v8::Object> GetSchema(const std::string& api);

  // Cache of schemas.
  typedef std::map<std::string, v8::Persistent<v8::Object> > SchemaCache;
  SchemaCache schema_cache_;

  // Single per-instance v8::Context to create v8::Values.
  v8::Persistent<v8::Context> context_;

  DISALLOW_COPY_AND_ASSIGN(V8SchemaRegistry);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_V8_SCHEMA_REGISTRY_H_
