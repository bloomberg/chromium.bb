// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INDEXED_DB_MESSAGES_H_
#define CHROME_COMMON_INDEXED_DB_MESSAGES_H_
#pragma once

#include "chrome/common/indexed_db_key.h"
#include "chrome/common/indexed_db_param_traits.h"
#include "chrome/common/serialized_script_value.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExceptionCode.h"

#define IPC_MESSAGE_START IndexedDBMsgStart

// Used to open an indexed database.
struct IndexedDBHostMsg_FactoryOpen_Params {
  IndexedDBHostMsg_FactoryOpen_Params();
  ~IndexedDBHostMsg_FactoryOpen_Params();

  // The routing ID of the view initiating the open.
  int32 routing_id;

  // The response should have this id.
  int32 response_id;

  // The origin doing the initiating.
  string16 origin;

  // The name of the database.
  string16 name;

  // The maximum size of the database.
  uint64 maximum_size;
};

// Used to create an object store.
struct IndexedDBHostMsg_DatabaseCreateObjectStore_Params {
  IndexedDBHostMsg_DatabaseCreateObjectStore_Params();
  ~IndexedDBHostMsg_DatabaseCreateObjectStore_Params();

  // The name of the object store.
  string16 name;

  // The keyPath of the object store.
  NullableString16 key_path;

  // Whether the object store created should have a key generator.
  bool auto_increment;

  // The transaction this is associated with.
  int32 transaction_id;

  // The database the object store belongs to.
  int32 idb_database_id;
};

// Used to open both cursors and object cursors in IndexedDB.
struct IndexedDBHostMsg_IndexOpenCursor_Params {
  IndexedDBHostMsg_IndexOpenCursor_Params();
  ~IndexedDBHostMsg_IndexOpenCursor_Params();

  // The response should have this id.
  int32 response_id;

  // The serialized lower key.
  IndexedDBKey lower_key;

  // The serialized upper key.
  IndexedDBKey upper_key;

  // Is the lower bound open?
  bool lower_open;

  // Is the upper bound open?
  bool upper_open;

  // The direction of this cursor.
  int32 direction;

  // The index the index belongs to.
  int32 idb_index_id;

  // The transaction this request belongs to.
  int transaction_id;
};

// Used to set a value in an object store.
struct IndexedDBHostMsg_ObjectStorePut_Params {
  IndexedDBHostMsg_ObjectStorePut_Params();
  ~IndexedDBHostMsg_ObjectStorePut_Params();

  // The object store's id.
  int32 idb_object_store_id;

  // The id any response should contain.
  int32 response_id;

  // The value to set.
  SerializedScriptValue serialized_value;

  // The key to set it on (may not be "valid"/set in some cases).
  IndexedDBKey key;

  // If it already exists, don't update (just return an error).
  bool add_only;

  // The transaction it's associated with.
  int transaction_id;
};

// Used to create an index.
struct IndexedDBHostMsg_ObjectStoreCreateIndex_Params {
  IndexedDBHostMsg_ObjectStoreCreateIndex_Params();
  ~IndexedDBHostMsg_ObjectStoreCreateIndex_Params();

  // The name of the index.
  string16 name;

  // The keyPath of the index.
  NullableString16 key_path;

  // Whether the index created has unique keys.
  bool unique;

  // The transaction this is associated with.
  int32 transaction_id;

  // The object store the index belongs to.
  int32 idb_object_store_id;
};

// Used to open an IndexedDB cursor.
struct IndexedDBHostMsg_ObjectStoreOpenCursor_Params {
  IndexedDBHostMsg_ObjectStoreOpenCursor_Params();
  ~IndexedDBHostMsg_ObjectStoreOpenCursor_Params();

  // The response should have this id.
  int32 response_id;

  // The serialized lower key.
  IndexedDBKey lower_key;

  // The serialized upper key.
  IndexedDBKey upper_key;

  // Is the lower bound open?
  bool lower_open;

  // Is the upper bound open?
  bool upper_open;

  // The direction of this cursor.
  int32 direction;

  // The object store the cursor belongs to.
  int32 idb_object_store_id;

  // The transaction this request belongs to.
  int transaction_id;
};

namespace IPC {
template <>
struct ParamTraits<IndexedDBHostMsg_FactoryOpen_Params> {
  typedef IndexedDBHostMsg_FactoryOpen_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<IndexedDBHostMsg_DatabaseCreateObjectStore_Params> {
  typedef IndexedDBHostMsg_DatabaseCreateObjectStore_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<IndexedDBHostMsg_IndexOpenCursor_Params> {
  typedef IndexedDBHostMsg_IndexOpenCursor_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<IndexedDBHostMsg_ObjectStorePut_Params> {
  typedef IndexedDBHostMsg_ObjectStorePut_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<IndexedDBHostMsg_ObjectStoreCreateIndex_Params> {
  typedef IndexedDBHostMsg_ObjectStoreCreateIndex_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<IndexedDBHostMsg_ObjectStoreOpenCursor_Params> {
  typedef IndexedDBHostMsg_ObjectStoreOpenCursor_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

// Indexed DB messages sent from the browser to the renderer.

// IDBCallback message handlers.
IPC_MESSAGE_CONTROL1(IndexedDBMsg_CallbacksSuccessNull,
                     int32 /* response_id */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksSuccessIDBCursor,
                     int32 /* response_id */,
                     int32 /* cursor_id */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksSuccessIDBDatabase,
                     int32 /* response_id */,
                     int32 /* idb_database_id */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksSuccessIndexedDBKey,
                     int32 /* response_id */,
                     IndexedDBKey /* indexed_db_key */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksSuccessIDBIndex,
                     int32 /* response_id */,
                     int32 /* idb_index_id */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksSuccessIDBObjectStore,
                     int32 /* response_id */,
                     int32 /* idb_object_store_id */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksSuccessIDBTransaction,
                     int32 /* response_id */,
                     int32 /* idb_transaction_id */)
IPC_MESSAGE_CONTROL2(IndexedDBMsg_CallbacksSuccessSerializedScriptValue,
                     int32 /* response_id */,
                     SerializedScriptValue /* serialized_script_value */)
IPC_MESSAGE_CONTROL3(IndexedDBMsg_CallbacksError,
                     int32 /* response_id */,
                     int /* code */,
                     string16 /* message */)

// IDBTransactionCallback message handlers.
IPC_MESSAGE_CONTROL1(IndexedDBMsg_TransactionCallbacksAbort,
                     int32 /* transaction_id */)
IPC_MESSAGE_CONTROL1(IndexedDBMsg_TransactionCallbacksComplete,
                     int32 /* transaction_id */)
IPC_MESSAGE_CONTROL1(IndexedDBMsg_TransactionCallbacksTimeout,
                     int32 /* transaction_id */)


// Indexed DB messages sent from the renderer to the browser.

// WebIDBCursor::direction() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_CursorDirection,
                            int32, /* idb_cursor_id */
                            int32 /* direction */)

// WebIDBCursor::key() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_CursorKey,
                            int32, /* idb_cursor_id */
                            IndexedDBKey /* key */)

// WebIDBCursor::value() message.
IPC_SYNC_MESSAGE_CONTROL1_2(IndexedDBHostMsg_CursorValue,
                            int32, /* idb_cursor_id */
                            SerializedScriptValue, /* script_value */
                            IndexedDBKey /* key */)

// WebIDBCursor::update() message.
IPC_SYNC_MESSAGE_CONTROL3_1(IndexedDBHostMsg_CursorUpdate,
                     int32, /* idb_cursor_id */
                     int32, /* response_id */
                     SerializedScriptValue, /* value */
                     WebKit::WebExceptionCode /* ec */)

// WebIDBCursor::continue() message.
IPC_SYNC_MESSAGE_CONTROL3_1(IndexedDBHostMsg_CursorContinue,
                     int32, /* idb_cursor_id */
                     int32, /* response_id */
                     IndexedDBKey, /* key */
                     WebKit::WebExceptionCode /* ec */)

// WebIDBCursor::remove() message.
IPC_SYNC_MESSAGE_CONTROL2_1(IndexedDBHostMsg_CursorDelete,
                     int32, /* idb_cursor_id */
                     int32, /* response_id */
                     WebKit::WebExceptionCode /* ec */)

// WebIDBFactory::open() message.
IPC_MESSAGE_CONTROL1(IndexedDBHostMsg_FactoryOpen,
                     IndexedDBHostMsg_FactoryOpen_Params)

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
IPC_SYNC_MESSAGE_CONTROL3_1(IndexedDBHostMsg_DatabaseSetVersion,
                            int32, /* idb_database_id */
                            int32, /* response_id */
                            string16, /* version */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBDatabase::transaction() message.
// TODO: make this message async. Have the renderer create a
// temporary ID and keep a map in the browser process of real
// IDs to temporary IDs. We can then update the transaction
// to its real ID asynchronously.
IPC_SYNC_MESSAGE_CONTROL4_2(IndexedDBHostMsg_DatabaseTransaction,
                            int32, /* idb_database_id */
                            std::vector<string16>, /* object_stores */
                            int32, /* mode */
                            int32, /* timeout */
                            int32, /* idb_transaction_id */
                            WebKit::WebExceptionCode /* ec */)

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
                            NullableString16 /* key_path */)

// WebIDBIndex::unique() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexUnique,
                            int32, /* idb_unique_id */
                            bool /* unique */)

// WebIDBIndex::openObjectCursor() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexOpenObjectCursor,
                            IndexedDBHostMsg_IndexOpenCursor_Params,
                            WebKit::WebExceptionCode /* ec */)

// WebIDBIndex::openKeyCursor() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_IndexOpenKeyCursor,
                            IndexedDBHostMsg_IndexOpenCursor_Params,
                            WebKit::WebExceptionCode /* ec */)

// WebIDBIndex::getObject() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_IndexGetObject,
                            int32, /* idb_index_id */
                            int32, /* response_id */
                            IndexedDBKey, /* key */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBIndex::getKey() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_IndexGetKey,
                            int32, /* idb_index_id */
                            int32, /* response_id */
                            IndexedDBKey, /* key */
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
                            NullableString16 /* keyPath */)

// WebIDBObjectStore::indexNames() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_ObjectStoreIndexNames,
                            int32, /* idb_object_store_id */
                            std::vector<string16> /* index_names */)

// WebIDBObjectStore::get() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_ObjectStoreGet,
                            int32, /* idb_object_store_id */
                            int32, /* response_id */
                            IndexedDBKey, /* key */
                            int32, /* transaction_id */
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::put() message.
IPC_SYNC_MESSAGE_CONTROL1_1(IndexedDBHostMsg_ObjectStorePut,
                            IndexedDBHostMsg_ObjectStorePut_Params,
                            WebKit::WebExceptionCode /* ec */)

// WebIDBObjectStore::delete() message.
IPC_SYNC_MESSAGE_CONTROL4_1(IndexedDBHostMsg_ObjectStoreDelete,
                            int32, /* idb_object_store_id */
                            int32, /* response_id */
                            IndexedDBKey, /* key */
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

#endif  // CHROME_COMMON_INDEXED_DB_MESSAGES_H_
