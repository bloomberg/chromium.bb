// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_mapper.h"

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"

namespace chromeos {
namespace onc {

Mapper::Mapper() {
}

Mapper::~Mapper() {
}

scoped_ptr<base::Value> Mapper::MapValue(
    const OncValueSignature& signature,
    const base::Value& onc_value) {
  scoped_ptr<base::Value> result_value;
  switch (onc_value.GetType()) {
    case base::Value::TYPE_DICTIONARY: {
      const base::DictionaryValue* dict = NULL;
      onc_value.GetAsDictionary(&dict);
      result_value = MapObject(signature, *dict);
      break;
    }
    case base::Value::TYPE_LIST: {
      const base::ListValue* list = NULL;
      onc_value.GetAsList(&list);
      bool nested_error_occured = false;
      result_value = MapArray(signature, *list, &nested_error_occured);
      if (nested_error_occured)
        result_value.reset();
      break;
    }
    default: {
      result_value = MapPrimitive(signature, onc_value);
      break;
    }
  }

  return result_value.Pass();
}

scoped_ptr<base::DictionaryValue> Mapper::MapObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object) {
  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue);

  bool found_unknown_field = false;
  bool nested_error_occured = false;
  MapFields(signature, onc_object, &found_unknown_field, &nested_error_occured,
            result.get());
  if (!nested_error_occured && !found_unknown_field)
    return result.Pass();
  else
    return scoped_ptr<base::DictionaryValue>();
}

scoped_ptr<base::Value> Mapper::MapPrimitive(
    const OncValueSignature& signature,
    const base::Value& onc_primitive) {
  return make_scoped_ptr(onc_primitive.DeepCopy());
}

void Mapper::MapFields(
    const OncValueSignature& object_signature,
    const base::DictionaryValue& onc_object,
    bool* found_unknown_field,
    bool* nested_error_occured,
    base::DictionaryValue* result) {

  for (base::DictionaryValue::Iterator it(onc_object); it.HasNext();
       it.Advance()) {
    bool current_field_unknown = false;
    scoped_ptr<base::Value> result_value = MapField(
        it.key(), object_signature, it.value(), &current_field_unknown);

    if (current_field_unknown)
      *found_unknown_field = true;
    else if (result_value.get() != NULL)
      result->SetWithoutPathExpansion(it.key(), result_value.release());
    else
      *nested_error_occured = true;
  }
}

scoped_ptr<base::Value> Mapper::MapField(
    const std::string& field_name,
    const OncValueSignature& object_signature,
    const base::Value& onc_value,
    bool* found_unknown_field) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(object_signature, field_name);

  if (field_signature != NULL) {
    if (field_signature->value_signature == NULL) {
      NOTREACHED() << "Found missing value signature at field '"
                   << field_name << "'.";
      return scoped_ptr<base::Value>();
    }

    return MapValue(*field_signature->value_signature, onc_value);
  } else {
    DVLOG(1) << "Found unknown field name: '" << field_name << "'";
    *found_unknown_field = true;
    return scoped_ptr<base::Value>();
  }
}

scoped_ptr<base::ListValue> Mapper::MapArray(
    const OncValueSignature& array_signature,
    const base::ListValue& onc_array,
    bool* nested_error_occured) {
  if (array_signature.onc_array_entry_signature == NULL) {
    NOTREACHED() << "Found missing onc_array_entry_signature.";
    return scoped_ptr<base::ListValue>();
  }

  scoped_ptr<base::ListValue> result_array(new base::ListValue);
  for (base::ListValue::const_iterator it = onc_array.begin();
       it != onc_array.end(); ++it) {
    const base::Value* entry = *it;

    scoped_ptr<base::Value> result_entry;
    result_entry = MapValue(*array_signature.onc_array_entry_signature, *entry);
    if (result_entry.get() != NULL)
      result_array->Append(result_entry.release());
    else
      *nested_error_occured = true;
  }
  return result_array.Pass();
}

}  // namespace onc
}  // namespace chromeos
