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
#include "extensions/renderer/api_type_reference_map.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/arguments.h"
#include "gin/per_context_data.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

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

bool IsContextValid(v8::Local<v8::Context> context) {
  // If the given context has been disposed, the per-context data has been
  // deleted, and the context is no longer valid. The APIBinding (which owns
  // various necessary pieces) should outlive all contexts, so if the context
  // is valid, associated callbacks should be safe.
  return gin::PerContextData::From(context) != nullptr;
}

void CallbackHelper(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);
  if (!IsContextValid(args.isolate()->GetCurrentContext()))
    return;

  v8::Local<v8::External> external;
  CHECK(args.GetData(&external));
  auto* callback = static_cast<APIBinding::HandlerCallback*>(external->Value());

  callback->Run(&args);
}

}  // namespace

struct APIBinding::MethodData {
  MethodData(std::string full_name, const APISignature* signature)
      : full_name(std::move(full_name)), signature(signature) {}

  // The fully-qualified name of this api (e.g. runtime.sendMessage instead of
  // sendMessage).
  std::string full_name;
  // The expected API signature.
  const APISignature* signature;
  // The callback used by the v8 function.
  APIBinding::HandlerCallback callback;
};

struct APIBinding::EventData {
  EventData(std::string exposed_name,
            std::string full_name,
            APIEventHandler* event_handler)
      : exposed_name(std::move(exposed_name)),
        full_name(std::move(full_name)),
        event_handler(event_handler) {}

  // The name of the event on the API object (e.g. onCreated).
  std::string exposed_name;
  // The fully-specified name of the event (e.g. tabs.onCreated).
  std::string full_name;
  // The associated event handler. This raw pointer is safe because the
  // EventData is only accessed from the callbacks associated with the
  // APIBinding, and both the APIBinding and APIEventHandler are owned by the
  // same object (the APIBindingsSystem).
  APIEventHandler* event_handler;
};

struct APIBinding::CustomPropertyData {
  CustomPropertyData(const std::string& type_name,
                     const std::string& property_name,
                     const CreateCustomType& create_custom_type)
      : type_name(type_name),
        property_name(property_name),
        create_custom_type(create_custom_type) {}

  // The type of the property, e.g. 'storage.StorageArea'.
  std::string type_name;
  // The name of the property on the object, e.g. 'local' for
  // chrome.storage.local.
  std::string property_name;

  CreateCustomType create_custom_type;
};

APIBinding::APIBinding(const std::string& api_name,
                       const base::ListValue* function_definitions,
                       const base::ListValue* type_definitions,
                       const base::ListValue* event_definitions,
                       const base::DictionaryValue* property_definitions,
                       const CreateCustomType& create_custom_type,
                       std::unique_ptr<APIBindingHooks> binding_hooks,
                       APITypeReferenceMap* type_refs,
                       APIRequestHandler* request_handler,
                       APIEventHandler* event_handler)
    : api_name_(api_name),
      property_definitions_(property_definitions),
      create_custom_type_(create_custom_type),
      binding_hooks_(std::move(binding_hooks)),
      type_refs_(type_refs),
      request_handler_(request_handler),
      weak_factory_(this) {
  // TODO(devlin): It might make sense to instantiate the object_template_
  // directly here, which would avoid the need to hold on to
  // |property_definitions_| and |enums_|. However, there are *some* cases where
  // we don't immediately stamp out an API from the template following
  // construction.

  if (function_definitions) {
    for (const auto& func : *function_definitions) {
      const base::DictionaryValue* func_dict = nullptr;
      CHECK(func->GetAsDictionary(&func_dict));
      std::string name;
      CHECK(func_dict->GetString("name", &name));

      const base::ListValue* params = nullptr;
      CHECK(func_dict->GetList("parameters", &params));
      auto signature = base::MakeUnique<APISignature>(*params);
      std::string full_name =
          base::StringPrintf("%s.%s", api_name_.c_str(), name.c_str());
      methods_[name] = base::MakeUnique<MethodData>(full_name, signature.get());
      type_refs->AddAPIMethodSignature(full_name, std::move(signature));
    }
  }

  if (type_definitions) {
    for (const auto& type : *type_definitions) {
      const base::DictionaryValue* type_dict = nullptr;
      CHECK(type->GetAsDictionary(&type_dict));
      std::string id;
      CHECK(type_dict->GetString("id", &id));
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
      type_refs->AddSpec(id, std::move(argument_spec));
      // Some types, like storage.StorageArea, have functions associated with
      // them. Cache the function signatures in the type map.
      const base::ListValue* type_functions = nullptr;
      if (type_dict->GetList("functions", &type_functions)) {
        for (const auto& func : *type_functions) {
          const base::DictionaryValue* func_dict = nullptr;
          CHECK(func->GetAsDictionary(&func_dict));
          std::string function_name;
          CHECK(func_dict->GetString("name", &function_name));

          const base::ListValue* params = nullptr;
          CHECK(func_dict->GetList("parameters", &params));
          type_refs->AddTypeMethodSignature(
              base::StringPrintf("%s.%s", id.c_str(), function_name.c_str()),
              base::MakeUnique<APISignature>(*params));
        }
      }
    }
  }

  if (event_definitions) {
    events_.reserve(event_definitions->GetSize());
    for (const auto& event : *event_definitions) {
      const base::DictionaryValue* event_dict = nullptr;
      CHECK(event->GetAsDictionary(&event_dict));
      std::string name;
      CHECK(event_dict->GetString("name", &name));
      std::string full_name =
          base::StringPrintf("%s.%s", api_name_.c_str(), name.c_str());
      events_.push_back(base::MakeUnique<EventData>(
          std::move(name), std::move(full_name), event_handler));
    }
  }
}

APIBinding::~APIBinding() {}

v8::Local<v8::Object> APIBinding::CreateInstance(
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    const AvailabilityCallback& is_available) {
  DCHECK(IsContextValid(context));
  if (object_template_.IsEmpty())
    InitializeTemplate(isolate);
  DCHECK(!object_template_.IsEmpty());

  v8::Local<v8::Object> object =
      object_template_.Get(isolate)->NewInstance(context).ToLocalChecked();

  // The object created from the template includes all methods, but some may
  // be unavailable in this context. Iterate over them and delete any that
  // aren't available.
  // TODO(devlin): Ideally, we'd only do this check on the methods that are
  // conditionally exposed. Or, we could have multiple templates for different
  // configurations, assuming there are a small number of possibilities.
  // TODO(devlin): enums should always be exposed, but there may be events that
  // are restricted. Investigate.
  for (const auto& key_value : methods_) {
    if (!is_available.Run(key_value.second->full_name)) {
      v8::Maybe<bool> success = object->Delete(
          context, gin::StringToSymbol(isolate, key_value.first));
      CHECK(success.IsJust());
      CHECK(success.FromJust());
    }
  }

  binding_hooks_->InitializeInContext(context);

  return object;
}

void APIBinding::InitializeTemplate(v8::Isolate* isolate) {
  DCHECK(object_template_.IsEmpty());
  v8::Local<v8::ObjectTemplate> object_template =
      v8::ObjectTemplate::New(isolate);

  for (const auto& key_value : methods_) {
    MethodData& method = *key_value.second;
    DCHECK(method.callback.is_null());
    method.callback =
        base::Bind(&APIBinding::HandleCall, weak_factory_.GetWeakPtr(),
                   method.full_name, method.signature);

    object_template->Set(
        gin::StringToSymbol(isolate, key_value.first),
        v8::FunctionTemplate::New(isolate, &CallbackHelper,
                                  v8::External::New(isolate, &method.callback),
                                  v8::Local<v8::Signature>(), 0,
                                  v8::ConstructorBehavior::kThrow));
  }

  for (const auto& event : events_) {
    object_template->SetLazyDataProperty(
        gin::StringToSymbol(isolate, event->exposed_name),
        &APIBinding::GetEventObject, v8::External::New(isolate, event.get()));
  }

  for (const auto& entry : enums_) {
    v8::Local<v8::ObjectTemplate> enum_object =
        v8::ObjectTemplate::New(isolate);
    for (const auto& enum_entry : entry.second) {
      enum_object->Set(gin::StringToSymbol(isolate, enum_entry.second),
                       gin::StringToSymbol(isolate, enum_entry.first));
    }
    object_template->Set(isolate, entry.first.c_str(), enum_object);
  }

  if (property_definitions_) {
    DecorateTemplateWithProperties(isolate, object_template,
                                   *property_definitions_);
  }

  object_template_.Set(isolate, object_template);
}

void APIBinding::DecorateTemplateWithProperties(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const base::DictionaryValue& properties) {
  static const char kValueKey[] = "value";
  for (base::DictionaryValue::Iterator iter(properties); !iter.IsAtEnd();
       iter.Advance()) {
    const base::DictionaryValue* dict = nullptr;
    CHECK(iter.value().GetAsDictionary(&dict));
    bool optional = false;
    if (dict->GetBoolean("optional", &optional)) {
      // TODO(devlin): What does optional even mean here? It's only used, it
      // seems, for lastError and inIncognitoContext, which are both handled
      // with custom bindings. Investigate, and remove.
      continue;
    }

    v8::Local<v8::String> v8_key = gin::StringToSymbol(isolate, iter.key());
    std::string ref;
    if (dict->GetString("$ref", &ref)) {
      auto property_data = base::MakeUnique<CustomPropertyData>(
          ref, iter.key(), create_custom_type_);
      object_template->SetLazyDataProperty(
          v8_key, &APIBinding::GetCustomPropertyObject,
          v8::External::New(isolate, property_data.get()));
      custom_properties_.push_back(std::move(property_data));
      continue;
    }

    std::string type;
    CHECK(dict->GetString("type", &type));
    if (type != "object" && !dict->HasKey(kValueKey)) {
      // TODO(devlin): What does a fundamental property not having a value mean?
      // It doesn't seem useful, and looks like it's only used by runtime.id,
      // which is set by custom bindings. Investigate, and remove.
      continue;
    }
    if (type == "integer") {
      int val = 0;
      CHECK(dict->GetInteger(kValueKey, &val));
      object_template->Set(v8_key, v8::Integer::New(isolate, val));
    } else if (type == "boolean") {
      bool val = false;
      CHECK(dict->GetBoolean(kValueKey, &val));
      object_template->Set(v8_key, v8::Boolean::New(isolate, val));
    } else if (type == "string") {
      std::string val;
      CHECK(dict->GetString(kValueKey, &val)) << iter.key();
      object_template->Set(v8_key, gin::StringToSymbol(isolate, val));
    } else if (type == "object" || !ref.empty()) {
      v8::Local<v8::ObjectTemplate> property_template =
          v8::ObjectTemplate::New(isolate);
      const base::DictionaryValue* property_dict = nullptr;
      CHECK(dict->GetDictionary("properties", &property_dict));
      DecorateTemplateWithProperties(isolate, property_template,
                                     *property_dict);
      object_template->Set(v8_key, property_template);
    }
  }
}

// static
void APIBinding::GetEventObject(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = info.Holder()->CreationContext();
  if (!IsContextValid(context))
    return;

  CHECK(info.Data()->IsExternal());
  auto* event_data =
      static_cast<EventData*>(info.Data().As<v8::External>()->Value());
  info.GetReturnValue().Set(event_data->event_handler->CreateEventInstance(
      event_data->full_name, context));
}

void APIBinding::GetCustomPropertyObject(
    v8::Local<v8::Name> property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = info.Holder()->CreationContext();
  if (!IsContextValid(context))
    return;

  CHECK(info.Data()->IsExternal());
  auto* property_data =
      static_cast<CustomPropertyData*>(info.Data().As<v8::External>()->Value());

  v8::Local<v8::Object> property = property_data->create_custom_type.Run(
      context, property_data->type_name, property_data->property_name);
  if (property.IsEmpty())
    return;

  info.GetReturnValue().Set(property);
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
    APIBindingHooks::RequestResult hooks_result = binding_hooks_->RunHooks(
        name, context, signature, &argument_list, *type_refs_);

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

  request_handler_->StartRequest(context, name, std::move(converted_arguments),
                                 callback, custom_callback);
}

}  // namespace extensions
