// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/values_struct_traits.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"

namespace mojo {

bool StructTraits<common::mojom::ListValueDataView,
                  std::unique_ptr<base::ListValue>>::
    Read(common::mojom::ListValueDataView data,
         std::unique_ptr<base::ListValue>* value_out) {
  mojo::ArrayDataView<common::mojom::ValueDataView> view;
  data.GetValuesDataView(&view);

  auto list_value = std::make_unique<base::ListValue>();
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
  std::vector<base::Value::DictStorage::value_type> dict_storage;
  dict_storage.reserve(view.size());
  for (size_t i = 0; i < view.size(); ++i) {
    base::StringPiece key;
    std::unique_ptr<base::Value> value;
    if (!view.keys().Read(i, &key) || !view.values().Read(i, &value))
      return false;

    dict_storage.emplace_back(key.as_string(), std::move(value));
  }
  *value_out = base::DictionaryValue::From(
      std::make_unique<base::Value>(base::Value::DictStorage(
          std::move(dict_storage), base::KEEP_LAST_OF_DUPES)));
  return true;
}

std::unique_ptr<base::DictionaryValue>
CloneTraits<std::unique_ptr<base::DictionaryValue>, false>::Clone(
    const std::unique_ptr<base::DictionaryValue>& input) {
  return input ? input->CreateDeepCopy() : nullptr;
}

bool UnionTraits<common::mojom::ValueDataView, std::unique_ptr<base::Value>>::
    Read(common::mojom::ValueDataView data,
         std::unique_ptr<base::Value>* value_out) {
  switch (data.tag()) {
    case common::mojom::ValueDataView::Tag::NULL_VALUE: {
      *value_out = std::make_unique<base::Value>();
      return true;
    }
    case common::mojom::ValueDataView::Tag::BOOL_VALUE: {
      *value_out = std::make_unique<base::Value>(data.bool_value());
      return true;
    }
    case common::mojom::ValueDataView::Tag::INT_VALUE: {
      *value_out = std::make_unique<base::Value>(data.int_value());
      return true;
    }
    case common::mojom::ValueDataView::Tag::DOUBLE_VALUE: {
      *value_out = std::make_unique<base::Value>(data.double_value());
      return true;
    }
    case common::mojom::ValueDataView::Tag::STRING_VALUE: {
      base::StringPiece string_value;
      if (!data.ReadStringValue(&string_value))
        return false;
      *value_out = std::make_unique<base::Value>(string_value);
      return true;
    }
    case common::mojom::ValueDataView::Tag::BINARY_VALUE: {
      mojo::ArrayDataView<uint8_t> binary_data;
      data.GetBinaryValueDataView(&binary_data);
      *value_out = base::Value::CreateWithCopiedBuffer(
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

std::unique_ptr<base::Value>
CloneTraits<std::unique_ptr<base::Value>, false>::Clone(
    const std::unique_ptr<base::Value>& input) {
  return input ? input->CreateDeepCopy() : nullptr;
}

}  // namespace mojo
