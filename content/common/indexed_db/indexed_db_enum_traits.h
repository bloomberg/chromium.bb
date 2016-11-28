// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_ENUM_TRAITS_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_ENUM_TRAITS_H_

#include "content/common/indexed_db/indexed_db.mojom.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace mojo {

template <>
struct EnumTraits<indexed_db::mojom::CursorDirection,
                  blink::WebIDBCursorDirection> {
  static indexed_db::mojom::CursorDirection ToMojom(
      blink::WebIDBCursorDirection input);
  static bool FromMojom(indexed_db::mojom::CursorDirection input,
                        blink::WebIDBCursorDirection* output);
};

template <>
struct EnumTraits<indexed_db::mojom::DataLoss, blink::WebIDBDataLoss> {
  static indexed_db::mojom::DataLoss ToMojom(blink::WebIDBDataLoss input);
  static bool FromMojom(indexed_db::mojom::DataLoss input,
                        blink::WebIDBDataLoss* output);
};

template <>
struct EnumTraits<indexed_db::mojom::OperationType,
                  blink::WebIDBOperationType> {
  static indexed_db::mojom::OperationType ToMojom(
      blink::WebIDBOperationType input);
  static bool FromMojom(indexed_db::mojom::OperationType input,
                        blink::WebIDBOperationType* output);
};

template <>
struct EnumTraits<indexed_db::mojom::PutMode, blink::WebIDBPutMode> {
  static indexed_db::mojom::PutMode ToMojom(blink::WebIDBPutMode input);
  static bool FromMojom(indexed_db::mojom::PutMode input,
                        blink::WebIDBPutMode* output);
};

template <>
struct EnumTraits<indexed_db::mojom::TaskType, blink::WebIDBTaskType> {
  static indexed_db::mojom::TaskType ToMojom(blink::WebIDBTaskType input);
  static bool FromMojom(indexed_db::mojom::TaskType input,
                        blink::WebIDBTaskType* output);
};

template <>
struct EnumTraits<indexed_db::mojom::TransactionMode,
                  blink::WebIDBTransactionMode> {
  static indexed_db::mojom::TransactionMode ToMojom(
      blink::WebIDBTransactionMode input);
  static bool FromMojom(indexed_db::mojom::TransactionMode input,
                        blink::WebIDBTransactionMode* output);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_ENUM_TRAITS_H_
