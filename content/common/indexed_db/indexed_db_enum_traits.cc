// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_enum_traits.h"

using indexed_db::mojom::CursorDirection;
using indexed_db::mojom::DataLoss;
using indexed_db::mojom::OperationType;
using indexed_db::mojom::PutMode;
using indexed_db::mojom::TaskType;
using indexed_db::mojom::TransactionMode;

namespace mojo {

// static
CursorDirection
EnumTraits<CursorDirection, blink::WebIDBCursorDirection>::ToMojom(
    blink::WebIDBCursorDirection input) {
  switch (input) {
    case blink::WebIDBCursorDirectionNext:
      return CursorDirection::Next;
    case blink::WebIDBCursorDirectionNextNoDuplicate:
      return CursorDirection::NextNoDuplicate;
    case blink::WebIDBCursorDirectionPrev:
      return CursorDirection::Prev;
    case blink::WebIDBCursorDirectionPrevNoDuplicate:
      return CursorDirection::PrevNoDuplicate;
  }
  NOTREACHED();
  return CursorDirection::Next;
}

// static
bool EnumTraits<CursorDirection, blink::WebIDBCursorDirection>::FromMojom(
    CursorDirection input,
    blink::WebIDBCursorDirection* output) {
  switch (input) {
    case CursorDirection::Next:
      *output = blink::WebIDBCursorDirectionNext;
      return true;
    case CursorDirection::NextNoDuplicate:
      *output = blink::WebIDBCursorDirectionNextNoDuplicate;
      return true;
    case CursorDirection::Prev:
      *output = blink::WebIDBCursorDirectionPrev;
      return true;
    case CursorDirection::PrevNoDuplicate:
      *output = blink::WebIDBCursorDirectionPrevNoDuplicate;
      return true;
  }
  return false;
}

// static
DataLoss EnumTraits<DataLoss, blink::WebIDBDataLoss>::ToMojom(
    blink::WebIDBDataLoss input) {
  switch (input) {
    case blink::WebIDBDataLossNone:
      return DataLoss::None;
    case blink::WebIDBDataLossTotal:
      return DataLoss::Total;
  }
  NOTREACHED();
  return DataLoss::None;
}

// static
bool EnumTraits<DataLoss, blink::WebIDBDataLoss>::FromMojom(
    DataLoss input,
    blink::WebIDBDataLoss* output) {
  switch (input) {
    case DataLoss::None:
      *output = blink::WebIDBDataLossNone;
      return true;
    case DataLoss::Total:
      *output = blink::WebIDBDataLossTotal;
      return true;
  }
  return false;
}

// static
OperationType EnumTraits<OperationType, blink::WebIDBOperationType>::ToMojom(
    blink::WebIDBOperationType input) {
  switch (input) {
    case blink::WebIDBAdd:
      return OperationType::Add;
    case blink::WebIDBPut:
      return OperationType::Put;
    case blink::WebIDBDelete:
      return OperationType::Delete;
    case blink::WebIDBClear:
      return OperationType::Clear;
    case blink::WebIDBOperationTypeCount:
      // WebIDBOperationTypeCount is not a valid option.
      break;
  }
  NOTREACHED();
  return OperationType::Add;
}

// static
bool EnumTraits<OperationType, blink::WebIDBOperationType>::FromMojom(
    OperationType input,
    blink::WebIDBOperationType* output) {
  switch (input) {
    case OperationType::Add:
      *output = blink::WebIDBAdd;
      return true;
    case OperationType::Put:
      *output = blink::WebIDBPut;
      return true;
    case OperationType::Delete:
      *output = blink::WebIDBDelete;
      return true;
    case OperationType::Clear:
      *output = blink::WebIDBClear;
      return true;
  }
  return false;
}

// static
PutMode EnumTraits<PutMode, blink::WebIDBPutMode>::ToMojom(
    blink::WebIDBPutMode input) {
  switch (input) {
    case blink::WebIDBPutModeAddOrUpdate:
      return PutMode::AddOrUpdate;
    case blink::WebIDBPutModeAddOnly:
      return PutMode::AddOnly;
    case blink::WebIDBPutModeCursorUpdate:
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
      *output = blink::WebIDBPutModeAddOrUpdate;
      return true;
    case PutMode::AddOnly:
      *output = blink::WebIDBPutModeAddOnly;
      return true;
    case PutMode::CursorUpdate:
      *output = blink::WebIDBPutModeCursorUpdate;
      return true;
  }
  return false;
}

// static
TaskType EnumTraits<TaskType, blink::WebIDBTaskType>::ToMojom(
    blink::WebIDBTaskType input) {
  switch (input) {
    case blink::WebIDBTaskTypeNormal:
      return TaskType::Normal;
    case blink::WebIDBTaskTypePreemptive:
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
      *output = blink::WebIDBTaskTypeNormal;
      return true;
    case TaskType::Preemptive:
      *output = blink::WebIDBTaskTypePreemptive;
      return true;
  }
  return false;
}

// static
TransactionMode
EnumTraits<TransactionMode, blink::WebIDBTransactionMode>::ToMojom(
    blink::WebIDBTransactionMode input) {
  switch (input) {
    case blink::WebIDBTransactionModeReadOnly:
      return TransactionMode::ReadOnly;
    case blink::WebIDBTransactionModeReadWrite:
      return TransactionMode::ReadWrite;
    case blink::WebIDBTransactionModeVersionChange:
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
      *output = blink::WebIDBTransactionModeReadOnly;
      return true;
    case TransactionMode::ReadWrite:
      *output = blink::WebIDBTransactionModeReadWrite;
      return true;
    case TransactionMode::VersionChange:
      *output = blink::WebIDBTransactionModeVersionChange;
      return true;
  }
  return false;
}

}  // namespace mojo
