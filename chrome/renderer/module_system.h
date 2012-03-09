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
class ModuleSystem : public NativeHandler {
 public:
  // |source_map| is a weak pointer.
  explicit ModuleSystem(const std::map<std::string, std::string>* source_map);
  virtual ~ModuleSystem();

  // Require the specified module. This is the equivalent of calling
  // require('module_name') from the loaded JS files.
  void Require(const std::string& module_name);

  // Register |native_handler| as a potential target for requireNative(), so
  // calls to requireNative(|name|) from JS will return a new object created by
  // |native_handler|.
  void RegisterNativeHandler(const std::string& name,
      scoped_ptr<NativeHandler> native_handler);

 private:
  // Ensure that require_ has been evaluated from require.js.
  void EnsureRequireLoaded();

  // Run |code| in the current context.
  v8::Handle<v8::Value> RunString(v8::Handle<v8::String> code);

  // Run the given code in the current context.
  // |args[0]| - the code to execute.
  v8::Handle<v8::Value> Run(const v8::Arguments& args);

  // Return the named source file stored in the source map.
  // |args[0]| - the name of a source file in source_map_.
  v8::Handle<v8::Value> GetSource(const v8::Arguments& args);

  // Return an object that contains the native methods defined by the named
  // NativeHandler.
  // |args[0]| - the name of a native handler object.
  v8::Handle<v8::Value> GetNative(const v8::Arguments& args);

  // A map from module names to the JS source for that module. GetSource()
  // performs a lookup on this map.
  const std::map<std::string, std::string>* source_map_;
  typedef std::map<std::string, linked_ptr<NativeHandler> > NativeHandlerMap;
  NativeHandlerMap native_handler_map_;
  v8::Handle<v8::Function> require_;
};

#endif  // CHROME_RENDERER_MODULE_SYSTEM_H_
