// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_MAPPER_H_
#define CHROMEOS_NETWORK_ONC_ONC_MAPPER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace base {
class Value;
class DictionaryValue;
class ListValue;
}

namespace chromeos {
namespace onc {

struct OncValueSignature;

// This class implements a DeepCopy of base::Values for ONC objects that
// iterates over both the ONC signature and the object hierarchy. DCHECKs if a
// field signature without value signature or an array signature without entry
// signature is reached.
//
// The general term "map" is used here, as this class is meant as base class and
// the copy behavior can be adapted by overriding the methods. By comparing the
// address of a signature object to the list of signatures in "onc_signature.h",
// accurate signature-specific translations or validations can be applied in the
// overriding methods.
//
// The ONC validator and normalizer derive from this class and adapt the default
// copy behavior.
class Mapper {
 public:
  Mapper();
  virtual ~Mapper();

 protected:
  // Calls |MapObject|, |MapArray| and |MapPrimitive| according to |onc_value|'s
  // type. By default aborts on nested errors in arrays. Result of the mapping
  // is returned. On error returns NULL.
  virtual scoped_ptr<base::Value> MapValue(
    const OncValueSignature& signature,
    const base::Value& onc_value);

  // Maps objects/dictionaries. By default calls |MapFields|, which recurses
  // into each field of |onc_object|, and aborts on unknown fields. Result of
  // the mapping is returned. On error returns NULL.
  virtual scoped_ptr<base::DictionaryValue> MapObject(
      const OncValueSignature& signature,
      const base::DictionaryValue& onc_object);

  // Maps primitive values like BinaryValue, StringValue, IntegerValue... (all
  // but dictionaries and lists). By default copies |onc_primitive|. Result of
  // the mapping is returned. On error returns NULL.
  virtual scoped_ptr<base::Value> MapPrimitive(
    const OncValueSignature& signature,
    const base::Value& onc_primitive);

  // Maps each field of the given |onc_object| according to
  // |object_signature|. Adds the mapping of each field to |result| using
  // |MapField| and drops unknown fields by default. Sets
  // |found_unknown_field| to true if this dictionary contains any unknown
  // fields. Set |nested_error_occured| to true if nested errors occured.
  virtual void MapFields(
      const OncValueSignature& object_signature,
      const base::DictionaryValue& onc_object,
      bool* found_unknown_field,
      bool* nested_error_occured,
      base::DictionaryValue* result);

  // Maps the value |onc_value| of field |field_name| according to its field
  // signature in |object_signature| using |MapValue|. Sets
  // |found_unknown_field| to true if |field_name| cannot be found in
  // |object_signature|, which by default is an error. Result of the mapping is
  // returned. On error returns NULL.
  virtual scoped_ptr<base::Value> MapField(
      const std::string& field_name,
      const OncValueSignature& object_signature,
      const base::Value& onc_value,
      bool* found_unknown_field);

  // Maps the array |onc_array| according to |array_signature|, which defines
  // the type of the entries. Maps each entry by calling |MapValue|. If any of
  // the nested mappings failed, the flag |nested_error_occured| is set to true
  // and the entry is dropped from the result. The resulting array is
  // returned. On error returns NULL.
  virtual scoped_ptr<base::ListValue> MapArray(
      const OncValueSignature& array_signature,
      const base::ListValue& onc_array,
      bool* nested_error_occured);

 private:
  DISALLOW_COPY_AND_ASSIGN(Mapper);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_MAPPER_H_
