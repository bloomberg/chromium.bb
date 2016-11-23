// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_param_traits.h"
#include "content/common/indexed_db/indexed_db_struct_traits.h"
#include "mojo/common/common_custom_types_struct_traits.h"

using content::IndexedDBKey;
using content::IndexedDBKeyPath;
using content::IndexedDBKeyRange;

namespace mojo {

// static
indexed_db::mojom::KeyDataPtr
StructTraits<indexed_db::mojom::KeyDataView, IndexedDBKey>::data(
    const IndexedDBKey& key) {
  auto data = indexed_db::mojom::KeyData::New();
  switch (key.type()) {
    case blink::WebIDBKeyTypeInvalid:
      data->set_other(indexed_db::mojom::DatalessKeyType::Invalid);
      return data;
    case blink::WebIDBKeyTypeArray:
      data->set_key_array(key.array());
      return data;
    case blink::WebIDBKeyTypeBinary:
      data->set_binary(std::vector<uint8_t>(
          key.binary().data(), key.binary().data() + key.binary().size()));
      return data;
    case blink::WebIDBKeyTypeString:
      data->set_string(key.string());
      return data;
    case blink::WebIDBKeyTypeDate:
      data->set_date(key.date());
      return data;
    case blink::WebIDBKeyTypeNumber:
      data->set_number(key.number());
      return data;
    case blink::WebIDBKeyTypeNull:
      data->set_other(indexed_db::mojom::DatalessKeyType::Null);
      return data;
    case blink::WebIDBKeyTypeMin:
      break;
  }
  NOTREACHED();
  return data;
}

// static
bool StructTraits<indexed_db::mojom::KeyDataView, IndexedDBKey>::Read(
    indexed_db::mojom::KeyDataView data,
    IndexedDBKey* out) {
  indexed_db::mojom::KeyDataDataView data_view;
  data.GetDataDataView(&data_view);

  switch (data_view.tag()) {
    case indexed_db::mojom::KeyDataDataView::Tag::KEY_ARRAY: {
      std::vector<IndexedDBKey> array;
      if (!data_view.ReadKeyArray(&array))
        return false;
      *out = IndexedDBKey(array);
      return true;
    }
    case indexed_db::mojom::KeyDataDataView::Tag::BINARY: {
      std::vector<uint8_t> binary;
      if (!data_view.ReadBinary(&binary))
        return false;
      *out = IndexedDBKey(
          std::string(binary.data(), binary.data() + binary.size()));
      return true;
    }
    case indexed_db::mojom::KeyDataDataView::Tag::STRING: {
      base::string16 string;
      if (!data_view.ReadString(&string))
        return false;
      *out = IndexedDBKey(string);
      return true;
    }
    case indexed_db::mojom::KeyDataDataView::Tag::DATE:
      *out = IndexedDBKey(data_view.date(), blink::WebIDBKeyTypeDate);
      return true;
    case indexed_db::mojom::KeyDataDataView::Tag::NUMBER:
      *out = IndexedDBKey(data_view.number(), blink::WebIDBKeyTypeNumber);
      return true;
    case indexed_db::mojom::KeyDataDataView::Tag::OTHER:
      switch (data_view.other()) {
        case indexed_db::mojom::DatalessKeyType::Invalid:
          *out = IndexedDBKey(blink::WebIDBKeyTypeInvalid);
          return true;
        case indexed_db::mojom::DatalessKeyType::Null:
          *out = IndexedDBKey(blink::WebIDBKeyTypeNull);
          return true;
      }
  }

  return false;
}

// static
indexed_db::mojom::KeyPathDataPtr
StructTraits<indexed_db::mojom::KeyPathDataView, IndexedDBKeyPath>::data(
    const IndexedDBKeyPath& key_path) {
  if (key_path.IsNull())
    return nullptr;

  auto data = indexed_db::mojom::KeyPathData::New();
  switch (key_path.type()) {
    case blink::WebIDBKeyPathTypeString:
      data->set_string(key_path.string());
      return data;
    case blink::WebIDBKeyPathTypeArray:
      data->set_string_array(key_path.array());
      return data;
    default:
      NOTREACHED();
      return data;
  }
}

// static
bool StructTraits<indexed_db::mojom::KeyPathDataView, IndexedDBKeyPath>::Read(
    indexed_db::mojom::KeyPathDataView data,
    IndexedDBKeyPath* out) {
  indexed_db::mojom::KeyPathDataDataView data_view;
  data.GetDataDataView(&data_view);

  if (data_view.is_null()) {
    *out = IndexedDBKeyPath();
    return true;
  }

  switch (data_view.tag()) {
    case indexed_db::mojom::KeyPathDataDataView::Tag::STRING: {
      base::string16 string;
      if (!data_view.ReadString(&string))
        return false;
      *out = IndexedDBKeyPath(string);
      return true;
    }
    case indexed_db::mojom::KeyPathDataDataView::Tag::STRING_ARRAY: {
      std::vector<base::string16> array;
      if (!data_view.ReadStringArray(&array))
        return false;
      *out = IndexedDBKeyPath(array);
      return true;
    }
  }

  return false;
}

// static
bool StructTraits<indexed_db::mojom::KeyRangeDataView, IndexedDBKeyRange>::Read(
    indexed_db::mojom::KeyRangeDataView data,
    IndexedDBKeyRange* out) {
  IndexedDBKey lower;
  IndexedDBKey upper;
  if (!data.ReadLower(&lower) || !data.ReadUpper(&upper))
    return false;

  *out = IndexedDBKeyRange(lower, upper, data.lower_open(), data.upper_open());
  return true;
}

// static
bool StructTraits<indexed_db::mojom::IndexKeysDataView,
                  content::IndexedDBIndexKeys>::
    Read(indexed_db::mojom::IndexKeysDataView data,
         content::IndexedDBIndexKeys* out) {
  out->first = data.index_id();
  return data.ReadIndexKeys(&out->second);
}

// static
bool StructTraits<indexed_db::mojom::IndexMetadataDataView,
                  content::IndexedDBIndexMetadata>::
    Read(indexed_db::mojom::IndexMetadataDataView data,
         content::IndexedDBIndexMetadata* out) {
  out->id = data.id();
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadKeyPath(&out->key_path))
    return false;
  out->unique = data.unique();
  out->multi_entry = data.multi_entry();
  return true;
}

// static
bool StructTraits<indexed_db::mojom::ObjectStoreMetadataDataView,
                  content::IndexedDBObjectStoreMetadata>::
    Read(indexed_db::mojom::ObjectStoreMetadataDataView data,
         content::IndexedDBObjectStoreMetadata* out) {
  out->id = data.id();
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadKeyPath(&out->key_path))
    return false;
  out->auto_increment = data.auto_increment();
  out->max_index_id = data.max_index_id();
  ArrayDataView<indexed_db::mojom::IndexMetadataDataView> indexes;
  data.GetIndexesDataView(&indexes);
  for (size_t i = 0; i < indexes.size(); ++i) {
    indexed_db::mojom::IndexMetadataDataView index;
    indexes.GetDataView(i, &index);
    DCHECK(!base::ContainsKey(out->indexes, index.id()));
    if (!StructTraits<
            indexed_db::mojom::IndexMetadataDataView,
            content::IndexedDBIndexMetadata>::Read(index,
                                                   &out->indexes[index.id()]))
      return false;
  }
  return true;
}

// static
bool StructTraits<indexed_db::mojom::DatabaseMetadataDataView,
                  content::IndexedDBDatabaseMetadata>::
    Read(indexed_db::mojom::DatabaseMetadataDataView data,
         content::IndexedDBDatabaseMetadata* out) {
  out->id = data.id();
  if (!data.ReadName(&out->name))
    return false;
  out->version = data.version();
  out->max_object_store_id = data.max_object_store_id();
  ArrayDataView<indexed_db::mojom::ObjectStoreMetadataDataView> object_stores;
  data.GetObjectStoresDataView(&object_stores);
  for (size_t i = 0; i < object_stores.size(); ++i) {
    indexed_db::mojom::ObjectStoreMetadataDataView object_store;
    object_stores.GetDataView(i, &object_store);
    DCHECK(!base::ContainsKey(out->object_stores, object_store.id()));
    if (!StructTraits<indexed_db::mojom::ObjectStoreMetadataDataView,
                      content::IndexedDBObjectStoreMetadata>::
            Read(object_store, &out->object_stores[object_store.id()]))
      return false;
  }
  return true;
}

}  // namespace mojo
