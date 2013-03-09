// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_OBJECT_BACKED_NATIVE_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_OBJECT_BACKED_NATIVE_HANDLER_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/linked_ptr.h"
#include "chrome/renderer/extensions/native_handler.h"
#include "chrome/renderer/extensions/scoped_persistent.h"
#include "v8/include/v8.h"

namespace extensions {

// An ObjectBackedNativeHandler is a factory for JS objects with functions on
// them that map to native C++ functions. Subclasses should call RouteFunction()
// in their constructor to define functions on the created JS objects.
class ObjectBackedNativeHandler : public NativeHandler {
 public:
  explicit ObjectBackedNativeHandler(v8::Handle<v8::Context> context);
  virtual ~ObjectBackedNativeHandler();

  // Create an object with bindings to the native functions defined through
  // RouteFunction().
  virtual v8::Handle<v8::Object> NewInstance() OVERRIDE;

 protected:
  typedef v8::Handle<v8::Value> (*HandlerFunc)(const v8::Arguments&);
  typedef base::Callback<v8::Handle<v8::Value>(const v8::Arguments&)>
      HandlerFunction;

  // Installs a new 'route' from |name| to |handler_function|. This means that
  // NewInstance()s of this ObjectBackedNativeHandler will have a property
  // |name| which will be handled by |handler_function|.
  void RouteFunction(const std::string& name,
                     const HandlerFunction& handler_function);

  void RouteStaticFunction(const std::string& name,
                           const HandlerFunc handler_func);

  v8::Handle<v8::Context> v8_context() { return v8_context_.get(); }

  virtual void Invalidate() OVERRIDE;

 private:

  static v8::Handle<v8::Value> Router(const v8::Arguments& args);

  struct RouterData;
  std::vector<linked_ptr<RouterData> > router_data_;

  // TODO(kalman): Just pass around a ChromeV8Context. It already has a
  // persistent handle to this context.
  ScopedPersistent<v8::Context> v8_context_;

  ScopedPersistent<v8::ObjectTemplate> object_template_;

  DISALLOW_COPY_AND_ASSIGN(ObjectBackedNativeHandler);
};

}  // extensions

#endif  // CHROME_RENDERER_EXTENSIONS_OBJECT_BACKED_NATIVE_HANDLER_H_
