// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_signature.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "gin/arguments.h"

namespace extensions {

APISignature::APISignature(const base::ListValue& specification) {
  signature_.reserve(specification.GetSize());
  for (const auto& value : specification) {
    const base::DictionaryValue* param = nullptr;
    CHECK(value->GetAsDictionary(&param));
    signature_.push_back(base::MakeUnique<ArgumentSpec>(*param));
  }
}

APISignature::~APISignature() {}

std::unique_ptr<base::ListValue> APISignature::ParseArguments(
    gin::Arguments* arguments,
    const ArgumentSpec::RefMap& type_refs,
    v8::Local<v8::Function>* callback_out,
    std::string* error) const {
  auto results = base::MakeUnique<base::ListValue>();

  // TODO(devlin): This is how extension APIs have always determined if a
  // function has a callback, but it seems a little silly. In the long run (once
  // signatures are generated), it probably makes sense to indicate this
  // differently.
  bool signature_has_callback =
      !signature_.empty() &&
      signature_.back()->type() == ArgumentType::FUNCTION;

  v8::Local<v8::Context> context = arguments->isolate()->GetCurrentContext();

  size_t end_size =
      signature_has_callback ? signature_.size() - 1 : signature_.size();
  for (size_t i = 0; i < end_size; ++i) {
    std::unique_ptr<base::Value> parsed =
        ParseArgument(*signature_[i], context, arguments, type_refs, error);
    if (!parsed)
      return nullptr;
    results->Append(std::move(parsed));
  }

  v8::Local<v8::Function> callback_value;
  if (signature_has_callback &&
      !ParseCallback(arguments, *signature_.back(), error, &callback_value)) {
    return nullptr;
  }

  if (!arguments->PeekNext().IsEmpty())
    return nullptr;  // Extra arguments aren't allowed.

  *callback_out = callback_value;
  return results;
}

std::unique_ptr<base::Value> APISignature::ParseArgument(
    const ArgumentSpec& spec,
    v8::Local<v8::Context> context,
    gin::Arguments* arguments,
    const ArgumentSpec::RefMap& type_refs,
    std::string* error) const {
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

bool APISignature::ParseCallback(gin::Arguments* arguments,
                                 const ArgumentSpec& callback_spec,
                                 std::string* error,
                                 v8::Local<v8::Function>* callback_out) const {
  v8::Local<v8::Value> value = arguments->PeekNext();
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) {
    if (!callback_spec.optional()) {
      *error = "Missing required argument: " + callback_spec.name();
      return false;
    }
    arguments->Skip();
    return true;
  }

  if (!value->IsFunction()) {
    *error = "Argument is wrong type: " + callback_spec.name();
    return false;
  }

  *callback_out = value.As<v8::Function>();
  arguments->Skip();
  return true;
}

}  // namespace extensions
