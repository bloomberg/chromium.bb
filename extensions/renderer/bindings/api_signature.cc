// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_signature.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "gin/arguments.h"

namespace extensions {

namespace {

bool HasCallback(const std::vector<std::unique_ptr<ArgumentSpec>>& signature) {
  // TODO(devlin): This is how extension APIs have always determined if a
  // function has a callback, but it seems a little silly. In the long run (once
  // signatures are generated), it probably makes sense to indicate this
  // differently.
  return !signature.empty() &&
         signature.back()->type() == ArgumentType::FUNCTION;
}

// A class to help with argument parsing. Note that this uses v8::Locals and
// const&s because it's an implementation detail of the APISignature; this
// should *only* be used directly on the stack!
class ArgumentParser {
 public:
  ArgumentParser(v8::Local<v8::Context> context,
                 const std::vector<std::unique_ptr<ArgumentSpec>>& signature,
                 const std::vector<v8::Local<v8::Value>>& arguments,
                 const APITypeReferenceMap& type_refs,
                 std::string* error)
      : context_(context),
        signature_(signature),
        arguments_(arguments),
        type_refs_(type_refs),
        error_(error) {}

  // Tries to parse the arguments against the expected signature.
  bool ParseArguments();

 protected:
  v8::Isolate* GetIsolate() { return context_->GetIsolate(); }

 private:
  v8::Local<v8::Value> next_argument() {
    return current_index_ < arguments_.size() ?
        arguments_[current_index_] : v8::Local<v8::Value>();
  }

  void ConsumeArgument() {
    current_index_ = std::min(arguments_.size(), current_index_ + 1);
  }

  // Attempts to match the next argument to the given |spec|.
  // If the next argument does not match and |spec| is optional, uses a null
  // value.
  // Returns true on success.
  bool ParseArgument(const ArgumentSpec& spec);

  // Attempts to parse the callback from the given |spec|. Returns true on
  // success.
  bool ParseCallback(const ArgumentSpec& spec);

  // Adds a null value to the parsed arguments.
  virtual void AddNull() = 0;
  virtual void AddNullCallback() = 0;
  // Returns a base::Value to be populated during argument matching.
  virtual std::unique_ptr<base::Value>* GetBuffer() = 0;
  // Adds a new parsed argument.
  virtual void AddParsedArgument(v8::Local<v8::Value> value) = 0;
  // Adds the parsed callback.
  virtual void SetCallback(v8::Local<v8::Function> callback) = 0;

  v8::Local<v8::Context> context_;
  const std::vector<std::unique_ptr<ArgumentSpec>>& signature_;
  const std::vector<v8::Local<v8::Value>>& arguments_;
  const APITypeReferenceMap& type_refs_;
  std::string* error_;
  size_t current_index_ = 0;

  // An error to pass while parsing arguments to avoid having to allocate a new
  // std::string on the stack multiple times.
  std::string parse_error_;

  DISALLOW_COPY_AND_ASSIGN(ArgumentParser);
};

class V8ArgumentParser : public ArgumentParser {
 public:
  V8ArgumentParser(v8::Local<v8::Context> context,
                   const std::vector<std::unique_ptr<ArgumentSpec>>& signature,
                   const std::vector<v8::Local<v8::Value>>& arguments,
                   const APITypeReferenceMap& type_refs,
                   std::string* error,
                   std::vector<v8::Local<v8::Value>>* values)
      : ArgumentParser(context, signature, arguments, type_refs, error),
        values_(values) {}

 private:
  void AddNull() override { values_->push_back(v8::Null(GetIsolate())); }
  void AddNullCallback() override {
    values_->push_back(v8::Null(GetIsolate()));
  }
  std::unique_ptr<base::Value>* GetBuffer() override { return nullptr; }
  void AddParsedArgument(v8::Local<v8::Value> value) override {
    values_->push_back(value);
  }
  void SetCallback(v8::Local<v8::Function> callback) override {
    values_->push_back(callback);
  }

  std::vector<v8::Local<v8::Value>>* values_;

  DISALLOW_COPY_AND_ASSIGN(V8ArgumentParser);
};

class BaseValueArgumentParser : public ArgumentParser {
 public:
  BaseValueArgumentParser(
      v8::Local<v8::Context> context,
      const std::vector<std::unique_ptr<ArgumentSpec>>& signature,
      const std::vector<v8::Local<v8::Value>>& arguments,
      const APITypeReferenceMap& type_refs,
      std::string* error,
      base::ListValue* list_value)
      : ArgumentParser(context, signature, arguments, type_refs, error),
        list_value_(list_value) {}

  v8::Local<v8::Function> callback() { return callback_; }

 private:
  void AddNull() override {
    list_value_->Append(std::make_unique<base::Value>());
  }
  void AddNullCallback() override {
    // The base::Value conversion doesn't include the callback directly, so we
    // don't add a null parameter here.
  }
  std::unique_ptr<base::Value>* GetBuffer() override { return &last_arg_; }
  void AddParsedArgument(v8::Local<v8::Value> value) override {
    // The corresponding base::Value is expected to have been stored in
    // |last_arg_| already.
    DCHECK(last_arg_);
    list_value_->Append(std::move(last_arg_));
    last_arg_.reset();
  }
  void SetCallback(v8::Local<v8::Function> callback) override {
    callback_ = callback;
  }

  base::ListValue* list_value_;
  std::unique_ptr<base::Value> last_arg_;
  v8::Local<v8::Function> callback_;

  DISALLOW_COPY_AND_ASSIGN(BaseValueArgumentParser);
};

bool ArgumentParser::ParseArguments() {
  if (arguments_.size() > signature_.size()) {
    *error_ = api_errors::TooManyArguments();
    return false;
  }

  bool signature_has_callback = HasCallback(signature_);

  size_t end_size =
      signature_has_callback ? signature_.size() - 1 : signature_.size();
  for (size_t i = 0; i < end_size; ++i) {
    if (!ParseArgument(*signature_[i]))
      return false;
  }

  if (signature_has_callback && !ParseCallback(*signature_.back()))
    return false;

  if (current_index_ != arguments_.size()) {
    // This can potentially happen even if the check above for too many
    // arguments succeeds when optional parameters are omitted. For instance,
    // if the signature expects (optional int, function callback) and the caller
    // provides (function callback, object random), the first size check and
    // callback spec would succeed, but we wouldn't consume all the arguments.
    *error_ = api_errors::TooManyArguments();
    return false;  // Extra arguments aren't allowed.
  }

  return true;
}

bool ArgumentParser::ParseArgument(const ArgumentSpec& spec) {
  v8::Local<v8::Value> value = next_argument();
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) {
    if (!spec.optional()) {
      *error_ = api_errors::MissingRequiredArgument(spec.name().c_str());
      return false;
    }
    // This is safe to call even if |arguments| is at the end (which can happen
    // if n optional arguments are omitted at the end of the signature).
    ConsumeArgument();

    AddNull();
    return true;
  }

  if (!spec.IsCorrectType(value, type_refs_, &parse_error_)) {
    if (!spec.optional()) {
      *error_ = api_errors::ArgumentError(spec.name(), parse_error_);
      return false;
    }

    AddNull();
    return true;
  }

  if (!spec.ParseArgument(context_, value, type_refs_, GetBuffer(),
                          &parse_error_)) {
    *error_ = api_errors::ArgumentError(spec.name(), parse_error_);
    return false;
  }

  ConsumeArgument();
  AddParsedArgument(value);
  return true;
}

bool ArgumentParser::ParseCallback(const ArgumentSpec& spec) {
  v8::Local<v8::Value> value = next_argument();
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) {
    if (!spec.optional()) {
      *error_ = api_errors::MissingRequiredArgument(spec.name().c_str());
      return false;
    }
    ConsumeArgument();
    AddNullCallback();
    return true;
  }

  if (!spec.ParseArgument(context_, value, type_refs_, nullptr,
                          &parse_error_)) {
    *error_ = api_errors::ArgumentError(spec.name(), parse_error_);
    return false;
  }

  ConsumeArgument();
  SetCallback(value.As<v8::Function>());
  return true;
}

}  // namespace

APISignature::APISignature(const base::ListValue& specification) {
  signature_.reserve(specification.GetSize());
  for (const auto& value : specification) {
    const base::DictionaryValue* param = nullptr;
    CHECK(value.GetAsDictionary(&param));
    signature_.push_back(std::make_unique<ArgumentSpec>(*param));
  }
}

APISignature::APISignature(std::vector<std::unique_ptr<ArgumentSpec>> signature)
    : signature_(std::move(signature)) {}

APISignature::~APISignature() {}

bool APISignature::ParseArgumentsToV8(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments,
    const APITypeReferenceMap& type_refs,
    std::vector<v8::Local<v8::Value>>* v8_out,
    std::string* error) const {
  DCHECK(v8_out);
  std::vector<v8::Local<v8::Value>> v8_values;
  V8ArgumentParser parser(
      context, signature_, arguments, type_refs, error, &v8_values);
  if (!parser.ParseArguments())
    return false;
  *v8_out = std::move(v8_values);
  return true;
}

bool APISignature::ParseArgumentsToJSON(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments,
    const APITypeReferenceMap& type_refs,
    std::unique_ptr<base::ListValue>* json_out,
    v8::Local<v8::Function>* callback_out,
    std::string* error) const {
  DCHECK(json_out);
  DCHECK(callback_out);
  std::unique_ptr<base::ListValue> json = std::make_unique<base::ListValue>();
  BaseValueArgumentParser parser(
      context, signature_, arguments, type_refs, error, json.get());
  if (!parser.ParseArguments())
    return false;
  *json_out = std::move(json);
  *callback_out = parser.callback();
  return true;
}

bool APISignature::ConvertArgumentsIgnoringSchema(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments,
    std::unique_ptr<base::ListValue>* json_out,
    v8::Local<v8::Function>* callback_out) const {
  size_t size = arguments.size();
  v8::Local<v8::Function> callback;
  // TODO(devlin): This is what the current bindings do, but it's quite terribly
  // incorrect. We only hit this flow when an API method has a hook to update
  // the arguments post-validation, and in some cases, the arguments returned by
  // that hook do *not* match the signature of the API method (e.g.
  // fileSystem.getDisplayPath); see also note in api_bindings.cc for why this
  // is bad. But then here, we *rely* on the signature to determine whether or
  // not the last parameter is a callback, even though the hooks may not return
  // the arguments in the signature. This is very broken.
  if (HasCallback(signature_)) {
    CHECK(!arguments.empty());
    v8::Local<v8::Value> value = arguments.back();
    --size;
    // Bindings should ensure that the value here is appropriate, but see the
    // comment above for limitations.
    DCHECK(value->IsFunction() || value->IsUndefined() || value->IsNull());
    if (value->IsFunction())
      callback = value.As<v8::Function>();
  }

  auto json = std::make_unique<base::ListValue>();
  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();
  converter->SetFunctionAllowed(true);
  for (size_t i = 0; i < size; ++i) {
    std::unique_ptr<base::Value> converted =
        converter->FromV8Value(arguments[i], context);
    if (!converted)
      return false;
    json->Append(std::move(converted));
  }

  *json_out = std::move(json);
  *callback_out = callback;
  return true;
}

std::string APISignature::GetExpectedSignature() const {
  if (!expected_signature_.empty() || signature_.empty())
    return expected_signature_;

  std::vector<std::string> pieces;
  pieces.reserve(signature_.size());
  const char* kOptionalPrefix = "optional ";
  for (const auto& spec : signature_) {
    pieces.push_back(
        base::StringPrintf("%s%s %s", spec->optional() ? kOptionalPrefix : "",
                           spec->GetTypeName().c_str(), spec->name().c_str()));
  }
  expected_signature_ = base::JoinString(pieces, ", ");

  return expected_signature_;
}

}  // namespace extensions
