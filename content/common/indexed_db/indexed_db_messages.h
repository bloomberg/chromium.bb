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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBObjectStore.h"

#define IPC_MESSAGE_START IndexedDBMsgStart

// Argument structures used in messages

IPC_ENUM_TRAITS(WebKit::WebIDBObjectStore::PutMode)

// Used to enumerate indexed databases.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_FactoryGetDatabaseNames_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  // The origin doing the initiating.
  IPC_STRUCT_MEMBER(string16, origin)
IPC_STRUCT_END()

// Used to open an indexed database.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_FactoryOpen_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  // The origin doing the initiating.
  IPC_STRUCT_MEMBER(string16, origin)
  // The name of the database.
  IPC_STRUCT_MEMBER(string16, name)
IPC_STRUCT_END()

// Used to delete an indexed database.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_FactoryDeleteDatabase_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  // The origin doing the initiating.
  IPC_STRUCT_MEMBER(string16, origin)
  // The name of the database.
  IPC_STRUCT_MEMBER(string16, name)
IPC_STRUCT_END()

// Used to create an object store.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_DatabaseCreateObjectStore_Params)
  // The name of the object store.
  IPC_STRUCT_MEMBER(string16, name)
  // The keyPath of the object store.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, key_path)
  // Whether the object store created should have a key generator.
  IPC_STRUCT_MEMBER(bool, auto_increment)
  // The transaction this is associated with.
  IPC_STRUCT_MEMBER(int32, transaction_id)
  // The database the object store belongs to.
  IPC_STRUCT_MEMBER(int32, idb_database_id)
IPC_STRUCT_END()

// Used to open both cursors and object cursors in IndexedDB.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_IndexOpenCursor_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The direction of this cursor.
  IPC_STRUCT_MEMBER(int32, direction)
  // The index the index belongs to.
  IPC_STRUCT_MEMBER(int32, idb_index_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int, transaction_id)
IPC_STRUCT_END()

// Used for counting values within an index IndexedDB.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_IndexCount_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The index the index belongs to.
  IPC_STRUCT_MEMBER(int32, idb_index_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int, transaction_id)
IPC_STRUCT_END()

// Used to set a value in an object store.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_ObjectStorePut_Params)
  // The object store's id.
  IPC_STRUCT_MEMBER(int32, idb_object_store_id)
  // The id any response should contain.
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  // The value to set.
  IPC_STRUCT_MEMBER(content::SerializedScriptValue, serialized_value)
  // The key to set it on (may not be "valid"/set in some cases).
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  // Whether this is an add or a put.
  IPC_STRUCT_MEMBER(WebKit::WebIDBObjectStore::PutMode, put_mode)
  // The transaction it's associated with.
  IPC_STRUCT_MEMBER(int, transaction_id)
IPC_STRUCT_END()

// Used to create an index.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_ObjectStoreCreateIndex_Params)
  // The name of the index.
  IPC_STRUCT_MEMBER(string16, name)
  // The keyPath of the index.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyPath, key_path)
  // Whether the index created has unique keys.
  IPC_STRUCT_MEMBER(bool, unique)
  // Whether the index created produces keys for each array entry.
  IPC_STRUCT_MEMBER(bool, multi_entry)
  // The transaction this is associated with.
  IPC_STRUCT_MEMBER(int32, transaction_id)
  // The object store the index belongs to.
  IPC_STRUCT_MEMBER(int32, idb_object_store_id)
IPC_STRUCT_END()

// Used to open an IndexedDB cursor.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_ObjectStoreOpenCursor_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The direction of this cursor.
  IPC_STRUCT_MEMBER(int32, direction)
  // The object store the cursor belongs to.
  IPC_STRUCT_MEMBER(int32, idb_object_store_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int, transaction_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessIDBCursor_Params)
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  IPC_STRUCT_MEMBER(int32, cursor_id)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, primary_key)
  IPC_STRUCT_MEMBER(content::SerializedScriptValue, serialized_value)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessCursorContinue_Params)
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  IPC_STRUCT_MEMBER(int32, cursor_id)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, key)
  IPC_STRUCT_MEMBER(content::IndexedDBKey, primary_key)
  IPC_STRUCT_MEMBER(content::SerializedScriptValue, serialized_value)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params)
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  IPC_STRUCT_MEMBER(int32, cursor_id)
  IPC_STRUCT_MEMBER(std::vector<content::IndexedDBKey>, keys)
  IPC_STRUCT_MEMBER(std::vector<content::IndexedDBKey>, primary_keys)
  IPC_STRUCT_MEMBER(std::vector<content::SerializedScriptValue>, values)
IPC_STRUCT_END()


// Used to count within an IndexedDB object store.
IPC_STRUCT_BEGIN(IndexedDBHostMsg_ObjectStoreCount_Params)
  // The response should have these ids.
  IPC_STRUCT_MEMBER(int32, thread_id)
  IPC_STRUCT_MEMBER(int32, response_id)
  // The serialized key range.
  IPC_STRUCT_MEMBER(content::IndexedDBKeyRange, key_range)
  // The object store the cursor belongs to.
  IPC_STRUCT_MEMBER(int32, idb_object_store_id)
  // The transaction this request belongs to.
  IPC_STRUCT_MEMBER(int, transaction_id)
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
                     int32 /* thread_id */,
                     int32 /* response_id */,
                     int32 /* idb_database_id */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessIndexedDBKey,
                     int32 /* thread_id */,
                     int32 /* response_id */,
                     content::IndexedDBKey /* indexed_db_key */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessIDBTransaction,
                     int32 /* thread_id */,
                     int32 /* response_id */,
                     int32 /* idb_transaction_id */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessSerializedScriptValue,
                     int32 /* thread_id */,
                     int32 /* response_id */,
                     content::SerializedScriptValue /* value */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksSuccessStringList,
                     int32 /* thread_id */,
                     int32 /* response_id */,
                     std::vector<string16> /* dom_string_list */)
IPC_MESSAGE_CONTROL4(IndexedDBMsg_CallbacksError,
                     int32 /* thread_id */,
                     int32 /* response_id */,
                     int /* code */,
                     string16 /* message */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksBlocked,
                     int32 /* thread_id */,
                     int32 /* response_id */)

// IDBTransactionCallback message handlers.
IPC_MESSAGE_CONTROL2(IndexedDBMsg_TransactionCallbacksAbort,
                     int32 /* thread_id */,
                     int32 /* transaction_id */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_TransactionCallbacksComplete,
                     int32 /* thread_id */,
                     int32 /* transaction_id */)

IPC_MESSAGE_CONTROL3(IndexedDBMsg_DatabaseCallbacksVersionChange,
                     int32, /* thread_id */
                     int32, /* database_id */
                     string16) /* new_version */

// Indexed DB messages sent from the renderer to the browser.

// WebIDBCursor::direction() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_CursorDirection,
                            int32, /* idb_cursor_id */
                            int32 /* direction */)

// WebIDBCursor::update() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_CursorUpdate,
                     int32, /* idb_cursor_id */
                     int32, /* thread_id */
                     int32, /* response_id */
                     content::SerializedScriptValue, /* value */
                     WebKit::WebExceptionCode /* ec */)

// WebIDBCursor::advance() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_CursorAdvance,
                     int32, /* idb_cursor_id */
                     int32, /* thread_id */
                     int32, /* response_id */
                     unsigned long, /* count */
                     WebKit::WebExceptionCode /* ec */)

// WebIDBCursor::continue() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_CursorContinue,
                     int32, /* idb_cursor_id */
                     int32, /* thread_id */
                     int32, /* response_id */
                     content::IndexedDBKey, /* key */
                     WebKit::WebExceptionCode /* ec */)

// WebIDBCursor::prefetchContinue() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_CursorPrefetch,
                     int32, /* idb_cursor_id */
                     int32, /* thread_id */
                     int32, /* response_id */
                     int32, /* n */
                     WebKit::WebExceptionCode /* ec */)

// WebIDBCursor::prefetchReset() message.
IPC_SYNC_MESSAGE_CONTROL3_0(IndexedDBHostMsg_CursorPrefetchReset,
                     int32, /* idb_cursor_id */
                     int32, /* used_prefetches */
                     int32  /* used_prefetches */)

// WebIDBCursor::remove() message.
IPC_SYNC_MESSAGE_CONTROL3_1(IndexedDBHostMsg_CursorDelete,
                     int32, /* idb_cursor_id */
                     int32, /* thread_id */
                     int32, /* response_id */
                     WebKit::WebExceptionCode /* ec */)

// WebIDBFactory::getDatabaseNames() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryGetDatabaseNames,
                     IndexedDBHostMsg_FactoryGetDatabaseNames_Params)

// WebIDBFactory::open() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryOpen,
                     IndexedDBHostMsg_FactoryOpen_Params)

// WebIDBFactory::deleteDatabase() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryDeleteDatabase,
                     IndexedDBHostMsg_FactoryDeleteDatabase_Params)

// WebIDBDatabase::name() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_DatabaseName,
                            int32, /* idb_database_id */
                            string16 /* name */)

// WebIDBDatabase::version() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_DatabaseVersion,
                            int32, /* idb_database_id */
                            string16 /* version */)

// WebIDBDatabase::objectStoreNames() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_DatabaseObjectStoreNames,
                            int32, /* idb_database_id */
                            std::vector<string16> /* objectStoreNames */)

// WebIDBDatabase::createObjectStore() message.
IPC_SYNC_MESSAGE_CONTROL1_2(IndexedDBHostMsg_DatabaseCreateObjectStore,
                            IndexedDBHostMsg_DatabaseCreateObjectStore_Params,
                            int32, /* object_store_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBDatabase::removeObjectStore() message.
IPC_SYNC_MESSAGE_CONTROL3_1(IndexedDBHostMsg_DatabaseDeleteObjectStore,
                            int32, /* idb_database_id */
                            string16, /* name */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBDatabase::setVersion() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_DatabaseSetVersion,
                            int32, /* idb_database_id */
                            int32, /* thread_id */
                            int32, /* response_id */
                            string16, /* version */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBDatabase::transaction() message.
// TODO: make this message async. Have the renderer create a
// temporary ID and keep a map in the browser process of real
// IDs to temporary IDs. We can then update the transaction
// to its real ID asynchronously.
IPC_SYNC_MESSAGE_CONTROL4_2(IndexedDBHostMsg_DatabaseTransaction,
                            int32, /* thread_id */
                            int32, /* idb_database_id */
                            std::vector<string16>, /* object_stores */
                            int32, /* mode */
                            int32, /* idb_transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBDatabase::open() message.
IPC_MESSAGE_CONTROL3(IndexedDBHostMsg_DatabaseOpen,
                     int32, /* idb_database_id */
                     int32 /* thread_id */,
                     int32 /* response_id */)

// WebIDBDatabase::close() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseClose,
                     int32 /* idb_database_id */)

// WebIDBDatabase::~WebIDBDatabase() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_DatabaseDestroyed,
                     int32 /* idb_database_id */)

// WebIDBIndex::name() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexName,
                            int32, /* idb_index_id */
                            string16 /* name */)

// WebIDBIndex::storeName() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexStoreName,
                            int32, /* idb_index_id */
                            string16 /* store_name */)

// WebIDBIndex::keyPath() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexKeyPath,
                            int32, /* idb_index_id */
                            content::IndexedDBKeyPath /* key_path */)

// WebIDBIndex::unique() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexUnique,
                            int32, /* idb_index_id */
                            bool /* unique */)

// WebIDBIndex::multiEntry() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexMultiEntry,
                            int32, /* idb_index_id */
                            bool /* multi_entry */)

// WebIDBIndex::openObjectCursor() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexOpenObjectCursor,
                            IndexedDBHostMsg_IndexOpenCursor_Params,
                            WebKit::WebExceptionCode /* ec */)

// WebIDBIndex::openKeyCursor() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexOpenKeyCursor,
                            IndexedDBHostMsg_IndexOpenCursor_Params,
                            WebKit::WebExceptionCode /* ec */)

// WebIDBIndex::count() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexCount,
                            IndexedDBHostMsg_IndexCount_Params,
                            WebKit::WebExceptionCode /* ec */)

// WebIDBIndex::getObject() message.
IPC_SYNC_MESSAGE_CONTROL5_1(IndexedDBHostMsg_IndexGetObject,
                            int32, /* idb_index_id */
                            int32, /* thread_id */
                            int32, /* response_id */
                            content::IndexedDBKeyRange, /* key */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBIndex::getKey() message.
IPC_SYNC_MESSAGE_CONTROL5_1(IndexedDBHostMsg_IndexGetKey,
                            int32, /* idb_index_id */
                            int32, /* thread_id */
                            int32, /* response_id */
                            content::IndexedDBKeyRange, /* key */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBIndex::~WebIDBIndex() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_IndexDestroyed,
                     int32 /* idb_index_id */)

// WebIDBObjectStore::name() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_ObjectStoreName,
                            int32, /* idb_object_store_id */
                            string16 /* name */)

// WebIDBObjectStore::keyPath() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_ObjectStoreKeyPath,
                            int32, /* idb_object_store_id */
                            content::IndexedDBKeyPath /* keyPath */)

// WebIDBObjectStore::indexNames() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_ObjectStoreIndexNames,
                            int32, /* idb_object_store_id */
                            std::vector<string16> /* index_names */)

// WebIDBObjectStore::get() message.
IPC_SYNC_MESSAGE_CONTROL5_1(IndexedDBHostMsg_ObjectStoreGet,
                            int32, /* idb_object_store_id */
                            int32, /* thread_id */
                            int32, /* response_id */
                            content::IndexedDBKeyRange, /* key_range */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::put() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_ObjectStorePut,
                            IndexedDBHostMsg_ObjectStorePut_Params,
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::delete(key) message.
IPC_SYNC_MESSAGE_CONTROL5_1(IndexedDBHostMsg_ObjectStoreDelete,
                            int32, /* idb_object_store_id */
                            int32, /* thread_id */
                            int32, /* response_id */
                            content::IndexedDBKey, /* key */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::delete(keyRange) message.
IPC_SYNC_MESSAGE_CONTROL5_1(IndexedDBHostMsg_ObjectStoreDeleteRange,
                            int32, /* idb_object_store_id */
                            int32, /* thread_id */
                            int32, /* response_id */
                            content::IndexedDBKeyRange, /* key_range */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::clear() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_ObjectStoreClear,
                            int32, /* idb_object_store_id */
                            int32, /* thread_id */
                            int32, /* response_id */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::createIndex() message.
IPC_SYNC_MESSAGE_CONTROL1_2(IndexedDBHostMsg_ObjectStoreCreateIndex,
                            IndexedDBHostMsg_ObjectStoreCreateIndex_Params,
                            int32, /* index_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::index() message.
IPC_SYNC_MESSAGE_CONTROL2_2(IndexedDBHostMsg_ObjectStoreIndex,
                            int32, /* idb_object_store_id */
                            string16, /* name */
                            int32, /* idb_index_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::deleteIndex() message.
IPC_SYNC_MESSAGE_CONTROL3_1(IndexedDBHostMsg_ObjectStoreDeleteIndex,
                            int32, /* idb_object_store_id */
                            string16, /* name */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::openCursor() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_ObjectStoreOpenCursor,
                            IndexedDBHostMsg_ObjectStoreOpenCursor_Params,
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::count() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_ObjectStoreCount,
                            IndexedDBHostMsg_ObjectStoreCount_Params,
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::~WebIDBObjectStore() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_ObjectStoreDestroyed,
                     int32 /* idb_object_store_id */)

// WebIDBDatabase::~WebIDBCursor() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_CursorDestroyed,
                     int32 /* idb_cursor_id */)

// IDBTransaction::ObjectStore message.
IPC_SYNC_MESSAGE_CONTROL2_2(IndexedDBHostMsg_TransactionObjectStore,
                            int32, /* transaction_id */
                            string16, /* name */
                            int32, /* object_store_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBTransaction::mode() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_TransactionMode,
                            int32, /* idb_transaction_id */
                            int /* mode */)

// WebIDBTransaction::abort() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_TransactionAbort,
                     int32 /* idb_transaction_id */)

// IDBTransaction::DidCompleteTaskEvents() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_TransactionDidCompleteTaskEvents,
                     int32 /* idb_transaction_id */)

// WebIDBTransaction::~WebIDBTransaction() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_TransactionDestroyed,
                     int32 /* idb_transaction_id */)
