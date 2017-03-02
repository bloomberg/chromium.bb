// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "mojo/common/values_struct_traits.h"

namespace mojo {

bool StructTraits<common::mojom::ListValueDataView,
                  std::unique_ptr<base::ListValue>>::
    Read(common::mojom::ListValueDataView data,
         std::unique_ptr<base::ListValue>* value_out) {
  mojo::ArrayDataView<common::mojom::ValueDataView> view;
  data.GetValuesDataView(&view);

  auto list_value = base::MakeUnique<base::ListValue>();
  for (size_t i = 0; i < view.size(); ++i) {
    std::unique_ptr<base::Value> value;
    if (!view.Read(i, &value))
      return false;

    list_value->Append(std::move(value));
  }
  *value_out = std::move(list_value);
  return true;
}

bool StructTraits<common::mojom::DictionaryValueDataView,
                  std::unique_ptr<base::DictionaryValue>>::
    Read(common::mojom::DictionaryValueDataView data,
         std::unique_ptr<base::DictionaryValue>* value_out) {
  mojo::MapDataView<mojo::StringDataView, common::mojom::ValueDataView> view;
  data.GetValuesDataView(&view);
  auto dictionary_value = base::MakeUnique<base::DictionaryValue>();
  for (size_t i = 0; i < view.size(); ++i) {
    base::StringPiece key;
    std::unique_ptr<base::Value> value;
    if (!view.keys().Read(i, &key) || !view.values().Read(i, &value))
      return false;

    dictionary_value->SetWithoutPathExpansion(key, std::move(value));
  }
  *value_out = std::move(dictionary_value);
  return true;
}

std::unique_ptr<base::DictionaryValue>
CloneTraits<std::unique_ptr<base::DictionaryValue>, false>::Clone(
    const std::unique_ptr<base::DictionaryValue>& input) {
  auto result = base::MakeUnique<base::DictionaryValue>();
  result->MergeDictionary(input.get());
  return result;
}

bool UnionTraits<common::mojom::ValueDataView, std::unique_ptr<base::Value>>::
    Read(common::mojom::ValueDataView data,
         std::unique_ptr<base::Value>* value_out) {
  switch (data.tag()) {
    case common::mojom::ValueDataView::Tag::NULL_VALUE: {
      *value_out = base::Value::CreateNullValue();
      return true;
    }
    case common::mojom::ValueDataView::Tag::BOOL_VALUE: {
      *value_out = base::MakeUnique<base::Value>(data.bool_value());
      return true;
    }
    case common::mojom::ValueDataView::Tag::INT_VALUE: {
      *value_out = base::MakeUnique<base::Value>(data.int_value());
      return true;
    }
    case common::mojom::ValueDataView::Tag::DOUBLE_VALUE: {
      *value_out = base::MakeUnique<base::Value>(data.double_value());
      return true;
    }
    case common::mojom::ValueDataView::Tag::STRING_VALUE: {
      base::StringPiece string_value;
      if (!data.ReadStringValue(&string_value))
        return false;
      *value_out = base::MakeUnique<base::StringValue>(string_value);
      return true;
    }
    case common::mojom::ValueDataView::Tag::BINARY_VALUE: {
      mojo::ArrayDataView<uint8_t> binary_data;
      data.GetBinaryValueDataView(&binary_data);
      *value_out = base::BinaryValue::CreateWithCopiedBuffer(
          reinterpret_cast<const char*>(binary_data.data()),
          binary_data.size());
      return true;
    }
    case common::mojom::ValueDataView::Tag::DICTIONARY_VALUE: {
      std::unique_ptr<base::DictionaryValue> dictionary_value;
      if (!data.ReadDictionaryValue(&dictionary_value))
        return false;
      *value_out = std::move(dictionary_value);
      return true;
    }
    case common::mojom::ValueDataView::Tag::LIST_VALUE: {
      std::unique_ptr<base::ListValue> list_value;
      if (!data.ReadListValue(&list_value))
        return false;
      *value_out = std::move(list_value);
      return true;
    }
  }
  return false;
}

}  // namespace mojo
