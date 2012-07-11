// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MODULE_SYSTEM_H_
#define CHROME_RENDERER_MODULE_SYSTEM_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/renderer/native_handler.h"
#include "v8/include/v8.h"

#include <map>
#include <set>
#include <string>

// A module system for JS similar to node.js' require() function.
// Each module has three variables in the global scope:
//   - exports, an object returned to dependencies who require() this
//     module.
//   - require, a function that takes a module name as an argument and returns
//     that module's exports object.
//   - requireNative, a function that takes the name of a registered
//     NativeHandler and returns an object that contains the functions the
//     NativeHandler defines.
//
// Each module in a ModuleSystem is executed at most once and its exports
// object cached.
//
// Note that a ModuleSystem must be used only in conjunction with a single
// v8::Context.
// TODO(koz): Rename this to JavaScriptModuleSystem.
class ModuleSystem : public NativeHandler {
 public:
  class SourceMap {
   public:
    virtual v8::Handle<v8::Value> GetSource(const std::string& name) = 0;
    virtual bool Contains(const std::string& name) = 0;
  };

  // Enables native bindings for the duration of its lifetime.
  class NativesEnabledScope {
   public:
    explicit NativesEnabledScope(ModuleSystem* module_system);
    ~NativesEnabledScope();

   private:
    ModuleSystem* module_system_;
    DISALLOW_COPY_AND_ASSIGN(NativesEnabledScope);
  };

  // |source_map| is a weak pointer.
  explicit ModuleSystem(v8::Handle<v8::Context> context, SourceMap* source_map);
  virtual ~ModuleSystem();

  // Returns true if the current context has a ModuleSystem installed in it.
  static bool IsPresentInCurrentContext();

  // Dumps the given exception message to LOG(ERROR).
  static void DumpException(v8::Handle<v8::Message> message);

  // Require the specified module. This is the equivalent of calling
  // require('module_name') from the loaded JS files.
  void Require(const std::string& module_name);
  v8::Handle<v8::Value> Require(const v8::Arguments& args);
  v8::Handle<v8::Value> RequireForJs(const v8::Arguments& args);
  v8::Handle<v8::Value> RequireForJsInner(v8::Handle<v8::String> module_name);

  // Register |native_handler| as a potential target for requireNative(), so
  // calls to requireNative(|name|) from JS will return a new object created by
  // |native_handler|.
  void RegisterNativeHandler(const std::string& name,
                             scoped_ptr<NativeHandler> native_handler);

  // Causes requireNative(|name|) to look for its module in |source_map_|
  // instead of using a registered native handler. This can be used in unit
  // tests to mock out native modules.
  void OverrideNativeHandler(const std::string& name);

  // Executes |code| in the current context with |name| as the filename.
  void RunString(const std::string& code, const std::string& name);

  // Retrieves the lazily defined field specified by |property|.
  static v8::Handle<v8::Value> LazyFieldGetter(v8::Local<v8::String> property,
                                               const v8::AccessorInfo& info);

  // Make |object|.|field| lazily evaluate to the result of
  // require(|module_name|)[|module_field|].
  void SetLazyField(v8::Handle<v8::Object> object,
                    const std::string& field,
                    const std::string& module_name,
                    const std::string& module_field);

 private:
  typedef std::map<std::string, linked_ptr<NativeHandler> > NativeHandlerMap;

  // Ensure that require_ has been evaluated from require.js.
  void EnsureRequireLoaded();

  // Run |code| in the current context with the name |name| used for stack
  // traces.
  v8::Handle<v8::Value> RunString(v8::Handle<v8::String> code,
                                  v8::Handle<v8::String> name);

  // Return the named source file stored in the source map.
  // |args[0]| - the name of a source file in source_map_.
  v8::Handle<v8::Value> GetSource(v8::Handle<v8::String> source_name);

  // Return an object that contains the native methods defined by the named
  // NativeHandler.
  // |args[0]| - the name of a native handler object.
  v8::Handle<v8::Value> GetNative(const v8::Arguments& args);

  // Wraps |source| in a (function(require, requireNative, exports) {...}).
  v8::Handle<v8::String> WrapSource(v8::Handle<v8::String> source);

  // Throws an exception in the calling JS context.
  v8::Handle<v8::Value> ThrowException(const std::string& message);

  // The context that this ModuleSystem is for.
  v8::Persistent<v8::Context> context_;

  // A map from module names to the JS source for that module. GetSource()
  // performs a lookup on this map.
  SourceMap* source_map_;

  // A map from native handler names to native handlers.
  NativeHandlerMap native_handler_map_;

  // When 0, natives are disabled, otherwise indicates how many callers have
  // pinned natives as enabled.
  int natives_enabled_;

  std::set<std::string> overridden_native_handlers_;

  DISALLOW_COPY_AND_ASSIGN(ModuleSystem);
};

#endif  // CHROME_RENDERER_MODULE_SYSTEM_H_
