// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDING_BRIDGE_H_
#define EXTENSIONS_RENDERER_API_BINDING_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "extensions/renderer/api_binding_types.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
class APIBindingHooks;
class APIRequestHandler;
class APITypeReferenceMap;

// An object that serves as a bridge between the current JS-centric bindings and
// the new native bindings system. This basically needs to conform to the public
// methods of the Binding prototype in binding.js.
class APIBindingBridge final : public gin::Wrappable<APIBindingBridge> {
 public:
  APIBindingBridge(const APITypeReferenceMap* type_refs,
                   APIRequestHandler* request_handler,
                   APIBindingHooks* hooks,
                   v8::Local<v8::Context> context,
                   v8::Local<v8::Value> api_object,
                   const std::string& extension_id,
                   const std::string& context_type,
                   const binding::RunJSFunction& run_js);
  ~APIBindingBridge() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

 private:
  // Runs the given function and registers custom hooks.
  // The function takes three arguments: an object,
  // {
  //   apiFunctions: <JSHookInterface> (see api_bindings_hooks.cc),
  //   compiledApi: <the API object>
  // }
  // as well as a string for the extension ID and a string for the context type.
  // This should register any hooks that the JS needs for the given API.
  void RegisterCustomHook(v8::Isolate* isolate,
                          v8::Local<v8::Function> function);

  // A handler to initiate an API request through the APIRequestHandler. A
  // replacement for custom bindings that utilize require('sendRequest').
  void SendRequest(gin::Arguments* arguments,
                   const std::string& name,
                   const std::vector<v8::Local<v8::Value>>& request_args);

  // Type references. Guaranteed to outlive this object.
  const APITypeReferenceMap* type_refs_;

  // The request handler. Guaranteed to outlive this object.
  APIRequestHandler* request_handler_;

  // The hooks associated with this API. Guaranteed to outlive this object.
  APIBindingHooks* hooks_;

  // The id of the extension that owns the context this belongs to.
  std::string extension_id_;

  // The type of context this belongs to.
  std::string context_type_;

  // A function to run JS safely.
  binding::RunJSFunction run_js_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingBridge);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_BRIDGE_H_
