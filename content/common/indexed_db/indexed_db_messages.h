// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <vector>

#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "content/common/indexed_db/indexed_db_param_traits.h"
#include "content/public/common/serialized_script_value.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExceptionCode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBMetadata.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransaction.h"

#define IPC_MESSAGE_START IndexedDBMsgStart

// Argument structures used in messages

IPC_ENUM_TRAITS(WebKit::WebIDBObjectStore::PutMode)
IPC_ENUM_TRAITS(WebKit::WebIDBCursor::Direction)
IPC_ENUM_TRAITS(WebKit::WebIDBTransaction::TaskType)
IPC_ENUM_TRAITS(WebKit::WebIDBDatabase::PutMode)
IPC_ENUM_TRAITS(WebKit::WebIDBDatabase::TaskType)

// Used to enumerate indexed databases.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_FactoryGetDatabaseNames_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The origin doing the initiating.
  IPC_STRUCT_MEMBER(string16, origin)
IPC_STRUCT_END()

// Used to open an indexed database.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_FactoryOpen_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  // Identifier of the request
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // Identifier for database callbacks
  IPC_STRUCT_MEMBER(int32, ipc_database_response_id)
  // The origin doing the initiating.
  IPC_STRUCT_MEMBER(string16, origin)
  // The name of the database.
  IPC_STRUCT_MEMBER(string16, name)
  // The transaction id used if a database upgrade is needed.
  IPC_STRUCT_MEMBER(int64, transaction_id)
  // The requested version of the database.
  IPC_STRUCT_MEMBER(int64, version)
IPC_STRUCT_END()

// Used to delete an indexed database.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_FactoryDeleteDatabase_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The origin doing the initiating.
  IPC_STRUCT_MEMBER(string16, origin)
  // The name of the database.
  IPC_STRUCT_MEMBER(string16, name)
IPC_STRUCT_END()

// Used to create an object store.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseCreateObjectStore_Params)
  // The storage id of the object store.
  IPC_STRUCT_MEMBER(int64, id)
  // The name of the object store.
  IPC_STRUCT_MEMBER(string16, name)
  // The keyPath of the object store.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, key_path)
  // Whether the object store created should have a key generator.
  IPC_STRUCT_MEMBER(bool, auto_increment)
  // The transaction this is associated with.
  IPC_STRUCT_MEMBER(int32, ipc_transaction_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32, ipc_database_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseGet_Params)
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  // The id any response should contain.
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32, ipc_database_id)
  // The transaction its associated with.
  IPC_STRUCT_MEMBER(int64, transaction_id)
  // The object store's id.
  IPC_STRUCT_MEMBER(int64, object_store_id)
  // The index's id.
  IPC_STRUCT_MEMBER(int64, index_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // If this is just retrieving the key
  IPC_STRUCT_MEMBER(bool, key_only)
IPC_STRUCT_END()

// Used to set a value in an object store.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabasePut_Params)
  // The id any response should contain.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32, ipc_database_id)
  // The transaction it's associated with.
  IPC_STRUCT_MEMBER(int64, transaction_id)
  // The object store's id.
  IPC_STRUCT_MEMBER(int64, object_store_id)
  // The index's id.
  IPC_STRUCT_MEMBER(int64, index_id)
  // The value to set.
  IPC_STRUCT_MEMBER(std::vector<uint8>, value)
  // The key to set it on (may not be "valid"/set in some cases).
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  // Whether this is an add or a put.
  IPC_STRUCT_MEMBER(WebKit::WebIDBDatabase::PutMode, put_mode)
  // The names of the indexes used below.
  IPC_STRUCT_MEMBER(std::vector<int64>, index_ids)
  // The keys for each index, such that each inner vector corresponds
  // to each index named in index_names, respectively.
  IPC_STRUCT_MEMBER(std::vector<std::vector<content::IndexedDBKey> >,
                    index_keys)
IPC_STRUCT_END()

// Used to open both cursors and object cursors in IndexedDB.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseOpenCursor_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32, ipc_database_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int64, transaction_id)
  // The object store.
  IPC_STRUCT_MEMBER(int64, object_store_id)
  // The index if any.
  IPC_STRUCT_MEMBER(int64, index_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The direction of this cursor.
  IPC_STRUCT_MEMBER(int32, direction)
  // If this is just retrieving the key
  IPC_STRUCT_MEMBER(bool, key_only)
  // The priority of this cursor.
  IPC_STRUCT_MEMBER(WebKit::WebIDBDatabase::TaskType, task_type)
IPC_STRUCT_END()

// Used to open both cursors and object cursors in IndexedDB.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseCount_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int64, transaction_id)
  // The IPC id of the database.
  IPC_STRUCT_MEMBER(int32, ipc_database_id)
  // The object store.
  IPC_STRUCT_MEMBER(int64, object_store_id)
  // The index if any.
  IPC_STRUCT_MEMBER(int64, index_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseDeleteRange_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The IPC id of the database.
  IPC_STRUCT_MEMBER(int32, ipc_database_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int64, transaction_id)
  // The object store.
  IPC_STRUCT_MEMBER(int64, object_store_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
IPC_STRUCT_END()

// Used to open both cursors and object cursors in IndexedDB.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_IndexOpenCursor_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The direction of this cursor.
  IPC_STRUCT_MEMBER(int32, direction)
  // The index the index belongs to.
  IPC_STRUCT_MEMBER(int32, ipc_index_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int, ipc_transaction_id)
IPC_STRUCT_END()

// Used for counting values within an index IndexedDB.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_IndexCount_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The index the index belongs to.
  IPC_STRUCT_MEMBER(int32, ipc_index_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int, ipc_transaction_id)
IPC_STRUCT_END()

// Used to set a value in an object store.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_ObjectStorePut_Params)
  // The object store's id.
  IPC_STRUCT_MEMBER(int32, ipc_object_store_id)
  // The id any response should contain.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The value to set.
  IPC_STRUCT_MEMBER(content::SerializedScriptValue, serialized_value)
  // The key to set it on (may not be "valid"/set in some cases).
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  // Whether this is an add or a put.
  IPC_STRUCT_MEMBER(WebKit::WebIDBObjectStore::PutMode, put_mode)
  // The names of the indexes used below.
  IPC_STRUCT_MEMBER(std::vector<int64>, index_ids)
  // The keys for each index, such that each inner vector corresponds
  // to each index named in index_names, respectively.
  IPC_STRUCT_MEMBER(std::vector<std::vector<content::IndexedDBKey> >,
                    index_keys)
  // The transaction it's associated with.
  IPC_STRUCT_MEMBER(int, ipc_transaction_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseSetIndexKeys_Params)
  // The IPC id of the database.
  IPC_STRUCT_MEMBER(int32, ipc_database_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int64, transaction_id)
  // The object store's id.
  IPC_STRUCT_MEMBER(int64, object_store_id)
  // The object store key that we're setting index keys for.
  IPC_STRUCT_MEMBER(content::IndexedDBKey, primary_key)
  // The indexes that we're setting keys on.
  IPC_STRUCT_MEMBER(std::vector<int64>, index_ids)
  // A list of index keys for each index.
  IPC_STRUCT_MEMBER(std::vector<std::vector<content::IndexedDBKey> >,
                    index_keys)
IPC_STRUCT_END()

// Used to create an index.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_ObjectStoreCreateIndex_Params)
  // The storage id of the index.
  IPC_STRUCT_MEMBER(int64, id)
  // The name of the index.
  IPC_STRUCT_MEMBER(string16, name)
  // The keyPath of the index.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, key_path)
  // Whether the index created has unique keys.
  IPC_STRUCT_MEMBER(bool, unique)
  // Whether the index created produces keys for each array entry.
  IPC_STRUCT_MEMBER(bool, multi_entry)
  // The transaction this is associated with.
  IPC_STRUCT_MEMBER(int32, ipc_transaction_id)
  // The object store the index belongs to.
  IPC_STRUCT_MEMBER(int32, ipc_object_store_id)
IPC_STRUCT_END()

// Used to open an IndexedDB cursor.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_ObjectStoreOpenCursor_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The direction of this cursor.
  IPC_STRUCT_MEMBER(WebKit::WebIDBCursor::Direction, direction)
  // The priority of this cursor.
  IPC_STRUCT_MEMBER(WebKit::WebIDBTransaction::TaskType, task_type)
  // The object store the cursor belongs to.
  IPC_STRUCT_MEMBER(int32, ipc_object_store_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int, ipc_transaction_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessIDBCursor_Params)
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  IPC_STRUCT_MEMBER(int32, ipc_cursor_id)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, primary_key)
  IPC_STRUCT_MEMBER(content::SerializedScriptValue, serialized_value)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessCursorContinue_Params)
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  IPC_STRUCT_MEMBER(int32, ipc_cursor_id)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, primary_key)
  IPC_STRUCT_MEMBER(content::SerializedScriptValue, serialized_value)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params)
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  IPC_STRUCT_MEMBER(int32, ipc_cursor_id)
  IPC_STRUCT_MEMBER(std::vector<content::IndexedDBKey>, keys)
  IPC_STRUCT_MEMBER(std::vector<content::IndexedDBKey>, primary_keys)
  IPC_STRUCT_MEMBER(std::vector<content::SerializedScriptValue>, values)
IPC_STRUCT_END()

// Used to count within an IndexedDB object store.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_ObjectStoreCount_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, ipc_thread_id)
  IPC_STRUCT_MEMBER(int32, ipc_response_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The object store the cursor belongs to.
  IPC_STRUCT_MEMBER(int32, ipc_object_store_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int, ipc_transaction_id)
IPC_STRUCT_END()

// Indexed DB messages sent from the browser to the renderer.

// The thread_id needs to be the first parameter in these messages.  In the IO
// thread on the renderer/client process, an IDB message filter assumes the
// thread_id is the first int.

// IDBCallback message handlers.
IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessIDBCursor,
                     IndexedDBMsg_CallbacksSuccessIDBCursor_Params)

IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessCursorContinue,
                     IndexedDBMsg_CallbacksSuccessCursorContinue_Params)

IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessCursorAdvance,
                     IndexedDBMsg_CallbacksSuccessCursorContinue_Params)

IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessCursorPrefetch,
                     IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params)

IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessIDBDatabase,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */,
                     int32 /* ipc_database_id */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessIndexedDBKey,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */,
                     content::IndexedDBKey /* indexed_db_key */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessSerializedScriptValue,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */,
                     content::SerializedScriptValue /* value */)
IPC_MESSAGE_CONTROL5(IndexedDBMsg_CallbacksSuccessSerializedScriptValueWithKey,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */,
                     content::SerializedScriptValue /* value */,
                     content::IndexedDBKey /* indexed_db_key */,
                     content::IndexedDBKeyPath /* indexed_db_keypath */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessInteger,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */,
                     int64 /* value */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksSuccessUndefined,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessStringList,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */,
                     std::vector<string16> /* dom_string_list */)
IPC_MESSAGE_CONTROL4(IndexedDBMsg_CallbacksError,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */,
                     int /* code */,
                     string16 /* message */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksBlocked,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksIntBlocked,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_response_id */,
                     int64 /* existing_version */)
IPC_MESSAGE_CONTROL5(IndexedDBMsg_CallbacksUpgradeNeeded,
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     int32, /* ipc_transaction_id */
                     int32, /* ipc_database_id */
                     int64) /* old_version */

// IDBTransactionCallback message handlers.
IPC_MESSAGE_CONTROL4(IndexedDBMsg_TransactionCallbacksAbort,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_transaction_id */,
                     int /* code */,
                     string16 /* message */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_TransactionCallbacksComplete,
                     int32 /* ipc_thread_id */,
                     int32 /* ipc_transaction_id */)

// IDBDatabaseCallback message handlers
IPC_MESSAGE_CONTROL2(IndexedDBMsg_DatabaseCallbacksForcedClose,
                     int32, /* ipc_thread_id */
                     int32) /* ipc_database_id */
IPC_MESSAGE_CONTROL3(IndexedDBMsg_DatabaseCallbacksVersionChange,
                     int32, /* ipc_thread_id */
                     int32, /* ipc_database_id */
                     string16) /* new_version */

IPC_MESSAGE_CONTROL4(IndexedDBMsg_DatabaseCallbacksIntVersionChange,
                     int32, /* ipc_thread_id */
                     int32, /* ipc_database_id */
                     int64, /* old_version */
                     int64) /* new_version */

// Indexed DB messages sent from the renderer to the browser.

// WebIDBCursor::advance() message.
IPC_MESSAGE_CONTROL4(IndexedDBHostMsg_CursorAdvance,
                     int32, /* ipc_cursor_id */
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     unsigned long) /* count */

// WebIDBCursor::continue() message.
IPC_MESSAGE_CONTROL4(IndexedDBHostMsg_CursorContinue,
                     int32, /* ipc_cursor_id */
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     content::IndexedDBKey) /* key */

// WebIDBCursor::prefetchContinue() message.
IPC_MESSAGE_CONTROL4(IndexedDBHostMsg_CursorPrefetch,
                     int32, /* ipc_cursor_id */
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     int32) /* n */

// WebIDBCursor::prefetchReset() message.
IPC_MESSAGE_CONTROL3(IndexedDBHostMsg_CursorPrefetchReset,
                     int32, /* ipc_cursor_id */
                     int32, /* used_prefetches */
                     int32)  /* used_prefetches */

// WebIDBCursor::delete() message.
IPC_MESSAGE_CONTROL3(IndexedDBHostMsg_CursorDelete,
                     int32, /* ipc_cursor_id */
                     int32, /* ipc_thread_id */
                     int32) /* ipc_response_id */

// WebIDBFactory::getDatabaseNames() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryGetDatabaseNames,
                     IndexedDBHostMsg_FactoryGetDatabaseNames_Params)

// WebIDBFactory::open() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryOpen,
                     IndexedDBHostMsg_FactoryOpen_Params)

// WebIDBFactory::open() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryOpenOld,
                     IndexedDBHostMsg_FactoryOpen_Params)

// WebIDBFactory::deleteDatabase() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryDeleteDatabase,
                     IndexedDBHostMsg_FactoryDeleteDatabase_Params)

// WebIDBDatabase::metadata() payload
IPC_STRUCT_BEGIN(IndexedDBIndexMetadata)
  IPC_STRUCT_MEMBER(int64, id)
  IPC_STRUCT_MEMBER(string16, name)
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, keyPath)
  IPC_STRUCT_MEMBER(bool, unique)
  IPC_STRUCT_MEMBER(bool, multiEntry)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBObjectStoreMetadata)
  IPC_STRUCT_MEMBER(int64, id)
  IPC_STRUCT_MEMBER(string16, name)
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, keyPath)
  IPC_STRUCT_MEMBER(bool, autoIncrement)
  IPC_STRUCT_MEMBER(int64, max_index_id)
  IPC_STRUCT_MEMBER(std::vector<IndexedDBIndexMetadata>, indexes)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBDatabaseMetadata)
  IPC_STRUCT_MEMBER(int64, id)
  IPC_STRUCT_MEMBER(string16, name)
  IPC_STRUCT_MEMBER(string16, version)
  IPC_STRUCT_MEMBER(int64, int_version)
  IPC_STRUCT_MEMBER(int64, max_object_store_id)
  IPC_STRUCT_MEMBER(std::vector<IndexedDBObjectStoreMetadata>, object_stores)
IPC_STRUCT_END()

// WebIDBDatabase::metadata() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_DatabaseMetadata,
                            int32, /* ipc_database_id */
                            IndexedDBDatabaseMetadata /* metadata */)

// WebIDBDatabase::createObjectStore() message.
IPC_SYNC_MESSAGE_CONTROL1_2(IndexedDBHostMsg_DatabaseCreateObjectStore,
                            IndexedDBHostMsg_DatabaseCreateObjectStore_Params,
                            int32, /* ipc_object_store_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBDatabase::deleteObjectStore() message.
IPC_SYNC_MESSAGE_CONTROL3_1(IndexedDBHostMsg_DatabaseDeleteObjectStore,
                            int32, /* ipc_database_id */
                            int64, /* object_store_id */
                            int32, /* ipc_transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBDatabase::createTransaction() message.
// TODO: make this message async.
IPC_SYNC_MESSAGE_CONTROL5_1(IndexedDBHostMsg_DatabaseCreateTransaction,
                            int32, /* ipc_thread_id */
                            int32, /* ipc_database_id */
                            int64, /* transaction_id */
                            std::vector<int64>, /* object_stores */
                            int32, /* mode */
                            int32) /* ipc_transaction_id */

// WebIDBDatabase::close() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseClose,
                     int32 /* ipc_database_id */)

// WebIDBDatabase::~WebIDBDatabase() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseDestroyed,
                     int32 /* ipc_database_id */)

// WebIDBDatabase::get() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseGet,
                     IndexedDBHostMsg_DatabaseGet_Params)

// WebIDBDatabase::put() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabasePut,
                     IndexedDBHostMsg_DatabasePut_Params)

// WebIDBDatabase::setIndexKeys() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseSetIndexKeys,
                     IndexedDBHostMsg_DatabaseSetIndexKeys_Params)

// WebIDBDatabase::setIndexesReady() message.
IPC_MESSAGE_CONTROL4(IndexedDBHostMsg_DatabaseSetIndexesReady,
                     int32, /* ipc_database_id */
                     int64, /* transaction_id */
                     int64, /* object_store_id */
                     std::vector<int64>) /* index_ids */

// WebIDBDatabase::openCursor() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseOpenCursor,
                     IndexedDBHostMsg_DatabaseOpenCursor_Params)

// WebIDBDatabase::count() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseCount,
                     IndexedDBHostMsg_DatabaseCount_Params)

// WebIDBDatabase::deleteRange() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseDeleteRange,
                     IndexedDBHostMsg_DatabaseDeleteRange_Params)

// WebIDBDatabase::clear() message.
IPC_MESSAGE_CONTROL5(IndexedDBHostMsg_DatabaseClear,
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     int32, /* ipc_database_id */
                     int64, /* transaction_id */
                     int64) /* object_store_id */

// WebIDBIndex::openObjectCursor() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_IndexOpenObjectCursor,
                     IndexedDBHostMsg_IndexOpenCursor_Params)

// WebIDBIndex::openKeyCursor() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_IndexOpenKeyCursor,
                     IndexedDBHostMsg_IndexOpenCursor_Params)

// WebIDBIndex::count() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_IndexCount,
                     IndexedDBHostMsg_IndexCount_Params)

// WebIDBIndex::getObject() message.
IPC_MESSAGE_CONTROL5(IndexedDBHostMsg_IndexGetObject,
                     int32, /* ipc_index_id */
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     content::IndexedDBKeyRange, /* key */
                     int32) /* ipc_transaction_id */

// WebIDBIndex::getKey() message.
IPC_MESSAGE_CONTROL5(IndexedDBHostMsg_IndexGetKey,
                     int32, /* ipc_index_id */
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     content::IndexedDBKeyRange, /* key */
                     int32) /* ipc_transaction_id */

// WebIDBIndex::~WebIDBIndex() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_IndexDestroyed,
                     int32) /* ipc_index_id */

// WebIDBObjectStore::get() message.
IPC_MESSAGE_CONTROL5(IndexedDBHostMsg_ObjectStoreGet,
                     int32, /* ipc_object_store_id */
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     content::IndexedDBKeyRange, /* key_range */
                     int32) /* ipc_transaction_id */

// WebIDBObjectStore::put() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_ObjectStorePut,
                     IndexedDBHostMsg_ObjectStorePut_Params)

// WebIDBObjectStore::setIndexKeys() message.
IPC_MESSAGE_CONTROL5(IndexedDBHostMsg_ObjectStoreSetIndexKeys,
                     int32, /* ipc_object_store_id */
                     content::IndexedDBKey, /* primary_key */
                     std::vector<int64>, /* index_ids */
                     std::vector<std::vector<content::IndexedDBKey> >,
                     /* index_keys */
                     int32 /* ipc_transaction_id */)

// WebIDBObjectStore::setIndexesReady() message.
IPC_MESSAGE_CONTROL3(IndexedDBHostMsg_ObjectStoreSetIndexesReady,
                     int32, /* ipc_object_store_id */
                     std::vector<int64>, /* index_names */
                     int32 /* ipc_transaction_id */)

// WebIDBObjectStore::delete() message.
IPC_MESSAGE_CONTROL5(IndexedDBHostMsg_ObjectStoreDelete,
                     int32, /* ipc_object_store_id */
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     content::IndexedDBKeyRange, /* key_range */
                     int32) /* ipc_transaction_id */

// WebIDBObjectStore::clear() message.
IPC_MESSAGE_CONTROL4(IndexedDBHostMsg_ObjectStoreClear,
                     int32, /* ipc_object_store_id */
                     int32, /* ipc_thread_id */
                     int32, /* ipc_response_id */
                     int32) /* ipc_transaction_id */

// WebIDBObjectStore::createIndex() message.
IPC_SYNC_MESSAGE_CONTROL1_2(IndexedDBHostMsg_ObjectStoreCreateIndex,
                            IndexedDBHostMsg_ObjectStoreCreateIndex_Params,
                            int32, /* ipc_index_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::index() message.
IPC_SYNC_MESSAGE_CONTROL2_1(IndexedDBHostMsg_ObjectStoreIndex,
                            int32, /* ipc_object_store_id */
                            int64, /* name */
                            int32) /* ipc_index_id */

// WebIDBObjectStore::deleteIndex() message.
IPC_SYNC_MESSAGE_CONTROL3_1(IndexedDBHostMsg_ObjectStoreDeleteIndex,
                            int32, /* ipc_object_store_id */
                            int64, /* object_store_id */
                            int32, /* ipc_transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::openCursor() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_ObjectStoreOpenCursor,
                     IndexedDBHostMsg_ObjectStoreOpenCursor_Params)

// WebIDBObjectStore::count() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_ObjectStoreCount,
                     IndexedDBHostMsg_ObjectStoreCount_Params)

// WebIDBObjectStore::~WebIDBObjectStore() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_ObjectStoreDestroyed,
                     int32 /* ipc_object_store_id */)

// WebIDBDatabase::~WebIDBCursor() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_CursorDestroyed,
                     int32 /* ipc_cursor_id */)

// IDBTransaction::ObjectStore message.
IPC_SYNC_MESSAGE_CONTROL2_2(IndexedDBHostMsg_TransactionObjectStore,
                            int32, /* ipc_transaction_id */
                            int64, /* id */
                            int32, /* ipc_object_store_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBTransaction::commit() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_TransactionCommit,
                     int32 /* ipc_transaction_id */)

// WebIDBTransaction::abort() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_TransactionAbort,
                     int32 /* ipc_transaction_id */)

// IDBTransaction::DidCompleteTaskEvents() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_TransactionDidCompleteTaskEvents,
                     int32 /* ipc_transaction_id */)

// WebIDBTransaction::~WebIDBTransaction() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_TransactionDestroyed,
                     int32 /* ipc_transaction_id */)
