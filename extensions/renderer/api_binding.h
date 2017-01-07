// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDING_H_
#define EXTENSIONS_RENDERER_API_BINDING_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "extensions/renderer/argument_spec.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

namespace gin {
class Arguments;
}

namespace extensions {
class APIBindingHooks;
class APIEventHandler;
class APISignature;

// A class that vends v8::Objects for extension APIs. These APIs have function
// interceptors for all exposed methods, which call back into the APIBinding.
// The APIBinding then matches the calling arguments against an expected method
// signature, throwing an error if they don't match.
// There should only need to be a single APIBinding object for each API, and
// each can vend multiple v8::Objects for different contexts.
// This object is designed to be one-per-isolate, but used across separate
// contexts.
class APIBinding {
 public:
  // The callback to called when an API method is invoked with matching
  // arguments. This passes the name of the api method and the arguments it
  // was passed, as well as the current isolate, context, and callback value.
  // Note that the callback can be empty if none was passed.
  using APIMethodCallback =
      base::Callback<void(const std::string& name,
                          std::unique_ptr<base::ListValue> arguments,
                          v8::Isolate*,
                          v8::Local<v8::Context>,
                          v8::Local<v8::Function>)>;

  // The callback for determining if a given API method (specified by |name|)
  // is available.
  using AvailabilityCallback = base::Callback<bool(const std::string& name)>;

  // The callback type for handling an API call.
  using HandlerCallback = base::Callback<void(gin::Arguments*)>;

  // The ArgumentSpec::RefMap is required to outlive this object.
  // |function_definitions|, |type_definitions| and |event_definitions|
  // may be null if the API does not specify any of that category.
  APIBinding(const std::string& name,
             const base::ListValue* function_definitions,
             const base::ListValue* type_definitions,
             const base::ListValue* event_definitions,
             const APIMethodCallback& callback,
             std::unique_ptr<APIBindingHooks> binding_hooks,
             ArgumentSpec::RefMap* type_refs);
  ~APIBinding();

  // Returns a new v8::Object for the API this APIBinding represents.
  v8::Local<v8::Object> CreateInstance(
      v8::Local<v8::Context> context,
      v8::Isolate* isolate,
      APIEventHandler* event_handler,
      const AvailabilityCallback& is_available);

  // Returns the JS interface to use when registering hooks with legacy custom
  // bindings.
  v8::Local<v8::Object> GetJSHookInterface(v8::Local<v8::Context> context);

 private:
  // Handles a call an API method with the given |name| and matches the
  // arguments against |signature|.
  void HandleCall(const std::string& name,
                  const APISignature* signature,
                  gin::Arguments* args);

  // The root name of the API, e.g. "tabs" for chrome.tabs.
  std::string api_name_;

  // A map from method name to method data.
  struct MethodData;
  std::map<std::string, std::unique_ptr<MethodData>> methods_;

  // The names of all events associated with this API.
  std::vector<std::string> event_names_;

  // The pair for enum entry is <original, js-ified>. JS enum entries use
  // SCREAMING_STYLE (whereas our API enums are just inconsistent).
  using EnumEntry = std::pair<std::string, std::string>;
  // A map of <name, values> for the enums on this API.
  std::map<std::string, std::vector<EnumEntry>> enums_;

  // The callback to use when an API is invoked with valid arguments.
  APIMethodCallback method_callback_;

  // The registered hooks for this API.
  std::unique_ptr<APIBindingHooks> binding_hooks_;

  // The reference map for all known types; required to outlive this object.
  const ArgumentSpec::RefMap* type_refs_;

  base::WeakPtrFactory<APIBinding> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(APIBinding);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_H_
