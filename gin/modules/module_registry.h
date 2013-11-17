// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_MODULES_MODULE_REGISTRY_H_
#define GIN_MODULES_MODULE_REGISTRY_H_

#include <list>
#include <string>
#include "base/compiler_specific.h"
#include "gin/per_context_data.h"

namespace gin {

struct PendingModule;

// This class implements the Asynchronous Module Definition (AMD) API.
// https://github.com/amdjs/amdjs-api/wiki/AMD
//
// Our implementation isn't complete yet. Missing features:
//   1) Built-in support for require, exports, and module.
//   2) Path resoltuion in module names.
//
// For these reasons, we don't have an "amd" property on the "define"
// function. The spec says we should only add that property once our
// implementation complies with the specification.
//
class ModuleRegistry : public ContextSupplement {
 public:
  static ModuleRegistry* From(v8::Handle<v8::Context> context);

  static void RegisterGlobals(v8::Isolate* isolate,
                              v8::Handle<v8::ObjectTemplate> templ);

  // The caller must have already entered our context.
  void AddBuiltinModule(v8::Isolate* isolate,
                        const std::string& id,
                        v8::Handle<v8::ObjectTemplate> templ);

  // Takes ownership of |pending|. The caller must have already entered
  // our context.
  void AddPendingModule(v8::Isolate* isolate, PendingModule* pending);

 private:
  typedef std::list<PendingModule*> PendingModuleList;  // Owning reference.

  explicit ModuleRegistry(v8::Isolate* isolate);
  virtual ~ModuleRegistry();

  // From ContextSupplement:
  virtual void Detach(v8::Handle<v8::Context> context) OVERRIDE;

  // Takes ownership of |pending|.
  bool AttemptToLoad(v8::Isolate* isolate, PendingModule* pending);
  void AttemptToLoadPendingModules(v8::Isolate* isolate);

  v8::Persistent<v8::Object> modules_;
  PendingModuleList pending_modules_;

  DISALLOW_COPY_AND_ASSIGN(ModuleRegistry);
};

}  // namespace gin

#endif  // GIN_MODULES_MODULE_REGISTRY_H_
