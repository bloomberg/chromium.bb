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
#include "extensions/renderer/argument_spec.h"
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
  // The result of checking for hooks to handle a request.
  struct RequestResult {
    enum ResultCode {
      HANDLED,             // A custom hook handled the request.
      THROWN,              // An exception was thrown during parsing or
                           // handling.
      INVALID_INVOCATION,  // The request was called with invalid arguments.
      NOT_HANDLED,         // The request was not handled.
    };

    explicit RequestResult(ResultCode code);
    RequestResult(ResultCode code, v8::Local<v8::Function> custom_callback);
    RequestResult(const RequestResult& other);
    ~RequestResult();

    ResultCode code;
    v8::Local<v8::Function> custom_callback;
    v8::Local<v8::Value> return_value;  // Only valid if code == HANDLED.
  };

  // The callback to handle an API method. We pass in the expected signature
  // (so the caller can verify arguments, optionally after modifying/"massaging"
  // them) and the passed arguments. The handler is responsible for returning,
  // which depending on the API could mean either returning synchronously
  // through gin::Arguments::Return or asynchronously through a passed callback.
  // TODO(devlin): As we continue expanding the hooks interface, we should allow
  // handlers to register a request so that they don't have to maintain a
  // reference to the callback themselves.
  using HandleRequestHook =
      base::Callback<RequestResult(const APISignature*,
                                   v8::Local<v8::Context> context,
                                   std::vector<v8::Local<v8::Value>>*,
                                   const APITypeReferenceMap&)>;

  APIBindingHooks(const std::string& api_name,
                  const binding::RunJSFunctionSync& run_js);
  ~APIBindingHooks();

  // Register a custom binding to handle requests.
  void RegisterHandleRequest(const std::string& method_name,
                             const HandleRequestHook& hook);

  // Registers a JS script to be compiled and run in order to initialize any JS
  // hooks within a v8 context.
  void RegisterJsSource(v8::Global<v8::String> source,
                        v8::Global<v8::String> resource_name);

  // Initializes JS hooks within a context.
  void InitializeInContext(v8::Local<v8::Context> context);

  // Looks for any custom hooks associated with the given request, and, if any
  // are found, runs them. Returns the result of running the hooks, if any.
  RequestResult RunHooks(const std::string& method_name,
                         v8::Local<v8::Context> context,
                         const APISignature* signature,
                         std::vector<v8::Local<v8::Value>>* arguments,
                         const APITypeReferenceMap& type_refs);

  // Returns a JS interface that can be used to register hooks.
  v8::Local<v8::Object> GetJSHookInterface(v8::Local<v8::Context> context);

  // Gets the custom-set JS callback for the given method, if one exists.
  v8::Local<v8::Function> GetCustomJSCallback(const std::string& method_name,
                                              v8::Local<v8::Context> context);

 private:
  // Updates the |arguments| by running |function| and settings arguments to the
  // returned result.
  bool UpdateArguments(v8::Local<v8::Function> function,
                       v8::Local<v8::Context> context,
                       std::vector<v8::Local<v8::Value>>* arguments);

  // Whether we've tried to use any hooks associated with this object.
  bool hooks_used_ = false;

  // All registered request handlers.
  std::map<std::string, HandleRequestHook> request_hooks_;

  // The script to run to initialize JS hooks, if any.
  v8::Global<v8::String> js_hooks_source_;

  // The name of the JS resource for the hooks. Used to create a ScriptOrigin
  // to make exception stack traces more readable.
  v8::Global<v8::String> js_resource_name_;

  // The name of the associated API.
  std::string api_name_;

  // We use synchronous JS execution here because at every point we execute JS,
  // it's in direct response to JS calling in. There should be no reason that
  // script is disabled.
  binding::RunJSFunctionSync run_js_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingHooks);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_HOOKS_H_
