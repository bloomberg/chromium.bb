// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_value.h"

#include "base/logging.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

// static
blink::mojom::IDBValuePtr IndexedDBValue::ConvertAndEraseValue(
    IndexedDBValue* value) {
  auto mojo_value = blink::mojom::IDBValue::New();
  if (!value->empty()) {
    // TODO(crbug.com/902498): Use mojom traits to map directly from
    //                         std::string.
    const char* value_data = value->bits.data();
    mojo_value->bits =
        std::vector<uint8_t>(value_data, value_data + value->bits.length());
    // Release value->bits std::string.
    value->bits.clear();
  }
  IndexedDBBlobInfo::ConvertBlobInfo(value->blob_info,
                                     &mojo_value->blob_or_file_info);
  return mojo_value;
}

IndexedDBValue::IndexedDBValue() = default;
IndexedDBValue::IndexedDBValue(
    const std::string& input_bits,
    const std::vector<IndexedDBBlobInfo>& input_blob_info)
    : bits(input_bits), blob_info(input_blob_info) {
  DCHECK(input_blob_info.empty() || input_bits.size());
}
IndexedDBValue::IndexedDBValue(const IndexedDBValue& other) = default;
IndexedDBValue::~IndexedDBValue() = default;
IndexedDBValue& IndexedDBValue::operator=(const IndexedDBValue& other) =
    default;

}  // namespace content
