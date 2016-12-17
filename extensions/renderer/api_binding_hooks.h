// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDING_HOOKS_H_
#define EXTENSIONS_RENDERER_API_BINDING_HOOKS_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "extensions/renderer/api_binding_types.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
class APISignature;

// A class to register custom hooks for given API calls that need different
// handling. An instance exists for a single API, but can be used across
// multiple contexts (but only on the same thread).
// TODO(devlin): We have both C++ and JS custom bindings, but this only allows
// for registration of C++ handlers. Add JS support.
class APIBindingHooks {
 public:
  // The callback to handle an API method. We pass in the expected signature
  // (so the caller can verify arguments, optionally after modifying/"massaging"
  // them) and the passed arguments. The handler is responsible for returning,
  // which depending on the API could mean either returning synchronously
  // through gin::Arguments::Return or asynchronously through a passed callback.
  // TODO(devlin): As we continue expanding the hooks interface, we should allow
  // handlers to register a request so that they don't have to maintain a
  // reference to the callback themselves.
  using HandleRequestHook =
      base::Callback<void(const APISignature*, gin::Arguments*)>;

  explicit APIBindingHooks(const binding::RunJSFunction& run_js);
  ~APIBindingHooks();

  // Register a custom binding to handle requests.
  void RegisterHandleRequest(const std::string& method_name,
                             const HandleRequestHook& hook);

  // Registers a JS script to be compiled and run in order to initialize any JS
  // hooks within a v8 context.
  void RegisterJsSource(v8::Global<v8::String> source,
                        v8::Global<v8::String> resource_name);

  // Initializes JS hooks within a context.
  void InitializeInContext(v8::Local<v8::Context> context,
                           const std::string& api_name);

  // Looks for a custom hook to handle the given request and, if one exists,
  // runs it. Returns true if a hook was found and run.
  bool HandleRequest(const std::string& api_name,
                     const std::string& method_name,
                     v8::Local<v8::Context> context,
                     const APISignature* signature,
                     gin::Arguments* arguments);

 private:
  // Whether we've tried to use any hooks associated with this object.
  bool hooks_used_ = false;

  // All registered request handlers.
  std::map<std::string, HandleRequestHook> request_hooks_;

  // The script to run to initialize JS hooks, if any.
  v8::Global<v8::String> js_hooks_source_;

  // The name of the JS resource for the hooks. Used to create a ScriptOrigin
  // to make exception stack traces more readable.
  v8::Global<v8::String> js_resource_name_;

  binding::RunJSFunction run_js_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingHooks);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_HOOKS_H_
