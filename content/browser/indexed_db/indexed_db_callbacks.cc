// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callbacks.h"

#include <algorithm>

#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "webkit/browser/quota/quota_manager.h"

namespace content {

namespace {
const int32 kNoCursor = -1;
const int32 kNoDatabaseCallbacks = -1;
const int64 kNoTransaction = -1;
}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32 ipc_thread_id,
                                       int32 ipc_callbacks_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(kNoTransaction),
      ipc_database_id_(kNoDatabase),
      ipc_database_callbacks_id_(kNoDatabaseCallbacks),
      data_loss_(blink::WebIDBDataLossNone) {}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32 ipc_thread_id,
                                       int32 ipc_callbacks_id,
                                       int32 ipc_cursor_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(ipc_cursor_id),
      host_transaction_id_(kNoTransaction),
      ipc_database_id_(kNoDatabase),
      ipc_database_callbacks_id_(kNoDatabaseCallbacks),
      data_loss_(blink::WebIDBDataLossNone) {}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32 ipc_thread_id,
                                       int32 ipc_callbacks_id,
                                       int32 ipc_database_callbacks_id,
                                       int64 host_transaction_id,
                                       const GURL& origin_url)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(host_transaction_id),
      origin_url_(origin_url),
      ipc_database_id_(kNoDatabase),
      ipc_database_callbacks_id_(ipc_database_callbacks_id),
      data_loss_(blink::WebIDBDataLossNone) {}

IndexedDBCallbacks::~IndexedDBCallbacks() {}

void IndexedDBCallbacks::OnError(const IndexedDBDatabaseError& error) {
  DCHECK(dispatcher_host_.get());

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksError(
      ipc_thread_id_, ipc_callbacks_id_, error.code(), error.message()));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(const std::vector<base::string16>& value) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::vector<base::string16> list;
  for (unsigned i = 0; i < value.size(); ++i)
    list.push_back(value[i]);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessStringList(
      ipc_thread_id_, ipc_callbacks_id_, list));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnBlocked(int64 existing_version) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  // No transaction/db callbacks for DeleteDatabase.
  DCHECK_EQ(kNoTransaction == host_transaction_id_,
            kNoDatabaseCallbacks == ipc_database_callbacks_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksIntBlocked(
      ipc_thread_id_, ipc_callbacks_id_, existing_version));
}

void IndexedDBCallbacks::OnDataLoss(blink::WebIDBDataLoss data_loss,
                                    std::string data_loss_message) {
  DCHECK_NE(blink::WebIDBDataLossNone, data_loss);
  data_loss_ = data_loss;
  data_loss_message_ = data_loss_message;
}

void IndexedDBCallbacks::OnUpgradeNeeded(
    int64 old_version,
    scoped_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_NE(kNoDatabaseCallbacks, ipc_database_callbacks_id_);

  dispatcher_host_->RegisterTransactionId(host_transaction_id_, origin_url_);
  int32 ipc_database_id =
      dispatcher_host_->Add(connection.release(), ipc_thread_id_, origin_url_);
  if (ipc_database_id < 0)
    return;
  ipc_database_id_ = ipc_database_id;
  IndexedDBMsg_CallbacksUpgradeNeeded_Params params;
  params.ipc_thread_id = ipc_thread_id_;
  params.ipc_callbacks_id = ipc_callbacks_id_;
  params.ipc_database_id = ipc_database_id;
  params.ipc_database_callbacks_id = ipc_database_callbacks_id_;
  params.old_version = old_version;
  params.idb_metadata = IndexedDBDispatcherHost::ConvertMetadata(metadata);
  params.data_loss = data_loss_;
  params.data_loss_message = data_loss_message_;
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksUpgradeNeeded(params));
}

void IndexedDBCallbacks::OnSuccess(scoped_ptr<IndexedDBConnection> connection,
                                   const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_NE(ipc_database_id_ == kNoDatabase, !connection);
  DCHECK_NE(kNoDatabaseCallbacks, ipc_database_callbacks_id_);

  scoped_refptr<IndexedDBCallbacks> self(this);

  int32 ipc_object_id = kNoDatabase;
  // Only register if the connection was not previously sent in OnUpgradeNeeded.
  if (ipc_database_id_ == kNoDatabase) {
    ipc_object_id = dispatcher_host_->Add(
        connection.release(), ipc_thread_id_, origin_url_);
  }

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIDBDatabase(
      ipc_thread_id_,
      ipc_callbacks_id_,
      ipc_database_callbacks_id_,
      ipc_object_id,
      IndexedDBDispatcherHost::ConvertMetadata(metadata)));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(scoped_refptr<IndexedDBCursor> cursor,
                                   const IndexedDBKey& key,
                                   const IndexedDBKey& primary_key,
                                   IndexedDBValue* value) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  int32 ipc_object_id = dispatcher_host_->Add(cursor.get());
  IndexedDBMsg_CallbacksSuccessIDBCursor_Params params;
  params.ipc_thread_id = ipc_thread_id_;
  params.ipc_callbacks_id = ipc_callbacks_id_;
  params.ipc_cursor_id = ipc_object_id;
  params.key = key;
  params.primary_key = primary_key;
  if (value && !value->empty())
    std::swap(params.value, value->bits);
  // TODO(alecflett): Avoid a copy here: the whole params object is
  // being copied into the message.
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIDBCursor(params));

  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(const IndexedDBKey& key,
                                   const IndexedDBKey& primary_key,
                                   IndexedDBValue* value) {
  DCHECK(dispatcher_host_.get());

  DCHECK_NE(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  IndexedDBCursor* idb_cursor =
      dispatcher_host_->GetCursorFromId(ipc_cursor_id_);

  DCHECK(idb_cursor);
  if (!idb_cursor)
    return;
  IndexedDBMsg_CallbacksSuccessCursorContinue_Params params;
  params.ipc_thread_id = ipc_thread_id_;
  params.ipc_callbacks_id = ipc_callbacks_id_;
  params.ipc_cursor_id = ipc_cursor_id_;
  params.key = key;
  params.primary_key = primary_key;
  if (value && !value->empty())
    std::swap(params.value, value->bits);
  // TODO(alecflett): Avoid a copy here: the whole params object is
  // being copied into the message.
  dispatcher_host_->Send(
      new IndexedDBMsg_CallbacksSuccessCursorContinue(params));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccessWithPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    const std::vector<IndexedDBValue>& values) {
  DCHECK_EQ(keys.size(), primary_keys.size());
  DCHECK_EQ(keys.size(), values.size());

  DCHECK(dispatcher_host_.get());

  DCHECK_NE(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::vector<IndexedDBKey> msgKeys;
  std::vector<IndexedDBKey> msgPrimaryKeys;

  for (size_t i = 0; i < keys.size(); ++i) {
    msgKeys.push_back(keys[i]);
    msgPrimaryKeys.push_back(primary_keys[i]);
  }

  IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params params;
  params.ipc_thread_id = ipc_thread_id_;
  params.ipc_callbacks_id = ipc_callbacks_id_;
  params.ipc_cursor_id = ipc_cursor_id_;
  params.keys = msgKeys;
  params.primary_keys = msgPrimaryKeys;
  std::vector<IndexedDBValue>::const_iterator iter;
  for (iter = values.begin(); iter != values.end(); ++iter)
    params.values.push_back(iter->bits);
  dispatcher_host_->Send(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch(params));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(IndexedDBValue* value,
                                   const IndexedDBKey& key,
                                   const IndexedDBKeyPath& key_path) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  IndexedDBMsg_CallbacksSuccessValueWithKey_Params params;
  params.ipc_thread_id = ipc_thread_id_;
  params.ipc_callbacks_id = ipc_callbacks_id_;
  params.primary_key = key;
  params.key_path = key_path;
  if (value && !value->empty())
    std::swap(params.value, value->bits);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessValueWithKey(params));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(IndexedDBValue* value) {
  DCHECK(dispatcher_host_.get());

  DCHECK(kNoCursor == ipc_cursor_id_ || value == NULL);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  IndexedDBMsg_CallbacksSuccessValue_Params params;
  params.ipc_thread_id = ipc_thread_id_;
  params.ipc_callbacks_id = ipc_callbacks_id_;
  if (value && !value->empty())
    std::swap(params.value, value->bits);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessValue(params));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(const IndexedDBKey& value) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIndexedDBKey(
      ipc_thread_id_, ipc_callbacks_id_, value));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess(int64 value) {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessInteger(
      ipc_thread_id_, ipc_callbacks_id_, value));
  dispatcher_host_ = NULL;
}

void IndexedDBCallbacks::OnSuccess() {
  DCHECK(dispatcher_host_.get());

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(kNoDatabaseCallbacks, ipc_database_callbacks_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessUndefined(
      ipc_thread_id_, ipc_callbacks_id_));
  dispatcher_host_ = NULL;
}

}  // namespace content
