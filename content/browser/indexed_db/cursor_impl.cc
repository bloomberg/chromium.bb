// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/cursor_impl.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

using blink::IndexedDBKey;

namespace content {

// TODO(cmp): Flatten calls / remove this class once IDB task runner CL settles.
class CursorImpl::IDBSequenceHelper {
 public:
  explicit IDBSequenceHelper(std::unique_ptr<IndexedDBCursor> cursor,
                             base::SequencedTaskRunner* idb_runner);
  ~IDBSequenceHelper();

  void Advance(uint32_t count,
               base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
               blink::mojom::IDBCursor::AdvanceCallback callback);
  void Continue(base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
                const IndexedDBKey& key,
                const IndexedDBKey& primary_key,
                blink::mojom::IDBCursor::CursorContinueCallback callback);
  void Prefetch(base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
                int32_t count,
                blink::mojom::IDBCursor::PrefetchCallback callback);
  void PrefetchReset(int32_t used_prefetches, int32_t unused_prefetches);

  void OnRemoveBinding(base::OnceClosure remove_binding_cb);

 private:
  std::unique_ptr<IndexedDBCursor> cursor_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IDBSequenceHelper);
};

CursorImpl::CursorImpl(std::unique_ptr<IndexedDBCursor> cursor,
                       const url::Origin& origin,
                       IndexedDBDispatcherHost* dispatcher_host,
                       scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : dispatcher_host_(dispatcher_host),
      origin_(origin),
      idb_runner_(std::move(idb_runner)) {
  DCHECK(idb_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_ = base::WrapUnique(
      new IDBSequenceHelper(std::move(cursor), idb_runner_.get()));
}

CursorImpl::~CursorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CursorImpl::Advance(uint32_t count,
                         blink::mojom::IDBCursor::AdvanceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_->Advance(count, dispatcher_host_->AsWeakPtr(), std::move(callback));
}

void CursorImpl::CursorContinue(
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    blink::mojom::IDBCursor::CursorContinueCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_->Continue(dispatcher_host_->AsWeakPtr(), key, primary_key,
                    std::move(callback));
}

void CursorImpl::Prefetch(int32_t count,
                          blink::mojom::IDBCursor::PrefetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_->Prefetch(dispatcher_host_->AsWeakPtr(), count, std::move(callback));
}

void CursorImpl::PrefetchReset(int32_t used_prefetches,
                               int32_t unused_prefetches) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_->PrefetchReset(used_prefetches, unused_prefetches);
}

void CursorImpl::OnRemoveBinding(base::OnceClosure remove_binding_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_->OnRemoveBinding(std::move(remove_binding_cb));
}

CursorImpl::IDBSequenceHelper::IDBSequenceHelper(
    std::unique_ptr<IndexedDBCursor> cursor,
    base::SequencedTaskRunner* idb_runner)
    : cursor_(std::move(cursor)) {
  DCHECK(idb_runner->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CursorImpl::IDBSequenceHelper::~IDBSequenceHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CursorImpl::IDBSequenceHelper::Advance(
    uint32_t count,
    base::WeakPtr<content::IndexedDBDispatcherHost> dispatcher_host,
    blink::mojom::IDBCursor::AdvanceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_->Advance(count, std::move(dispatcher_host), std::move(callback));
}

void CursorImpl::IDBSequenceHelper::Continue(
    base::WeakPtr<content::IndexedDBDispatcherHost> dispatcher_host,
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    blink::mojom::IDBCursor::CursorContinueCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_->Continue(
      std::move(dispatcher_host),
      key.IsValid() ? std::make_unique<IndexedDBKey>(key) : nullptr,
      primary_key.IsValid() ? std::make_unique<IndexedDBKey>(primary_key)
                            : nullptr,
      std::move(callback));
}

void CursorImpl::IDBSequenceHelper::Prefetch(
    base::WeakPtr<content::IndexedDBDispatcherHost> dispatcher_host,
    int32_t count,
    blink::mojom::IDBCursor::PrefetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_->PrefetchContinue(std::move(dispatcher_host), count,
                            std::move(callback));
}

void CursorImpl::IDBSequenceHelper::PrefetchReset(int32_t used_prefetches,
                                                  int32_t unused_prefetches) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status s =
      cursor_->PrefetchReset(used_prefetches, unused_prefetches);
  // TODO(cmumford): Handle this error (crbug.com/363397)
  if (!s.ok())
    DLOG(ERROR) << "Unable to reset prefetch";
}

void CursorImpl::IDBSequenceHelper::OnRemoveBinding(
    base::OnceClosure remove_binding_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_->OnRemoveBinding(std::move(remove_binding_cb));
}

}  // namespace content
