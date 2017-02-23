// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_VALUES_STRUCT_TRAITS_H_
#define MOJO_COMMON_VALUES_STRUCT_TRAITS_H_

#include "base/values.h"
#include "mojo/common/values.mojom.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/map_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

namespace mojo {

template <>
struct ArrayTraits<base::ListValue> {
  using Element = std::unique_ptr<base::Value>;
  using ConstIterator = base::ListValue::const_iterator;

  static size_t GetSize(const base::ListValue& input) {
    return input.GetSize();
  }

  static ConstIterator GetBegin(const base::ListValue& input) {
    return input.begin();
  }

  static void AdvanceIterator(ConstIterator& iterator) { ++iterator; }

  static const Element& GetValue(ConstIterator& iterator) { return *iterator; }
};

template <>
struct StructTraits<common::mojom::ListValueDataView, base::ListValue> {
  static const base::ListValue& values(const base::ListValue& value) {
    return value;
  }
};

template <>
struct StructTraits<common::mojom::ListValueDataView,
                    std::unique_ptr<base::ListValue>> {
  static bool IsNull(const std::unique_ptr<base::ListValue>& value) {
    return !value;
  }

  static void SetToNull(std::unique_ptr<base::ListValue>* value) {
    value->reset();
  }

  static const base::ListValue& values(
      const std::unique_ptr<base::ListValue>& value) {
    return *value;
  }

  static bool Read(common::mojom::ListValueDataView data,
                   std::unique_ptr<base::ListValue>* value);
};

template <>
struct MapTraits<base::DictionaryValue> {
  using Key = std::string;
  using Value = base::Value;
  using Iterator = base::DictionaryValue::Iterator;

  static size_t GetSize(const base::DictionaryValue& input) {
    return input.size();
  }

  static Iterator GetBegin(const base::DictionaryValue& input) {
    return Iterator(input);
  }

  static void AdvanceIterator(Iterator& iterator) { iterator.Advance(); }

  static const Key& GetKey(Iterator& iterator) { return iterator.key(); }

  static const Value& GetValue(Iterator& iterator) { return iterator.value(); }
};

template <>
struct StructTraits<common::mojom::DictionaryValueDataView,
                    base::DictionaryValue> {
  static const base::DictionaryValue& values(
      const base::DictionaryValue& value) {
    return value;
  }
};

template <>
struct StructTraits<common::mojom::DictionaryValueDataView,
                    std::unique_ptr<base::DictionaryValue>> {
  static bool IsNull(const std::unique_ptr<base::DictionaryValue>& value) {
    return !value;
  }

  static void SetToNull(std::unique_ptr<base::DictionaryValue>* value) {
    value->reset();
  }

  static const base::DictionaryValue& values(
      const std::unique_ptr<base::DictionaryValue>& value) {
    return *value;
  }
  static bool Read(common::mojom::DictionaryValueDataView data,
                   std::unique_ptr<base::DictionaryValue>* value);
};

template <>
struct CloneTraits<std::unique_ptr<base::DictionaryValue>, false> {
  static std::unique_ptr<base::DictionaryValue> Clone(
      const std::unique_ptr<base::DictionaryValue>& input);
};

template <>
struct UnionTraits<common::mojom::ValueDataView, base::Value> {
  static common::mojom::ValueDataView::Tag GetTag(const base::Value& data) {
    switch (data.GetType()) {
      case base::Value::Type::NONE:
        return common::mojom::ValueDataView::Tag::NULL_VALUE;
      case base::Value::Type::BOOLEAN:
        return common::mojom::ValueDataView::Tag::BOOL_VALUE;
      case base::Value::Type::INTEGER:
        return common::mojom::ValueDataView::Tag::INT_VALUE;
      case base::Value::Type::DOUBLE:
        return common::mojom::ValueDataView::Tag::DOUBLE_VALUE;
      case base::Value::Type::STRING:
        return common::mojom::ValueDataView::Tag::STRING_VALUE;
      case base::Value::Type::BINARY:
        return common::mojom::ValueDataView::Tag::BINARY_VALUE;
      case base::Value::Type::DICTIONARY:
        return common::mojom::ValueDataView::Tag::DICTIONARY_VALUE;
      case base::Value::Type::LIST:
        return common::mojom::ValueDataView::Tag::LIST_VALUE;
    }
    NOTREACHED();
    return common::mojom::ValueDataView::Tag::NULL_VALUE;
  }

  static common::mojom::NullValuePtr null_value(const base::Value& value) {
    return common::mojom::NullValuePtr();
  }

  static bool bool_value(const base::Value& value) {
    bool bool_value{};
    if (!value.GetAsBoolean(&bool_value))
      NOTREACHED();
    return bool_value;
  }

  static int32_t int_value(const base::Value& value) {
    int int_value{};
    if (!value.GetAsInteger(&int_value))
      NOTREACHED();
    return int_value;
  }

  static double double_value(const base::Value& value) {
    double double_value{};
    if (!value.GetAsDouble(&double_value))
      NOTREACHED();
    return double_value;
  }

  static base::StringPiece string_value(const base::Value& value) {
    base::StringPiece string_piece;
    if (!value.GetAsString(&string_piece))
      NOTREACHED();
    return string_piece;
  }

  static mojo::ConstCArray<uint8_t> binary_value(const base::Value& value) {
    const base::BinaryValue* binary_value = nullptr;
    if (!value.GetAsBinary(&binary_value))
      NOTREACHED();
    return mojo::ConstCArray<uint8_t>(
        binary_value->GetSize(),
        reinterpret_cast<const uint8_t*>(binary_value->GetBuffer()));
  }

  static const base::ListValue& list_value(const base::Value& value) {
    const base::ListValue* list_value = nullptr;
    if (!value.GetAsList(&list_value))
      NOTREACHED();
    return *list_value;
  }
  static const base::DictionaryValue& dictionary_value(
      const base::Value& value) {
    const base::DictionaryValue* dictionary_value = nullptr;
    if (!value.GetAsDictionary(&dictionary_value))
      NOTREACHED();
    return *dictionary_value;
  }
};

template <>
struct UnionTraits<common::mojom::ValueDataView, std::unique_ptr<base::Value>> {
  static bool IsNull(const std::unique_ptr<base::Value>& value) {
    return !value;
  }

  static void SetToNull(std::unique_ptr<base::Value>* value) { value->reset(); }

  static common::mojom::ValueDataView::Tag GetTag(
      const std::unique_ptr<base::Value>& value) {
    return UnionTraits<common::mojom::ValueDataView, base::Value>::GetTag(
        *value);
  }

  static common::mojom::NullValuePtr null_value(
      const std::unique_ptr<base::Value>& value) {
    return UnionTraits<common::mojom::ValueDataView, base::Value>::null_value(
        *value);
  }
  static bool bool_value(const std::unique_ptr<base::Value>& value) {
    return UnionTraits<common::mojom::ValueDataView, base::Value>::bool_value(
        *value);
  }
  static int32_t int_value(const std::unique_ptr<base::Value>& value) {
    return UnionTraits<common::mojom::ValueDataView, base::Value>::int_value(
        *value);
  }
  static double double_value(const std::unique_ptr<base::Value>& value) {
    return UnionTraits<common::mojom::ValueDataView, base::Value>::double_value(
        *value);
  }
  static base::StringPiece string_value(
      const std::unique_ptr<base::Value>& value) {
    return UnionTraits<common::mojom::ValueDataView, base::Value>::string_value(
        *value);
  }
  static mojo::ConstCArray<uint8_t> binary_value(
      const std::unique_ptr<base::Value>& value) {
    return UnionTraits<common::mojom::ValueDataView, base::Value>::binary_value(
        *value);
  }
  static const base::ListValue& list_value(
      const std::unique_ptr<base::Value>& value) {
    return UnionTraits<common::mojom::ValueDataView, base::Value>::list_value(
        *value);
  }
  static const base::DictionaryValue& dictionary_value(
      const std::unique_ptr<base::Value>& value) {
    return UnionTraits<common::mojom::ValueDataView,
                       base::Value>::dictionary_value(*value);
  }

  static bool Read(common::mojom::ValueDataView data,
                   std::unique_ptr<base::Value>* value);
};

}  // namespace mojo

#endif  // MOJO_COMMON_VALUES_STRUCT_TRAITS_H_
