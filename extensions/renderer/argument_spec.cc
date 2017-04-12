// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/argument_spec.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "extensions/renderer/api_type_reference_map.h"
#include "gin/converter.h"
#include "gin/dictionary.h"

namespace extensions {

namespace {

template <class T>
bool ParseFundamentalValueHelper(v8::Local<v8::Value> arg,
                                 v8::Local<v8::Context> context,
                                 const base::Optional<int>& minimum,
                                 std::unique_ptr<base::Value>* out_value) {
  T val;
  if (!gin::Converter<T>::FromV8(context->GetIsolate(), arg, &val))
    return false;
  if (minimum && val < minimum.value())
    return false;
  if (out_value)
    *out_value = base::MakeUnique<base::Value>(val);
  return true;
}

}  // namespace

ArgumentSpec::ArgumentSpec(const base::Value& value)
    : type_(ArgumentType::INTEGER), optional_(false) {
  const base::DictionaryValue* dict = nullptr;
  CHECK(value.GetAsDictionary(&dict));
  dict->GetBoolean("optional", &optional_);
  dict->GetString("name", &name_);

  InitializeType(dict);
}

ArgumentSpec::ArgumentSpec(ArgumentType type) : type_(type), optional_(false) {}

void ArgumentSpec::InitializeType(const base::DictionaryValue* dict) {
  std::string ref_string;
  if (dict->GetString("$ref", &ref_string)) {
    ref_ = std::move(ref_string);
    type_ = ArgumentType::REF;
    return;
  }

  {
    const base::ListValue* choices = nullptr;
    if (dict->GetList("choices", &choices)) {
      DCHECK(!choices->empty());
      type_ = ArgumentType::CHOICES;
      choices_.reserve(choices->GetSize());
      for (const auto& choice : *choices)
        choices_.push_back(base::MakeUnique<ArgumentSpec>(choice));
      return;
    }
  }

  std::string type_string;
  CHECK(dict->GetString("type", &type_string));
  if (type_string == "integer")
    type_ = ArgumentType::INTEGER;
  else if (type_string == "number")
    type_ = ArgumentType::DOUBLE;
  else if (type_string == "object")
    type_ = ArgumentType::OBJECT;
  else if (type_string == "array")
    type_ = ArgumentType::LIST;
  else if (type_string == "boolean")
    type_ = ArgumentType::BOOLEAN;
  else if (type_string == "string")
    type_ = ArgumentType::STRING;
  else if (type_string == "binary")
    type_ = ArgumentType::BINARY;
  else if (type_string == "any")
    type_ = ArgumentType::ANY;
  else if (type_string == "function")
    type_ = ArgumentType::FUNCTION;
  else
    NOTREACHED();

  int min = 0;
  if (dict->GetInteger("minimum", &min))
    minimum_ = min;

  int min_length = 0;
  if (dict->GetInteger("minLength", &min_length) ||
      dict->GetInteger("minItems", &min_length)) {
    DCHECK_GE(min_length, 0);
    min_length_ = min_length;
  }

  int max_length = 0;
  if (dict->GetInteger("maxLength", &max_length) ||
      dict->GetInteger("maxItems", &max_length)) {
    DCHECK_GE(max_length, 0);
    max_length_ = max_length;
  }

  if (type_ == ArgumentType::OBJECT) {
    const base::DictionaryValue* properties_value = nullptr;
    if (dict->GetDictionary("properties", &properties_value)) {
      for (base::DictionaryValue::Iterator iter(*properties_value);
           !iter.IsAtEnd(); iter.Advance()) {
        properties_[iter.key()] = base::MakeUnique<ArgumentSpec>(iter.value());
      }
    }
    const base::DictionaryValue* additional_properties_value = nullptr;
    if (dict->GetDictionary("additionalProperties",
                            &additional_properties_value)) {
      additional_properties_ =
          base::MakeUnique<ArgumentSpec>(*additional_properties_value);
      // Additional properties are always optional.
      additional_properties_->optional_ = true;
    }
    std::string instance_of;
    if (dict->GetString("isInstanceOf", &instance_of)) {
      instance_of_ = instance_of;
    }
  } else if (type_ == ArgumentType::LIST) {
    const base::DictionaryValue* item_value = nullptr;
    CHECK(dict->GetDictionary("items", &item_value));
    list_element_type_ = base::MakeUnique<ArgumentSpec>(*item_value);
  } else if (type_ == ArgumentType::STRING) {
    // Technically, there's no reason enums couldn't be other objects (e.g.
    // numbers), but right now they seem to be exclusively strings. We could
    // always update this if need be.
    const base::ListValue* enums = nullptr;
    if (dict->GetList("enum", &enums)) {
      size_t size = enums->GetSize();
      CHECK_GT(size, 0u);
      for (size_t i = 0; i < size; ++i) {
        std::string enum_value;
        // Enum entries come in two versions: a list of possible strings, and
        // a dictionary with a field 'name'.
        if (!enums->GetString(i, &enum_value)) {
          const base::DictionaryValue* enum_value_dictionary = nullptr;
          CHECK(enums->GetDictionary(i, &enum_value_dictionary));
          CHECK(enum_value_dictionary->GetString("name", &enum_value));
        }
        enum_values_.insert(std::move(enum_value));
      }
    }
  }
}

ArgumentSpec::~ArgumentSpec() {}

bool ArgumentSpec::ParseArgument(v8::Local<v8::Context> context,
                                 v8::Local<v8::Value> value,
                                 const APITypeReferenceMap& refs,
                                 std::unique_ptr<base::Value>* out_value,
                                 std::string* error) const {
  if (type_ == ArgumentType::FUNCTION) {
    if (!value->IsFunction())
      return false;

    if (out_value) {
      // Certain APIs (contextMenus) have functions as parameters other than the
      // callback (contextMenus uses it for an onclick listener). Our generated
      // types have adapted to consider functions "objects" and serialize them
      // as dictionaries.
      // TODO(devlin): It'd be awfully nice to get rid of this eccentricity.
      *out_value = base::MakeUnique<base::DictionaryValue>();
    }
    return true;
  }

  if (type_ == ArgumentType::REF) {
    DCHECK(ref_);
    const ArgumentSpec* reference = refs.GetSpec(ref_.value());
    DCHECK(reference) << ref_.value();
    return reference->ParseArgument(context, value, refs, out_value, error);
  }

  if (type_ == ArgumentType::CHOICES) {
    for (const auto& choice : choices_) {
      if (choice->ParseArgument(context, value, refs, out_value, error))
        return true;
    }
    *error = "Did not match any of the choices";
    return false;
  }

  if (IsFundamentalType())
    return ParseArgumentToFundamental(context, value, out_value, error);
  if (type_ == ArgumentType::OBJECT) {
    // Don't allow functions or arrays (even though they are technically
    // objects). This is to make it easier to match otherwise-ambiguous
    // signatures. For instance, if an API method has an optional object
    // parameter and then an optional callback, we wouldn't necessarily be able
    // to match the arguments if we allowed functions as objects.
    if (!value->IsObject() || value->IsFunction() || value->IsArray()) {
      *error = "Wrong type";
      return false;
    }
    v8::Local<v8::Object> object = value.As<v8::Object>();
    return ParseArgumentToObject(context, object, refs, out_value, error);
  }
  if (type_ == ArgumentType::LIST) {
    if (!value->IsArray()) {
      *error = "Wrong type";
      return false;
    }
    v8::Local<v8::Array> array = value.As<v8::Array>();
    return ParseArgumentToArray(context, array, refs, out_value, error);
  }
  if (type_ == ArgumentType::BINARY) {
    if (!value->IsArrayBuffer() && !value->IsArrayBufferView()) {
      *error = "Wrong type";
      return false;
    }
    return ParseArgumentToAny(context, value, out_value, error);
  }
  if (type_ == ArgumentType::ANY)
    return ParseArgumentToAny(context, value, out_value, error);
  NOTREACHED();
  return false;
}

bool ArgumentSpec::IsFundamentalType() const {
  return type_ == ArgumentType::INTEGER || type_ == ArgumentType::DOUBLE ||
         type_ == ArgumentType::BOOLEAN || type_ == ArgumentType::STRING;
}

bool ArgumentSpec::ParseArgumentToFundamental(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value,
    std::unique_ptr<base::Value>* out_value,
    std::string* error) const {
  DCHECK(IsFundamentalType());
  switch (type_) {
    case ArgumentType::INTEGER:
      return ParseFundamentalValueHelper<int32_t>(value, context, minimum_,
                                                  out_value);
    case ArgumentType::DOUBLE:
      return ParseFundamentalValueHelper<double>(value, context, minimum_,
                                                 out_value);
    case ArgumentType::STRING: {
      if (!value->IsString())
        return false;

      v8::Local<v8::String> v8_string = value.As<v8::String>();
      size_t length = static_cast<size_t>(v8_string->Length());
      if (min_length_ && length < *min_length_) {
        *error = "Less than min length";
        return false;
      }

      if (max_length_ && length > *max_length_) {
        *error = "Greater than max length";
        return false;
      }

      // If we don't need to match enum values and don't need to convert, we're
      // done...
      if (!out_value && enum_values_.empty())
        return true;
      // ...Otherwise, we need to convert to a std::string.
      std::string s;
      // We already checked that this is a string, so this should never fail.
      CHECK(gin::Converter<std::string>::FromV8(context->GetIsolate(), value,
                                                &s));
      if (!enum_values_.empty() && enum_values_.count(s) == 0)
        return false;
      if (out_value) {
        // TODO(devlin): If base::Value ever takes a std::string&&, we
        // could use std::move to construct.
        *out_value = base::MakeUnique<base::Value>(s);
      }
      return true;
    }
    case ArgumentType::BOOLEAN: {
      if (!value->IsBoolean())
        return false;
      if (out_value) {
        *out_value =
            base::MakeUnique<base::Value>(value.As<v8::Boolean>()->Value());
      }
      return true;
    }
    default:
      NOTREACHED();
  }
  return false;
}

bool ArgumentSpec::ParseArgumentToObject(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const APITypeReferenceMap& refs,
    std::unique_ptr<base::Value>* out_value,
    std::string* error) const {
  DCHECK_EQ(ArgumentType::OBJECT, type_);
  std::unique_ptr<base::DictionaryValue> result;
  // Only construct the result if we have an |out_value| to populate.
  if (out_value)
    result = base::MakeUnique<base::DictionaryValue>();

  v8::Local<v8::Array> own_property_names;
  if (!object->GetOwnPropertyNames(context).ToLocal(&own_property_names))
    return false;

  // Track all properties we see from |properties_| to check if any are missing.
  // Use ArgumentSpec* instead of std::string for comparison + copy efficiency.
  std::set<const ArgumentSpec*> seen_properties;
  uint32_t length = own_property_names->Length();
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> key;
    if (!own_property_names->Get(context, i).ToLocal(&key))
      return false;
    // In JS, all keys are strings or numbers (or symbols, but those are
    // excluded by GetOwnPropertyNames()). If you try to set anything else
    // (e.g. an object), it is converted to a string.
    DCHECK(key->IsString() || key->IsNumber());
    v8::String::Utf8Value utf8_key(key);

    ArgumentSpec* property_spec = nullptr;
    auto iter = properties_.find(*utf8_key);
    if (iter != properties_.end()) {
      property_spec = iter->second.get();
      seen_properties.insert(property_spec);
    } else if (additional_properties_) {
      property_spec = additional_properties_.get();
    } else {
      *error = base::StringPrintf("Unknown property: %s", *utf8_key);
      return false;
    }

    v8::Local<v8::Value> prop_value;
    // Fun: It's possible that a previous getter has removed the property from
    // the object. This isn't that big of a deal, since it would only manifest
    // in the case of some reasonably-crazy script objects, and it's probably
    // not worth optimizing for the uncommon case to the detriment of the
    // common (and either should be totally safe). We can always add a
    // HasOwnProperty() check here in the future, if we desire.
    // See also comment in ParseArgumentToArray() about passing in custom
    // crazy values here.
    if (!object->Get(context, key).ToLocal(&prop_value))
      return false;

    // Note: We don't serialize undefined or null values.
    // TODO(devlin): This matches current behavior, but it is correct?
    if (prop_value->IsUndefined() || prop_value->IsNull()) {
      if (!property_spec->optional_) {
        *error = base::StringPrintf("Missing key: %s", *utf8_key);
        return false;
      }
      continue;
    }

    std::unique_ptr<base::Value> property;
    if (!property_spec->ParseArgument(context, prop_value, refs,
                                      result ? &property : nullptr, error)) {
      return false;
    }
    if (result)
      result->SetWithoutPathExpansion(*utf8_key, std::move(property));
  }

  for (const auto& pair : properties_) {
    const ArgumentSpec* spec = pair.second.get();
    if (!spec->optional_ && seen_properties.count(spec) == 0) {
      *error = "Missing key: " + pair.first;
      return false;
    }
  }

  if (instance_of_) {
    // Check for the instance somewhere in the object's prototype chain.
    // NOTE: This only checks that something in the prototype chain was
    // constructed with the same name as the desired instance, but doesn't
    // validate that it's the same constructor as the expected one. For
    // instance, if we expect isInstanceOf == 'Date', script could pass in
    // (function() {
    //   function Date() {}
    //   return new Date();
    // })()
    // Since the object contains 'Date' in its prototype chain, this check
    // succeeds, even though the object is not of built-in type Date.
    // Since this isn't (or at least shouldn't be) a security check, this is
    // okay.
    bool found = false;
    v8::Local<v8::Value> next_check = object;
    do {
      v8::Local<v8::Object> current = next_check.As<v8::Object>();
      v8::String::Utf8Value constructor(current->GetConstructorName());
      if (*instance_of_ ==
          base::StringPiece(*constructor, constructor.length())) {
        found = true;
        break;
      }
      next_check = current->GetPrototype();
    } while (next_check->IsObject());

    if (!found) {
      *error = "Object is not of correct instance";
      return false;
    }
  }

  if (out_value)
    *out_value = std::move(result);
  return true;
}

bool ArgumentSpec::ParseArgumentToArray(v8::Local<v8::Context> context,
                                        v8::Local<v8::Array> value,
                                        const APITypeReferenceMap& refs,
                                        std::unique_ptr<base::Value>* out_value,
                                        std::string* error) const {
  DCHECK_EQ(ArgumentType::LIST, type_);

  uint32_t length = value->Length();
  if (min_length_ && length < *min_length_) {
    *error = "Less than min length";
    return false;
  }

  if (max_length_ && length > *max_length_) {
    *error = "Greater than max length";
    return false;
  }

  std::unique_ptr<base::ListValue> result;
  // Only construct the result if we have an |out_value| to populate.
  if (out_value)
    result = base::MakeUnique<base::ListValue>();

  for (uint32_t i = 0; i < length; ++i) {
    v8::MaybeLocal<v8::Value> maybe_subvalue = value->Get(context, i);
    v8::Local<v8::Value> subvalue;
    // Note: This can fail in the case of a developer passing in the following:
    // var a = [];
    // Object.defineProperty(a, 0, { get: () => { throw new Error('foo'); } });
    // Currently, this will cause the developer-specified error ('foo') to be
    // thrown.
    // TODO(devlin): This is probably fine, but it's worth contemplating
    // catching the error and throwing our own.
    if (!maybe_subvalue.ToLocal(&subvalue))
      return false;
    std::unique_ptr<base::Value> item;
    if (!list_element_type_->ParseArgument(context, subvalue, refs,
                                           result ? &item : nullptr, error)) {
      return false;
    }
    if (result)
      result->Append(std::move(item));
  }
  if (out_value)
    *out_value = std::move(result);
  return true;
}

bool ArgumentSpec::ParseArgumentToAny(v8::Local<v8::Context> context,
                                      v8::Local<v8::Value> value,
                                      std::unique_ptr<base::Value>* out_value,
                                      std::string* error) const {
  DCHECK(type_ == ArgumentType::ANY || type_ == ArgumentType::BINARY);
  if (out_value) {
    std::unique_ptr<content::V8ValueConverter> converter(
        content::V8ValueConverter::create());
    std::unique_ptr<base::Value> converted(
        converter->FromV8Value(value, context));
    if (!converted) {
      *error = "Could not convert to 'any'.";
      return false;
    }
    if (type_ == ArgumentType::BINARY)
      DCHECK_EQ(base::Value::Type::BINARY, converted->GetType());
    *out_value = std::move(converted);
  }
  return true;
}

}  // namespace extensions
