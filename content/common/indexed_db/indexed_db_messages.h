// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "content/common/indexed_db/indexed_db_param_traits.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

// Singly-included section for typedefs in multiply-included file.
#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_MESSAGES_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_MESSAGES_H_

// IPC_MESSAGE macros fail on the std::map, when expanding. We need to define
// a type to avoid that.
// Map observer_id to corresponding set of indices in observations.
typedef std::map<int32_t, std::vector<int32_t>> ObservationIndex;

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_MESSAGES_H_

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START IndexedDBMsgStart

// Argument structures used in messages

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBCursorDirection,
                          blink::WebIDBCursorDirectionLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBPutMode, blink::WebIDBPutModeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBTaskType, blink::WebIDBTaskTypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBTransactionMode,
                          blink::WebIDBTransactionModeLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBDataLoss, blink::WebIDBDataLossTotal)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebIDBOperationType,
                          blink::WebIDBOperationTypeLast)

IPC_STRUCT_BEGIN(IndexedDBMsg_Observation)
  IPC_STRUCT_MEMBER(int64_t, object_store_id)
  IPC_STRUCT_MEMBER(blink::WebIDBOperationType, type)
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_ObserverChanges)
  IPC_STRUCT_MEMBER(ObservationIndex, observation_index)
  IPC_STRUCT_MEMBER(std::vector<IndexedDBMsg_Observation>, observations)
IPC_STRUCT_END()

// Indexed DB messages sent from the browser to the renderer.

// The thread_id needs to be the first parameter in these messages.  In the IO
// thread on the renderer/client process, an IDB message filter assumes the
// thread_id is the first int.

// IDBDatabaseCallback message handlers
IPC_MESSAGE_CONTROL2(IndexedDBMsg_DatabaseCallbacksChanges,
                     int32_t, /* ipc_thread_id */
                     IndexedDBMsg_ObserverChanges)
