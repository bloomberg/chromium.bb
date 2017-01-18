// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/api_binding_hooks.h"
#include "extensions/renderer/api_event_handler.h"
#include "extensions/renderer/api_request_handler.h"
#include "extensions/renderer/api_signature.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/arguments.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {

// Returns the name of the enum value for use in JavaScript; JS enum entries use
// SCREAMING_STYLE.
std::string GetJSEnumEntryName(const std::string& original) {
  std::string result;
  DCHECK(!original.empty());
  // If the original starts with a digit, prefix it with an underscore.
  if (base::IsAsciiDigit(original[0]))
    result.push_back('_');
  // Given 'myEnum-Foo':
  for (size_t i = 0; i < original.size(); ++i) {
    // Add an underscore between camelcased items:
    // 'myEnum-Foo' -> 'mY_Enum-Foo'
    if (i > 0 && base::IsAsciiLower(original[i - 1]) &&
        base::IsAsciiUpper(original[i])) {
      result.push_back('_');
      result.push_back(original[i]);
    } else if (original[i] == '-') {  // 'mY_Enum-Foo' -> 'mY_Enum_Foo'
      result.push_back('_');
    } else {  // 'mY_Enum_Foo' -> 'MY_ENUM_FOO'
      result.push_back(base::ToUpperASCII(original[i]));
    }
  }
  return result;
}

void CallbackHelper(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  // If the current context (the in which this function was created) has been
  // disposed, the per-context data has been deleted. Since it was the owner of
  // the callback, we can no longer access that object.
  // Various parts of the binding system rely on per-context data. If that has
  // been deleted (which happens during context shutdown), bail out.
  v8::Local<v8::Context> context = args.isolate()->GetCurrentContext();
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return;

  v8::Local<v8::External> external;
  CHECK(args.GetData(&external));
  auto* callback = static_cast<APIBinding::HandlerCallback*>(external->Value());

  callback->Run(&args);
}

}  // namespace

APIBinding::Request::Request() {}
APIBinding::Request::~Request() {}

struct APIBinding::MethodData {
  MethodData(std::string full_name,
             const base::ListValue& method_signature)
      : full_name(std::move(full_name)),
        signature(method_signature) {}

  // The fully-qualified name of this api (e.g. runtime.sendMessage instead of
  // sendMessage).
  std::string full_name;
  // The expected API signature.
  APISignature signature;
  // The template for the v8::Function for this method.
  v8::Eternal<v8::FunctionTemplate> function_template;
  // The callback used by the v8 function.
  APIBinding::HandlerCallback callback;
};

APIBinding::APIBinding(const std::string& api_name,
                       const base::ListValue* function_definitions,
                       const base::ListValue* type_definitions,
                       const base::ListValue* event_definitions,
                       const SendRequestMethod& callback,
                       std::unique_ptr<APIBindingHooks> binding_hooks,
                       ArgumentSpec::RefMap* type_refs,
                       APIRequestHandler* request_handler)
    : api_name_(api_name),
      method_callback_(callback),
      binding_hooks_(std::move(binding_hooks)),
      type_refs_(type_refs),
      request_handler_(request_handler),
      weak_factory_(this) {
  DCHECK(!method_callback_.is_null());
  if (function_definitions) {
    for (const auto& func : *function_definitions) {
      const base::DictionaryValue* func_dict = nullptr;
      CHECK(func->GetAsDictionary(&func_dict));
      std::string name;
      CHECK(func_dict->GetString("name", &name));

      const base::ListValue* params = nullptr;
      CHECK(func_dict->GetList("parameters", &params));
      methods_[name] = base::MakeUnique<MethodData>(
          base::StringPrintf("%s.%s", api_name_.c_str(), name.c_str()),
          *params);
    }
  }

  if (type_definitions) {
    for (const auto& type : *type_definitions) {
      const base::DictionaryValue* type_dict = nullptr;
      CHECK(type->GetAsDictionary(&type_dict));
      std::string id;
      CHECK(type_dict->GetString("id", &id));
      DCHECK(type_refs->find(id) == type_refs->end());
      // TODO(devlin): refs are sometimes preceeded by the API namespace; we
      // might need to take that into account.
      auto argument_spec = base::MakeUnique<ArgumentSpec>(*type_dict);
      const std::set<std::string>& enum_values = argument_spec->enum_values();
      if (!enum_values.empty()) {
        // Type names may be prefixed by the api name. If so, remove the prefix.
        base::Optional<std::string> stripped_id;
        if (base::StartsWith(id, api_name_, base::CompareCase::SENSITIVE))
          stripped_id = id.substr(api_name_.size() + 1);  // +1 for trailing '.'
        std::vector<EnumEntry>& entries =
            enums_[stripped_id ? *stripped_id : id];
        entries.reserve(enum_values.size());
        for (const auto& enum_value : enum_values) {
          entries.push_back(
              std::make_pair(enum_value, GetJSEnumEntryName(enum_value)));
        }
      }
      (*type_refs)[id] = std::move(argument_spec);
    }
  }

  if (event_definitions) {
    event_names_.reserve(event_definitions->GetSize());
    for (const auto& event : *event_definitions) {
      const base::DictionaryValue* event_dict = nullptr;
      CHECK(event->GetAsDictionary(&event_dict));
      std::string name;
      CHECK(event_dict->GetString("name", &name));
      event_names_.push_back(std::move(name));
    }
  }
}

APIBinding::~APIBinding() {}

v8::Local<v8::Object> APIBinding::CreateInstance(
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    APIEventHandler* event_handler,
    const AvailabilityCallback& is_available) {
  // TODO(devlin): APIs may change depending on which features are available,
  // but we should be able to cache the unconditional methods on an object
  // template, create the object, and then add any conditional methods. Ideally,
  // this information should be available on the generated API specification.
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);

  for (const auto& key_value : methods_) {
    MethodData& method = *key_value.second;
    if (!is_available.Run(method.full_name))
      continue;

    v8::Eternal<v8::FunctionTemplate>& function_template =
        method.function_template;
    if (function_template.IsEmpty()) {
      DCHECK(method.callback.is_null());
      method.callback =
          base::Bind(&APIBinding::HandleCall, weak_factory_.GetWeakPtr(),
                     method.full_name, &method.signature);
      function_template.Set(
          isolate,
          v8::FunctionTemplate::New(
              isolate, &CallbackHelper,
              v8::External::New(isolate, &method.callback),
              v8::Local<v8::Signature>(), 0, v8::ConstructorBehavior::kThrow));
    }

    v8::Local<v8::FunctionTemplate> local_template =
        function_template.Get(isolate);
    v8::MaybeLocal<v8::Function> maybe_function =
        local_template->GetFunction(context);
    v8::Maybe<bool> success = object->CreateDataProperty(
        context, gin::StringToSymbol(isolate, key_value.first),
        maybe_function.ToLocalChecked());
    DCHECK(success.IsJust());
    DCHECK(success.FromJust());
  }

  for (const std::string& event_name : event_names_) {
    std::string full_event_name =
        base::StringPrintf("%s.%s", api_name_.c_str(), event_name.c_str());
    v8::Local<v8::Object> event =
        event_handler->CreateEventInstance(full_event_name, context);
    DCHECK(!event.IsEmpty());
    v8::Maybe<bool> success = object->CreateDataProperty(
        context, gin::StringToSymbol(isolate, event_name), event);
    DCHECK(success.IsJust());
    DCHECK(success.FromJust());
  }

  for (const auto& entry : enums_) {
    // TODO(devlin): Store these on an ObjectTemplate.
    v8::Local<v8::Object> enum_object = v8::Object::New(isolate);
    for (const auto& enum_entry : entry.second) {
      v8::Maybe<bool> success = enum_object->CreateDataProperty(
          context, gin::StringToSymbol(isolate, enum_entry.second),
          gin::StringToSymbol(isolate, enum_entry.first));
      DCHECK(success.IsJust());
      DCHECK(success.FromJust());
    }
    v8::Maybe<bool> success = object->CreateDataProperty(
        context, gin::StringToSymbol(isolate, entry.first), enum_object);
    DCHECK(success.IsJust());
    DCHECK(success.FromJust());
  }

  binding_hooks_->InitializeInContext(context, api_name_);

  return object;
}

v8::Local<v8::Object> APIBinding::GetJSHookInterface(
    v8::Local<v8::Context> context) {
  return binding_hooks_->GetJSHookInterface(api_name_, context);
}

void APIBinding::HandleCall(const std::string& name,
                            const APISignature* signature,
                            gin::Arguments* arguments) {
  std::string error;
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);

  // Since this is called synchronously from the JS entry point,
  // GetCurrentContext() should always be correct.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  std::vector<v8::Local<v8::Value>> argument_list;
  if (arguments->Length() > 0) {
    // Just copying handles should never fail.
    CHECK(arguments->GetRemaining(&argument_list));
  }

  bool invalid_invocation = false;
  v8::Local<v8::Function> custom_callback;
  {
    v8::TryCatch try_catch(isolate);
    APIBindingHooks::RequestResult hooks_result =
        binding_hooks_->RunHooks(api_name_, name, context,
                                 signature, &argument_list, *type_refs_);

    switch (hooks_result.code) {
      case APIBindingHooks::RequestResult::INVALID_INVOCATION:
        invalid_invocation = true;
        // Throw a type error below so that it's not caught by our try-catch.
        break;
      case APIBindingHooks::RequestResult::THROWN:
        DCHECK(try_catch.HasCaught());
        try_catch.ReThrow();
        return;
      case APIBindingHooks::RequestResult::HANDLED:
        if (!hooks_result.return_value.IsEmpty())
          arguments->Return(hooks_result.return_value);
        return;  // Our work here is done.
      case APIBindingHooks::RequestResult::NOT_HANDLED:
        break;  // Handle in the default manner.
    }
    custom_callback = hooks_result.custom_callback;
  }

  if (invalid_invocation) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }

  std::unique_ptr<base::ListValue> converted_arguments;
  v8::Local<v8::Function> callback;
  {
    v8::TryCatch try_catch(isolate);
    invalid_invocation = !signature->ParseArgumentsToJSON(
        context, argument_list, *type_refs_,
        &converted_arguments, &callback, &error);
    if (try_catch.HasCaught()) {
      DCHECK(!converted_arguments);
      try_catch.ReThrow();
      return;
    }
  }
  if (invalid_invocation) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }

  auto request = base::MakeUnique<Request>();
  if (!callback.IsEmpty()) {
    // In the JS bindings, custom callbacks are called with the arguments of
    // name, the full request object (see below), the original callback, and
    // the responses from the API. The responses from the API are handled by the
    // APIRequestHandler, but we need to curry in the other values.
    std::vector<v8::Local<v8::Value>> callback_args;
    if (!custom_callback.IsEmpty()) {
      // TODO(devlin): The |request| object in the JS bindings includes
      // properties for callback, callbackSchema, args, stack, id, and
      // customCallback. Of those, it appears that we only use stack, args, and
      // id (since callback is curried in separately). We may be able to update
      // bindings to get away from some of those. For now, just pass in an empty
      // object (most APIs don't rely on it).
      v8::Local<v8::Object> request = v8::Object::New(isolate);
      callback_args = { gin::StringToSymbol(isolate, name), request, callback };
      callback = custom_callback;
    }
    request->request_id = request_handler_->AddPendingRequest(
        isolate, callback, context, callback_args);
    request->has_callback = true;
  }
  // TODO(devlin): Query and curry user gestures around.
  request->has_user_gesture = false;
  request->arguments = std::move(converted_arguments);
  request->method_name = name;

  method_callback_.Run(std::move(request), context);
}

}  // namespace extensions
