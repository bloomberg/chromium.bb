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

const char kExtensionAPIPerContextKey[] = "extension_api_binding";

struct APIPerContextData : public base::SupportsUserData::Data {
  std::vector<std::unique_ptr<APIBinding::HandlerCallback>> context_callbacks;
};

void CallbackHelper(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);

  // If the current context (the in which this function was created) has been
  // disposed, the per-context data has been deleted. Since it was the owner of
  // the callback, we can no longer access that object.
  v8::Local<v8::Context> context = args.isolate()->GetCurrentContext();
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return;

  v8::Local<v8::External> external;
  CHECK(args.GetData(&external));
  auto* callback = static_cast<APIBinding::HandlerCallback*>(external->Value());

#if DCHECK_IS_ON()
  // If there is per-context data, then it should own the callback that is about
  // to be called. Double-check this in debug builds.
  APIPerContextData* data = static_cast<APIPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIPerContextKey));
  DCHECK(data);
  DCHECK(std::any_of(
      data->context_callbacks.begin(), data->context_callbacks.end(),
      [callback](
          const std::unique_ptr<APIBinding::HandlerCallback>& live_callback) {
        return live_callback.get() == callback;
      }));
#endif  // DCHECK_IS_ON()

  callback->Run(&args);
}

}  // namespace

APIBinding::APIBinding(const std::string& api_name,
                       const base::ListValue* function_definitions,
                       const base::ListValue* type_definitions,
                       const base::ListValue* event_definitions,
                       const APIMethodCallback& callback,
                       std::unique_ptr<APIBindingHooks> binding_hooks,
                       ArgumentSpec::RefMap* type_refs)
    : api_name_(api_name),
      method_callback_(callback),
      binding_hooks_(std::move(binding_hooks)),
      type_refs_(type_refs),
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
      signatures_[name] = base::MakeUnique<APISignature>(*params);
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
  APIPerContextData* data = static_cast<APIPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIPerContextKey));
  if (!data) {
    auto api_data = base::MakeUnique<APIPerContextData>();
    data = api_data.get();
    per_context_data->SetUserData(kExtensionAPIPerContextKey,
                                  api_data.release());
  }

  for (const auto& sig : signatures_) {
    std::string full_method_name =
        base::StringPrintf("%s.%s", api_name_.c_str(), sig.first.c_str());

    if (!is_available.Run(full_method_name))
      continue;

    auto handler_callback = base::MakeUnique<HandlerCallback>(
        base::Bind(&APIBinding::HandleCall, weak_factory_.GetWeakPtr(),
                   full_method_name, sig.second.get()));
    // TODO(devlin): We should be able to cache these in a function template.
    v8::MaybeLocal<v8::Function> maybe_function =
        v8::Function::New(context, &CallbackHelper,
                          v8::External::New(isolate, handler_callback.get()),
                          0, v8::ConstructorBehavior::kThrow);
    data->context_callbacks.push_back(std::move(handler_callback));
    v8::Maybe<bool> success = object->CreateDataProperty(
        context, gin::StringToSymbol(isolate, sig.first),
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
  {
    v8::TryCatch try_catch(isolate);
    APIBindingHooks::RequestResult hooks_result =
        binding_hooks_->HandleRequest(api_name_, name, context,
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

  DCHECK(converted_arguments);
  method_callback_.Run(name, std::move(converted_arguments), isolate, context,
                       callback);
}

}  // namespace extensions
