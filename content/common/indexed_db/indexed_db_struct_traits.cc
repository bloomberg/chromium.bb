// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_struct_traits.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_struct_traits.h"

using blink::IndexedDBKey;
using blink::IndexedDBIndexKeys;
using content::IndexedDBKeyPath;
using content::IndexedDBKeyRange;
using indexed_db::mojom::PutMode;
using indexed_db::mojom::TaskType;
using indexed_db::mojom::TransactionMode;

namespace mojo {

// static
indexed_db::mojom::KeyPathDataPtr
StructTraits<indexed_db::mojom::KeyPathDataView, IndexedDBKeyPath>::data(
    const IndexedDBKeyPath& key_path) {
  if (key_path.IsNull())
    return nullptr;

  auto data = indexed_db::mojom::KeyPathData::New();
  switch (key_path.type()) {
    case blink::kWebIDBKeyPathTypeString:
      data->set_string(key_path.string());
      return data;
    case blink::kWebIDBKeyPathTypeArray:
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
bool StructTraits<indexed_db::mojom::IndexKeysDataView, IndexedDBIndexKeys>::
    Read(indexed_db::mojom::IndexKeysDataView data, IndexedDBIndexKeys* out) {
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

// static
PutMode EnumTraits<PutMode, blink::WebIDBPutMode>::ToMojom(
    blink::WebIDBPutMode input) {
  switch (input) {
    case blink::kWebIDBPutModeAddOrUpdate:
      return PutMode::AddOrUpdate;
    case blink::kWebIDBPutModeAddOnly:
      return PutMode::AddOnly;
    case blink::kWebIDBPutModeCursorUpdate:
      return PutMode::CursorUpdate;
  }
  NOTREACHED();
  return PutMode::AddOrUpdate;
}

// static
bool EnumTraits<PutMode, blink::WebIDBPutMode>::FromMojom(
    PutMode input,
    blink::WebIDBPutMode* output) {
  switch (input) {
    case PutMode::AddOrUpdate:
      *output = blink::kWebIDBPutModeAddOrUpdate;
      return true;
    case PutMode::AddOnly:
      *output = blink::kWebIDBPutModeAddOnly;
      return true;
    case PutMode::CursorUpdate:
      *output = blink::kWebIDBPutModeCursorUpdate;
      return true;
  }
  return false;
}

// static
TaskType EnumTraits<TaskType, blink::WebIDBTaskType>::ToMojom(
    blink::WebIDBTaskType input) {
  switch (input) {
    case blink::kWebIDBTaskTypeNormal:
      return TaskType::Normal;
    case blink::kWebIDBTaskTypePreemptive:
      return TaskType::Preemptive;
  }
  NOTREACHED();
  return TaskType::Normal;
}

// static
bool EnumTraits<TaskType, blink::WebIDBTaskType>::FromMojom(
    TaskType input,
    blink::WebIDBTaskType* output) {
  switch (input) {
    case TaskType::Normal:
      *output = blink::kWebIDBTaskTypeNormal;
      return true;
    case TaskType::Preemptive:
      *output = blink::kWebIDBTaskTypePreemptive;
      return true;
  }
  return false;
}

// static
TransactionMode
EnumTraits<TransactionMode, blink::WebIDBTransactionMode>::ToMojom(
    blink::WebIDBTransactionMode input) {
  switch (input) {
    case blink::kWebIDBTransactionModeReadOnly:
      return TransactionMode::ReadOnly;
    case blink::kWebIDBTransactionModeReadWrite:
      return TransactionMode::ReadWrite;
    case blink::kWebIDBTransactionModeVersionChange:
      return TransactionMode::VersionChange;
  }
  NOTREACHED();
  return TransactionMode::ReadOnly;
}

// static
bool EnumTraits<TransactionMode, blink::WebIDBTransactionMode>::FromMojom(
    TransactionMode input,
    blink::WebIDBTransactionMode* output) {
  switch (input) {
    case TransactionMode::ReadOnly:
      *output = blink::kWebIDBTransactionModeReadOnly;
      return true;
    case TransactionMode::ReadWrite:
      *output = blink::kWebIDBTransactionModeReadWrite;
      return true;
    case TransactionMode::VersionChange:
      *output = blink::kWebIDBTransactionModeVersionChange;
      return true;
  }
  return false;
}
}  // namespace mojo
