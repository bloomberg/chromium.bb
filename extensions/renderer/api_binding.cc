// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/argument_spec.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/arguments.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {

const char kExtensionAPIPerContextKey[] = "extension_api_binding";

using APISignature = std::vector<std::unique_ptr<ArgumentSpec>>;

// Returns the expected APISignature for a dictionary describing an API method.
std::unique_ptr<APISignature> GetAPISignature(
    const base::DictionaryValue& dict) {
  std::unique_ptr<APISignature> signature = base::MakeUnique<APISignature>();
  const base::ListValue* params = nullptr;
  CHECK(dict.GetList("parameters", &params));
  signature->reserve(params->GetSize());
  for (const auto& value : *params) {
    const base::DictionaryValue* param = nullptr;
    CHECK(value->GetAsDictionary(&param));
    std::string name;
    CHECK(param->GetString("name", &name));
    signature->push_back(base::MakeUnique<ArgumentSpec>(*param));
  }
  return signature;
}

// Attempts to match an argument from |arguments| to the given |spec|.
// If the next argmument does not match and |spec| is optional, a null
// base::Value is returned.
// If the argument matches, |arguments| is advanced and the converted value is
// returned.
// If the argument does not match and it is not optional, returns null and
// populates error.
std::unique_ptr<base::Value> ParseArgument(
    const ArgumentSpec& spec,
    v8::Local<v8::Context> context,
    gin::Arguments* arguments,
    const ArgumentSpec::RefMap& type_refs,
    std::string* error) {
  v8::Local<v8::Value> value = arguments->PeekNext();
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) {
    if (!spec.optional()) {
      *error = "Missing required argument: " + spec.name();
      return nullptr;
    }
    // This is safe to call even if |arguments| is at the end (which can happen
    // if n optional arguments are omitted at the end of the signature).
    arguments->Skip();
    return base::Value::CreateNullValue();
  }

  std::unique_ptr<base::Value> result =
      spec.ConvertArgument(context, value, type_refs, error);
  if (!result) {
    if (!spec.optional()) {
      *error = "Missing required argument: " + spec.name();
      return nullptr;
    }
    return base::Value::CreateNullValue();
  }

  arguments->Skip();
  return result;
}

// Parses |args| against |signature| and populates error with any errors.
std::unique_ptr<base::ListValue> ParseArguments(
    const APISignature* signature,
    gin::Arguments* arguments,
    const ArgumentSpec::RefMap& type_refs,
    std::string* error) {
  auto results = base::MakeUnique<base::ListValue>();

  v8::Local<v8::Context> context = arguments->isolate()->GetCurrentContext();

  for (const auto& argument_spec : *signature) {
    std::unique_ptr<base::Value> parsed =
        ParseArgument(*argument_spec, context, arguments, type_refs, error);
    if (!parsed)
      return nullptr;
    results->Append(std::move(parsed));
  }

  if (!arguments->PeekNext().IsEmpty())
    return nullptr;  // Extra arguments aren't allowed.

  return results;
}

void CallbackHelper(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);
  v8::Local<v8::External> external;
  CHECK(args.GetData(&external));
  auto callback = static_cast<APIBinding::HandlerCallback*>(external->Value());
  callback->Run(&args);
}

}  // namespace

APIBinding::APIPerContextData::APIPerContextData() {}
APIBinding::APIPerContextData::~APIPerContextData() {}

APIBinding::APIBinding(const std::string& name,
                       const base::ListValue& function_definitions,
                       const base::ListValue& type_definitions,
                       const APIMethodCallback& callback,
                       ArgumentSpec::RefMap* type_refs)
    : method_callback_(callback), type_refs_(type_refs), weak_factory_(this) {
  DCHECK(!method_callback_.is_null());
  for (const auto& func : function_definitions) {
    const base::DictionaryValue* func_dict = nullptr;
    CHECK(func->GetAsDictionary(&func_dict));
    std::string name;
    CHECK(func_dict->GetString("name", &name));
    std::unique_ptr<APISignature> spec = GetAPISignature(*func_dict);
    signatures_[name] = std::move(spec);
  }
  for (const auto& type : type_definitions) {
    const base::DictionaryValue* type_dict = nullptr;
    CHECK(type->GetAsDictionary(&type_dict));
    std::string id;
    CHECK(type_dict->GetString("id", &id));
    DCHECK(type_refs->find(id) == type_refs->end());
    // TODO(devlin): refs are sometimes preceeded by the API namespace; we might
    // need to take that into account.
    (*type_refs)[id] = base::MakeUnique<ArgumentSpec>(*type_dict);
  }
}

APIBinding::~APIBinding() {}

v8::Local<v8::Object> APIBinding::CreateInstance(v8::Local<v8::Context> context,
                                                 v8::Isolate* isolate) {
  // TODO(devlin): APIs may change depending on which features are available,
  // but we should be able to cache the unconditional methods on an object
  // template, create the object, and then add any conditional methods.
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
    auto handler_callback = base::MakeUnique<HandlerCallback>(
        base::Bind(&APIBinding::HandleCall, weak_factory_.GetWeakPtr(),
                   sig.first, sig.second.get()));
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

  return object;
}

void APIBinding::HandleCall(const std::string& name,
                            const APISignature* signature,
                            gin::Arguments* arguments) {
  std::string error;
  v8::HandleScope handle_scope(arguments->isolate());
  std::unique_ptr<base::ListValue> parsed_arguments;
  {
    v8::TryCatch try_catch(arguments->isolate());
    parsed_arguments = ParseArguments(signature, arguments,
                                      *type_refs_, &error);
    if (try_catch.HasCaught()) {
      DCHECK(!parsed_arguments);
      try_catch.ReThrow();
      return;
    }
  }
  if (!parsed_arguments) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }
  method_callback_.Run(name, std::move(parsed_arguments));
}

}  // namespace extensions
