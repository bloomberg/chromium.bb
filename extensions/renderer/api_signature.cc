// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_signature.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/renderer/argument_spec.h"
#include "gin/arguments.h"

namespace extensions {

namespace {

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
    list_value_->Append(base::Value::CreateNullValue());
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
  // TODO(devlin): This is how extension APIs have always determined if a
  // function has a callback, but it seems a little silly. In the long run (once
  // signatures are generated), it probably makes sense to indicate this
  // differently.
  bool signature_has_callback =
      !signature_.empty() &&
      signature_.back()->type() == ArgumentType::FUNCTION;

  size_t end_size =
      signature_has_callback ? signature_.size() - 1 : signature_.size();
  for (size_t i = 0; i < end_size; ++i) {
    if (!ParseArgument(*signature_[i]))
      return false;
  }

  if (signature_has_callback && !ParseCallback(*signature_.back()))
    return false;

  if (current_index_ != arguments_.size())
    return false;  // Extra arguments aren't allowed.

  return true;
}

bool ArgumentParser::ParseArgument(const ArgumentSpec& spec) {
  v8::Local<v8::Value> value = next_argument();
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) {
    if (!spec.optional()) {
      *error_ = "Missing required argument: " + spec.name();
      return false;
    }
    // This is safe to call even if |arguments| is at the end (which can happen
    // if n optional arguments are omitted at the end of the signature).
    ConsumeArgument();

    AddNull();
    return true;
  }

  if (!spec.ParseArgument(context_, value, type_refs_, GetBuffer(), error_)) {
    if (!spec.optional()) {
      *error_ = "Missing required argument: " + spec.name();
      return false;
    }

    AddNull();
    return true;
  }

  ConsumeArgument();
  AddParsedArgument(value);
  return true;
}

bool ArgumentParser::ParseCallback(const ArgumentSpec& spec) {
  v8::Local<v8::Value> value = next_argument();
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) {
    if (!spec.optional()) {
      *error_ = "Missing required argument: " + spec.name();
      return false;
    }
    ConsumeArgument();
    return true;
  }

  if (!value->IsFunction()) {
    *error_ = "Argument is wrong type: " + spec.name();
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
    CHECK(value->GetAsDictionary(&param));
    signature_.push_back(base::MakeUnique<ArgumentSpec>(*param));
  }
}

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
  std::unique_ptr<base::ListValue> json = base::MakeUnique<base::ListValue>();
  BaseValueArgumentParser parser(
      context, signature_, arguments, type_refs, error, json.get());
  if (!parser.ParseArguments())
    return false;
  *json_out = std::move(json);
  *callback_out = parser.callback();
  return true;
}

}  // namespace extensions
