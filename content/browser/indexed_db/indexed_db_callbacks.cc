// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callbacks.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/fileapi_message_filter.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/indexed_db_metadata.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/quota/quota_manager.h"

using indexed_db::mojom::CallbacksAssociatedPtrInfo;
using storage::ShareableFileReference;

namespace content {

namespace {
const int32_t kNoCursor = -1;
const int64_t kNoTransaction = -1;
}

class IndexedDBCallbacks::IOThreadHelper {
 public:
  explicit IOThreadHelper(CallbacksAssociatedPtrInfo callbacks_info);
  ~IOThreadHelper();

  void SendError(const IndexedDBDatabaseError& error);
  void SendSuccessStringList(const std::vector<base::string16>& value);
  void SendBlocked(int64_t existing_version);
  void SendUpgradeNeeded(int32_t database_id,
                         int64_t old_version,
                         blink::WebIDBDataLoss data_loss,
                         const std::string& data_loss_message,
                         const content::IndexedDBDatabaseMetadata& metadata);
  void SendSuccessDatabase(int32_t database_id,
                           const content::IndexedDBDatabaseMetadata& metadata);
  void SendSuccessInteger(int64_t value);

 private:
  ::indexed_db::mojom::CallbacksAssociatedPtr callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadHelper);
};

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32_t ipc_thread_id,
                                       int32_t ipc_callbacks_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(kNoTransaction),
      ipc_database_id_(kNoDatabase),
      data_loss_(blink::WebIDBDataLossNone),
      sent_blocked_(false),
      io_helper_(nullptr) {}

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                       int32_t ipc_thread_id,
                                       int32_t ipc_callbacks_id,
                                       int32_t ipc_cursor_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id),
      ipc_cursor_id_(ipc_cursor_id),
      host_transaction_id_(kNoTransaction),
      ipc_database_id_(kNoDatabase),
      data_loss_(blink::WebIDBDataLossNone),
      sent_blocked_(false),
      io_helper_(nullptr) {}

IndexedDBCallbacks::IndexedDBCallbacks(
    IndexedDBDispatcherHost* dispatcher_host,
    const url::Origin& origin,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info)
    : dispatcher_host_(dispatcher_host),
      ipc_cursor_id_(kNoCursor),
      host_transaction_id_(kNoTransaction),
      origin_(origin),
      ipc_database_id_(kNoDatabase),
      data_loss_(blink::WebIDBDataLossNone),
      sent_blocked_(false),
      io_helper_(new IOThreadHelper(std::move(callbacks_info))) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  thread_checker_.DetachFromThread();
}

IndexedDBCallbacks::~IndexedDBCallbacks() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void IndexedDBCallbacks::OnError(const IndexedDBDatabaseError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);

  if (io_helper_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&IOThreadHelper::SendError,
                   base::Unretained(io_helper_.get()), error));
  } else {
    dispatcher_host_->Send(new IndexedDBMsg_CallbacksError(
        ipc_thread_id_, ipc_callbacks_id_, error.code(), error.message()));
  }
  dispatcher_host_ = nullptr;

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.Error",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

void IndexedDBCallbacks::OnSuccess(const std::vector<base::string16>& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);
  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendSuccessStringList,
                 base::Unretained(io_helper_.get()), value));
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnBlocked(int64_t existing_version) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);
  DCHECK_EQ(kNoCursor, ipc_cursor_id_);

  if (sent_blocked_)
    return;

  sent_blocked_ = true;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendBlocked,
                 base::Unretained(io_helper_.get()), existing_version));

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.Blocked",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

void IndexedDBCallbacks::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata,
    const IndexedDBDataLossInfo& data_loss_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);
  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);

  data_loss_ = data_loss_info.status;
  dispatcher_host_->RegisterTransactionId(host_transaction_id_, origin_);
  int32_t ipc_database_id =
      dispatcher_host_->Add(connection.release(), origin_);
  if (ipc_database_id < 0)
    return;

  ipc_database_id_ = ipc_database_id;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendUpgradeNeeded,
                 base::Unretained(io_helper_.get()), ipc_database_id,
                 old_version, data_loss_info.status, data_loss_info.message,
                 metadata));

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.UpgradeNeeded",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

void IndexedDBCallbacks::OnSuccess(
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(io_helper_);
  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_NE(kNoTransaction, host_transaction_id_);
  DCHECK_NE(ipc_database_id_ == kNoDatabase, !connection);

  scoped_refptr<IndexedDBCallbacks> self(this);

  int32_t ipc_object_id = kNoDatabase;
  // Only register if the connection was not previously sent in OnUpgradeNeeded.
  if (ipc_database_id_ == kNoDatabase) {
    ipc_object_id = dispatcher_host_->Add(connection.release(), origin_);
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadHelper::SendSuccessDatabase,
                 base::Unretained(io_helper_.get()), ipc_object_id, metadata));
  dispatcher_host_ = nullptr;

  if (!connection_open_start_time_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.IndexedDB.OpenTime.Success",
        base::TimeTicks::Now() - connection_open_start_time_);
    connection_open_start_time_ = base::TimeTicks();
  }
}

static std::string CreateBlobData(
    const IndexedDBBlobInfo& blob_info,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    base::TaskRunner* task_runner) {
  if (!blob_info.uuid().empty()) {
    // We're sending back a live blob, not a reference into our backing store.
    return dispatcher_host->HoldBlobData(blob_info);
  }
  scoped_refptr<ShareableFileReference> shareable_file =
      ShareableFileReference::Get(blob_info.file_path());
  if (!shareable_file.get()) {
    shareable_file = ShareableFileReference::GetOrCreate(
        blob_info.file_path(),
        ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE,
        task_runner);
    if (!blob_info.release_callback().is_null())
      shareable_file->AddFinalReleaseCallback(blob_info.release_callback());
  }
  return dispatcher_host->HoldBlobData(blob_info);
}

static bool CreateAllBlobs(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<IndexedDBMsg_BlobOrFileInfo>* blob_or_file_info,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host) {
  IDB_TRACE("IndexedDBCallbacks::CreateAllBlobs");
  DCHECK_EQ(blob_info.size(), blob_or_file_info->size());
  size_t i;
  if (!dispatcher_host->blob_storage_context())
    return false;
  for (i = 0; i < blob_info.size(); ++i) {
    (*blob_or_file_info)[i].uuid =
        CreateBlobData(blob_info[i], dispatcher_host,
                       dispatcher_host->context()->TaskRunner());
  }
  return true;
}

template <class ParamType, class MsgType>
static void CreateBlobsAndSend(
    ParamType* params,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<IndexedDBMsg_BlobOrFileInfo>* blob_or_file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (CreateAllBlobs(blob_info, blob_or_file_info, dispatcher_host))
    dispatcher_host->Send(new MsgType(*params));
}

static void BlobLookupForCursorPrefetch(
    IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params* params,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    const std::vector<IndexedDBValue>& values) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(values.size(), params->values.size());

  for (size_t i = 0; i < values.size(); ++i) {
    if (!CreateAllBlobs(values[i].blob_info,
                        &params->values[i].blob_or_file_info, dispatcher_host))
      return;
  }

  dispatcher_host->Send(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch(*params));
}

static void BlobLookupForGetAll(
    IndexedDBMsg_CallbacksSuccessArray_Params* params,
    scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
    const std::vector<IndexedDBReturnValue>& values) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(values.size(), params->values.size());

  for (size_t i = 0; i < values.size(); ++i) {
    if (!CreateAllBlobs(values[i].blob_info,
                        &params->values[i].blob_or_file_info, dispatcher_host))
      return;
  }

  dispatcher_host->Send(new IndexedDBMsg_CallbacksSuccessArray(*params));
}

static void FillInBlobData(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<IndexedDBMsg_BlobOrFileInfo>* blob_or_file_info) {
  for (const auto& iter : blob_info) {
    if (iter.is_file()) {
      IndexedDBMsg_BlobOrFileInfo info;
      info.is_file = true;
      info.mime_type = iter.type();
      info.file_name = iter.file_name();
      info.file_path = iter.file_path().AsUTF16Unsafe();
      info.size = iter.size();
      info.last_modified = iter.last_modified().ToDoubleT();
      blob_or_file_info->push_back(info);
    } else {
      IndexedDBMsg_BlobOrFileInfo info;
      info.mime_type = iter.type();
      info.size = iter.size();
      blob_or_file_info->push_back(info);
    }
  }
}

void IndexedDBCallbacks::RegisterBlobsAndSend(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    const base::Closure& callback) {
  for (const auto& iter : blob_info) {
    if (!iter.mark_used_callback().is_null())
      iter.mark_used_callback().Run();
  }
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, callback);
}

void IndexedDBCallbacks::OnSuccess(scoped_refptr<IndexedDBCursor> cursor,
                                   const IndexedDBKey& key,
                                   const IndexedDBKey& primary_key,
                                   IndexedDBValue* value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(!io_helper_);

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  int32_t ipc_object_id = dispatcher_host_->Add(cursor.get());
  std::unique_ptr<IndexedDBMsg_CallbacksSuccessIDBCursor_Params> params(
      new IndexedDBMsg_CallbacksSuccessIDBCursor_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->ipc_cursor_id = ipc_object_id;
  params->key = key;
  params->primary_key = primary_key;
  if (value && !value->empty())
    std::swap(params->value.bits, value->bits);
  // TODO(alecflett): Avoid a copy here: the whole params object is
  // being copied into the message.
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIDBCursor(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessIDBCursor_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->value.blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(
            CreateBlobsAndSend<IndexedDBMsg_CallbacksSuccessIDBCursor_Params,
                               IndexedDBMsg_CallbacksSuccessIDBCursor>,
            base::Owned(params.release()), dispatcher_host_, value->blob_info,
            base::Unretained(&p->value.blob_or_file_info)));
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess(const IndexedDBKey& key,
                                   const IndexedDBKey& primary_key,
                                   IndexedDBValue* value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(!io_helper_);

  DCHECK_NE(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  IndexedDBCursor* idb_cursor =
      dispatcher_host_->GetCursorFromId(ipc_cursor_id_);

  DCHECK(idb_cursor);
  if (!idb_cursor)
    return;

  std::unique_ptr<IndexedDBMsg_CallbacksSuccessCursorContinue_Params> params(
      new IndexedDBMsg_CallbacksSuccessCursorContinue_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->ipc_cursor_id = ipc_cursor_id_;
  params->key = key;
  params->primary_key = primary_key;
  if (value && !value->empty())
    std::swap(params->value.bits, value->bits);
  // TODO(alecflett): Avoid a copy here: the whole params object is
  // being copied into the message.
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(
        new IndexedDBMsg_CallbacksSuccessCursorContinue(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessCursorContinue_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->value.blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(CreateBlobsAndSend<
                       IndexedDBMsg_CallbacksSuccessCursorContinue_Params,
                       IndexedDBMsg_CallbacksSuccessCursorContinue>,
                   base::Owned(params.release()), dispatcher_host_,
                   value->blob_info,
                   base::Unretained(&p->value.blob_or_file_info)));
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccessWithPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    std::vector<IndexedDBValue>* values) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(!io_helper_);
  DCHECK_EQ(keys.size(), primary_keys.size());
  DCHECK_EQ(keys.size(), values->size());

  DCHECK_NE(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::vector<IndexedDBKey> msg_keys;
  std::vector<IndexedDBKey> msg_primary_keys;

  for (size_t i = 0; i < keys.size(); ++i) {
    msg_keys.push_back(keys[i]);
    msg_primary_keys.push_back(primary_keys[i]);
  }

  std::unique_ptr<IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params> params(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->ipc_cursor_id = ipc_cursor_id_;
  params->keys = msg_keys;
  params->primary_keys = msg_primary_keys;
  params->values.resize(values->size());

  bool found_blob_info = false;
  for (size_t i = 0; i < values->size(); ++i) {
    params->values[i].bits.swap(values->at(i).bits);
    if (!values->at(i).blob_info.empty()) {
      found_blob_info = true;
      FillInBlobData(values->at(i).blob_info,
                     &params->values[i].blob_or_file_info);
      for (const auto& blob_iter : values->at(i).blob_info) {
        if (!blob_iter.mark_used_callback().is_null())
          blob_iter.mark_used_callback().Run();
      }
    }
  }

  if (found_blob_info) {
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(BlobLookupForCursorPrefetch,
                                       base::Owned(params.release()),
                                       dispatcher_host_,
                                       *values));
  } else {
    dispatcher_host_->Send(
        new IndexedDBMsg_CallbacksSuccessCursorPrefetch(*params.get()));
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess(IndexedDBReturnValue* value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(!io_helper_);

  if (value && value->primary_key.IsValid()) {
    DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  } else {
    DCHECK(kNoCursor == ipc_cursor_id_ || value == NULL);
  }
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::unique_ptr<IndexedDBMsg_CallbacksSuccessValue_Params> params(
      new IndexedDBMsg_CallbacksSuccessValue_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  if (value && value->primary_key.IsValid()) {
    params->value.primary_key = value->primary_key;
    params->value.key_path = value->key_path;
  }
  if (value && !value->empty())
    std::swap(params->value.bits, value->bits);
  if (!value || value->blob_info.empty()) {
    dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessValue(*params));
  } else {
    IndexedDBMsg_CallbacksSuccessValue_Params* p = params.get();
    FillInBlobData(value->blob_info, &p->value.blob_or_file_info);
    RegisterBlobsAndSend(
        value->blob_info,
        base::Bind(CreateBlobsAndSend<IndexedDBMsg_CallbacksSuccessValue_Params,
                                      IndexedDBMsg_CallbacksSuccessValue>,
                   base::Owned(params.release()), dispatcher_host_,
                   value->blob_info,
                   base::Unretained(&p->value.blob_or_file_info)));
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccessArray(
    std::vector<IndexedDBReturnValue>* values,
    const IndexedDBKeyPath& key_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(!io_helper_);

  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  std::unique_ptr<IndexedDBMsg_CallbacksSuccessArray_Params> params(
      new IndexedDBMsg_CallbacksSuccessArray_Params());
  params->ipc_thread_id = ipc_thread_id_;
  params->ipc_callbacks_id = ipc_callbacks_id_;
  params->values.resize(values->size());

  bool found_blob_info = false;
  for (size_t i = 0; i < values->size(); ++i) {
    IndexedDBMsg_ReturnValue& pvalue = params->values[i];
    IndexedDBReturnValue& value = (*values)[i];
    pvalue.bits.swap(value.bits);
    if (!value.blob_info.empty()) {
      found_blob_info = true;
      FillInBlobData(value.blob_info, &pvalue.blob_or_file_info);
      for (const auto& blob_info : value.blob_info) {
        if (!blob_info.mark_used_callback().is_null())
          blob_info.mark_used_callback().Run();
      }
    }
    pvalue.primary_key = value.primary_key;
    pvalue.key_path = key_path;
  }

  if (found_blob_info) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(BlobLookupForGetAll, base::Owned(params.release()),
                   dispatcher_host_, *values));
  } else {
    dispatcher_host_->Send(
        new IndexedDBMsg_CallbacksSuccessArray(*params.get()));
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess(const IndexedDBKey& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(!io_helper_);

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessIndexedDBKey(
      ipc_thread_id_, ipc_callbacks_id_, value));
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess(int64_t value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  if (io_helper_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&IOThreadHelper::SendSuccessInteger,
                   base::Unretained(io_helper_.get()), value));
  } else {
    DCHECK_EQ(kNoCursor, ipc_cursor_id_);
    DCHECK_EQ(kNoTransaction, host_transaction_id_);
    DCHECK_EQ(kNoDatabase, ipc_database_id_);
    DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

    dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessInteger(
        ipc_thread_id_, ipc_callbacks_id_, value));
  }
  dispatcher_host_ = nullptr;
}

void IndexedDBCallbacks::OnSuccess() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dispatcher_host_);
  DCHECK(!io_helper_);

  DCHECK_EQ(kNoCursor, ipc_cursor_id_);
  DCHECK_EQ(kNoTransaction, host_transaction_id_);
  DCHECK_EQ(kNoDatabase, ipc_database_id_);
  DCHECK_EQ(blink::WebIDBDataLossNone, data_loss_);

  dispatcher_host_->Send(new IndexedDBMsg_CallbacksSuccessUndefined(
      ipc_thread_id_, ipc_callbacks_id_));
  dispatcher_host_ = nullptr;
}

bool IndexedDBCallbacks::IsValid() const {
  DCHECK(dispatcher_host_.get());

  return dispatcher_host_->IsOpen();
}

void IndexedDBCallbacks::SetConnectionOpenStartTime(
    const base::TimeTicks& start_time) {
  connection_open_start_time_ = start_time;
}

IndexedDBCallbacks::IOThreadHelper::IOThreadHelper(
    CallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callbacks_.Bind(std::move(callbacks_info));
}

IndexedDBCallbacks::IOThreadHelper::~IOThreadHelper() {}

void IndexedDBCallbacks::IOThreadHelper::SendError(
    const IndexedDBDatabaseError& error) {
  callbacks_->Error(error.code(), error.message());
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessStringList(
    const std::vector<base::string16>& value) {
  callbacks_->SuccessStringList(value);
}

void IndexedDBCallbacks::IOThreadHelper::SendBlocked(int64_t existing_version) {
  callbacks_->Blocked(existing_version);
}

void IndexedDBCallbacks::IOThreadHelper::SendUpgradeNeeded(
    int32_t database_id,
    int64_t old_version,
    blink::WebIDBDataLoss data_loss,
    const std::string& data_loss_message,
    const content::IndexedDBDatabaseMetadata& metadata) {
  callbacks_->UpgradeNeeded(database_id, old_version, data_loss,
                            data_loss_message, metadata);
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessDatabase(
    int32_t database_id,
    const content::IndexedDBDatabaseMetadata& metadata) {
  callbacks_->SuccessDatabase(database_id, metadata);
}

void IndexedDBCallbacks::IOThreadHelper::SendSuccessInteger(int64_t value) {
  callbacks_->SuccessInteger(value);
}

}  // namespace content
