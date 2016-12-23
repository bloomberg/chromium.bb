// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/cursor_impl.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

namespace content {

class CursorImpl::IDBThreadHelper {
 public:
  explicit IDBThreadHelper(std::unique_ptr<IndexedDBCursor> cursor);
  ~IDBThreadHelper();

  void Advance(uint32_t count, scoped_refptr<IndexedDBCallbacks> callbacks);
  void Continue(const IndexedDBKey& key,
                const IndexedDBKey& primary_key,
                scoped_refptr<IndexedDBCallbacks> callbacks);
  void Prefetch(int32_t count, scoped_refptr<IndexedDBCallbacks> callbacks);
  void PrefetchReset(int32_t used_prefetches, int32_t unused_prefetches);

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  std::unique_ptr<IndexedDBCursor> cursor_;
  const url::Origin origin_;

  DISALLOW_COPY_AND_ASSIGN(IDBThreadHelper);
};

CursorImpl::CursorImpl(std::unique_ptr<IndexedDBCursor> cursor,
                       const url::Origin& origin,
                       scoped_refptr<IndexedDBDispatcherHost> dispatcher_host)
    : helper_(new IDBThreadHelper(std::move(cursor))),
      dispatcher_host_(std::move(dispatcher_host)),
      origin_(origin),
      idb_runner_(base::ThreadTaskRunnerHandle::Get()) {}

CursorImpl::~CursorImpl() {
  idb_runner_->DeleteSoon(FROM_HERE, helper_);
}

void CursorImpl::Advance(
    uint32_t count,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));
  idb_runner_->PostTask(FROM_HERE, base::Bind(&IDBThreadHelper::Advance,
                                              base::Unretained(helper_), count,
                                              base::Passed(&callbacks)));
}

void CursorImpl::Continue(
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));
  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::Continue, base::Unretained(helper_), key,
                 primary_key, base::Passed(&callbacks)));
}

void CursorImpl::Prefetch(
    int32_t count,
    ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks_info) {
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      dispatcher_host_.get(), origin_, std::move(callbacks_info)));
  idb_runner_->PostTask(FROM_HERE, base::Bind(&IDBThreadHelper::Prefetch,
                                              base::Unretained(helper_), count,
                                              base::Passed(&callbacks)));
}

void CursorImpl::PrefetchReset(
    int32_t used_prefetches,
    int32_t unused_prefetches,
    const std::vector<std::string>& unused_blob_uuids) {
  for (const auto& uuid : unused_blob_uuids)
    dispatcher_host_->DropBlobData(uuid);

  idb_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IDBThreadHelper::PrefetchReset, base::Unretained(helper_),
                 used_prefetches, unused_prefetches));
}

CursorImpl::IDBThreadHelper::IDBThreadHelper(
    std::unique_ptr<IndexedDBCursor> cursor)
    : cursor_(std::move(cursor)) {}

CursorImpl::IDBThreadHelper::~IDBThreadHelper() {
  cursor_->RemoveCursorFromTransaction();
}

void CursorImpl::IDBThreadHelper::Advance(
    uint32_t count,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  cursor_->Advance(count, std::move(callbacks));
}

void CursorImpl::IDBThreadHelper::Continue(
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  cursor_->Continue(
      key.IsValid() ? base::MakeUnique<IndexedDBKey>(key) : nullptr,
      primary_key.IsValid() ? base::MakeUnique<IndexedDBKey>(primary_key)
                            : nullptr,
      std::move(callbacks));
}

void CursorImpl::IDBThreadHelper::Prefetch(
    int32_t count,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  cursor_->PrefetchContinue(count, std::move(callbacks));
}

void CursorImpl::IDBThreadHelper::PrefetchReset(int32_t used_prefetches,
                                                int32_t unused_prefetches) {
  leveldb::Status s =
      cursor_->PrefetchReset(used_prefetches, unused_prefetches);
  // TODO(cmumford): Handle this error (crbug.com/363397)
  if (!s.ok())
    DLOG(ERROR) << "Unable to reset prefetch";
}

}  // namespace content
