// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_mapper.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"

namespace chromeos {
namespace onc {

Mapper::Mapper() = default;

Mapper::~Mapper() = default;

std::unique_ptr<base::Value> Mapper::MapValue(
    const OncValueSignature& signature,
    const base::Value& onc_value,
    bool* error) {
  std::unique_ptr<base::Value> result_value;
  switch (onc_value.type()) {
    case base::Value::Type::DICTIONARY: {
      const base::DictionaryValue* dict = NULL;
      onc_value.GetAsDictionary(&dict);
      result_value = MapObject(signature, *dict, error);
      break;
    }
    case base::Value::Type::LIST: {
      const base::ListValue* list = NULL;
      onc_value.GetAsList(&list);
      result_value = MapArray(signature, *list, error);
      break;
    }
    default: {
      result_value = MapPrimitive(signature, onc_value, error);
      break;
    }
  }

  return result_value;
}

std::unique_ptr<base::DictionaryValue> Mapper::MapObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object,
    bool* error) {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);

  bool found_unknown_field = false;
  MapFields(signature, onc_object, &found_unknown_field, error, result.get());
  if (found_unknown_field)
    *error = true;
  return result;
}

std::unique_ptr<base::Value> Mapper::MapPrimitive(
    const OncValueSignature& signature,
    const base::Value& onc_primitive,
    bool* error) {
  return base::WrapUnique(onc_primitive.DeepCopy());
}

void Mapper::MapFields(const OncValueSignature& object_signature,
                       const base::DictionaryValue& onc_object,
                       bool* found_unknown_field,
                       bool* nested_error,
                       base::DictionaryValue* result) {
  for (base::DictionaryValue::Iterator it(onc_object); !it.IsAtEnd();
       it.Advance()) {
    bool current_field_unknown = false;
    std::unique_ptr<base::Value> result_value =
        MapField(it.key(), object_signature, it.value(), &current_field_unknown,
                 nested_error);

    if (current_field_unknown)
      *found_unknown_field = true;
    else if (result_value.get() != NULL)
      result->SetWithoutPathExpansion(it.key(), std::move(result_value));
    else
      DCHECK(*nested_error);
  }
}

std::unique_ptr<base::Value> Mapper::MapField(
    const std::string& field_name,
    const OncValueSignature& object_signature,
    const base::Value& onc_value,
    bool* found_unknown_field,
    bool* error) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(object_signature, field_name);

  if (field_signature != NULL) {
    DCHECK(field_signature->value_signature != NULL)
        << "Found missing value signature at field '" << field_name << "'.";

    return MapValue(*field_signature->value_signature, onc_value, error);
  } else {
    DVLOG(1) << "Found unknown field name: '" << field_name << "'";
    *found_unknown_field = true;
    return std::unique_ptr<base::Value>();
  }
}

std::unique_ptr<base::ListValue> Mapper::MapArray(
    const OncValueSignature& array_signature,
    const base::ListValue& onc_array,
    bool* nested_error) {
  DCHECK(array_signature.onc_array_entry_signature != NULL)
      << "Found missing onc_array_entry_signature.";

  std::unique_ptr<base::ListValue> result_array(new base::ListValue);
  int original_index = 0;
  for (const auto& entry : onc_array) {
    std::unique_ptr<base::Value> result_entry;
    result_entry =
        MapEntry(original_index, *array_signature.onc_array_entry_signature,
                 entry, nested_error);
    if (result_entry.get() != NULL)
      result_array->Append(std::move(result_entry));
    else
      DCHECK(*nested_error);
    ++original_index;
  }
  return result_array;
}

std::unique_ptr<base::Value> Mapper::MapEntry(
    int index,
    const OncValueSignature& signature,
    const base::Value& onc_value,
    bool* error) {
  return MapValue(signature, onc_value, error);
}

}  // namespace onc
}  // namespace chromeos
