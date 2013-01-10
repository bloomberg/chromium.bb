// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_NATIVE_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_NATIVE_HANDLER_H_

#include "base/bind.h"
#include "v8/include/v8.h"

#include <string>

namespace extensions {

// A NativeHandler is a factory for JS objects with functions on them that map
// to native C++ functions. Subclasses should call RouteFunction() in their
// constructor to define functions on the created JS objects.
//
// NativeHandlers are intended to be used with a ModuleSystem. The ModuleSystem
// will assume ownership of the NativeHandler, and as a ModuleSystem is tied to
// a single v8::Context, this implies that NativeHandlers will also be tied to
// a single v8::context.
// TODO(koz): Rename this to NativeJavaScriptModule.
class NativeHandler {
 public:
  explicit NativeHandler();
  virtual ~NativeHandler();

  // Create an object with bindings to the native functions defined through
  // RouteFunction().
  virtual v8::Handle<v8::Object> NewInstance();

 protected:
  typedef v8::Handle<v8::Value> (*HandlerFunc)(const v8::Arguments&);
  typedef base::Callback<v8::Handle<v8::Value>(const v8::Arguments&)>
      HandlerFunction;

  // Installs a new 'route' from |name| to |handler_function|. This means that
  // NewInstance()s of this NativeHandler will have a property |name| which
  // will be handled by |handler_function|.
  void RouteFunction(const std::string& name,
                     const HandlerFunction& handler_function);

  void RouteStaticFunction(const std::string& name,
                           const HandlerFunc handler_func);

 private:
  static v8::Handle<v8::Value> Router(const v8::Arguments& args);
  static void DisposeFunction(v8::Persistent<v8::Value> object,
                              void* parameter);

  v8::Persistent<v8::ObjectTemplate> object_template_;

  DISALLOW_COPY_AND_ASSIGN(NativeHandler);
};

}  // extensions

#endif  // CHROME_RENDERER_EXTENSIONS_NATIVE_HANDLER_H_
