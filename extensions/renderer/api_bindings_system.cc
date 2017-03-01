// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_bindings_system.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_hooks.h"

namespace extensions {

APIBindingsSystem::APIBindingsSystem(
    const binding::RunJSFunction& call_js,
    const binding::RunJSFunctionSync& call_js_sync,
    const GetAPISchemaMethod& get_api_schema,
    const APIRequestHandler::SendRequestMethod& send_request,
    const APIEventHandler::EventListenersChangedMethod& event_listeners_changed,
    APILastError last_error)
    : type_reference_map_(base::Bind(&APIBindingsSystem::InitializeType,
                                     base::Unretained(this))),
      request_handler_(send_request, call_js, std::move(last_error)),
      event_handler_(call_js, event_listeners_changed),
      call_js_(call_js),
      call_js_sync_(call_js_sync),
      get_api_schema_(get_api_schema) {}

APIBindingsSystem::~APIBindingsSystem() {}

v8::Local<v8::Object> APIBindingsSystem::CreateAPIInstance(
    const std::string& api_name,
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    const APIBinding::AvailabilityCallback& is_available,
    APIBindingHooks** hooks_out) {
  std::unique_ptr<APIBinding>& binding = api_bindings_[api_name];
  if (!binding)
    binding = CreateNewAPIBinding(api_name);
  if (hooks_out)
    *hooks_out = binding->hooks();
  return binding->CreateInstance(context, isolate, is_available);
}

std::unique_ptr<APIBinding> APIBindingsSystem::CreateNewAPIBinding(
    const std::string& api_name) {
  const base::DictionaryValue& api_schema = get_api_schema_.Run(api_name);

  const base::ListValue* function_definitions = nullptr;
  api_schema.GetList("functions", &function_definitions);
  const base::ListValue* type_definitions = nullptr;
  api_schema.GetList("types", &type_definitions);
  const base::ListValue* event_definitions = nullptr;
  api_schema.GetList("events", &event_definitions);
  const base::DictionaryValue* property_definitions = nullptr;
  api_schema.GetDictionary("properties", &property_definitions);

  // Find the hooks for the API. If none exist, an empty set will be created so
  // we can use JS custom bindings.
  // TODO(devlin): Once all legacy custom bindings are converted, we don't have
  // to unconditionally pass in binding hooks.
  std::unique_ptr<APIBindingHooks> hooks;
  auto iter = binding_hooks_.find(api_name);
  if (iter != binding_hooks_.end()) {
    hooks = std::move(iter->second);
    binding_hooks_.erase(iter);
  } else {
    hooks = base::MakeUnique<APIBindingHooks>(api_name, call_js_sync_);
  }

  return base::MakeUnique<APIBinding>(
      api_name, function_definitions, type_definitions, event_definitions,
      property_definitions,
      base::Bind(&APIBindingsSystem::CreateCustomType, base::Unretained(this)),
      std::move(hooks), &type_reference_map_, &request_handler_,
      &event_handler_);
}

void APIBindingsSystem::InitializeType(const std::string& type_name) {
  // In order to initialize the type, we just initialize the full binding. This
  // seems like a lot of work, but in practice, trying to extract out only the
  // types from the schema, and then update the reference map based on that, is
  // close enough to the same cost. Additionally, this happens lazily on API
  // use, and relatively few APIs specify types from another API. Finally, this
  // will also go away if/when we generate all these specifications.
  std::string::size_type dot = type_name.rfind('.');
  // The type name should be fully qualified (include the API name).
  DCHECK_NE(std::string::npos, dot);
  DCHECK_LT(dot, type_name.size() - 1);
  std::string api_name = type_name.substr(0, dot);
  // If we've already instantiated the binding, the type should have been in
  // there.
  DCHECK(api_bindings_.find(api_name) == api_bindings_.end());

  api_bindings_[api_name] = CreateNewAPIBinding(api_name);
}

void APIBindingsSystem::CompleteRequest(int request_id,
                                        const base::ListValue& response,
                                        const std::string& error) {
  request_handler_.CompleteRequest(request_id, response, error);
}

void APIBindingsSystem::FireEventInContext(const std::string& event_name,
                                           v8::Local<v8::Context> context,
                                           const base::ListValue& response) {
  event_handler_.FireEventInContext(event_name, context, response);
}

APIBindingHooks* APIBindingsSystem::GetHooksForAPI(
    const std::string& api_name) {
  DCHECK(api_bindings_.empty())
      << "Hook registration must happen before creating any binding instances.";
  std::unique_ptr<APIBindingHooks>& hooks = binding_hooks_[api_name];
  if (!hooks)
    hooks = base::MakeUnique<APIBindingHooks>(api_name, call_js_sync_);
  return hooks.get();
}

void APIBindingsSystem::RegisterCustomType(const std::string& type_name,
                                           const CustomTypeHandler& function) {
  DCHECK(custom_types_.find(type_name) == custom_types_.end())
      << "Custom type already registered: " << type_name;
  custom_types_[type_name] = function;
}

v8::Local<v8::Object> APIBindingsSystem::CreateCustomType(
    v8::Local<v8::Context> context,
    const std::string& type_name,
    const std::string& property_name) {
  auto iter = custom_types_.find(type_name);
  DCHECK(iter != custom_types_.end()) << "Custom type not found: " << type_name;
  return iter->second.Run(context, property_name, &request_handler_,
                          &event_handler_, &type_reference_map_);
}

}  // namespace extensions
