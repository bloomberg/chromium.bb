// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_INDEXED_DB_CALLBACKS_IMPL_H_
#define CONTENT_CHILD_INDEXED_DB_INDEXED_DB_CALLBACKS_IMPL_H_

#include "content/common/indexed_db/indexed_db.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace blink {
class WebIDBCallbacks;
}

namespace content {

class ThreadSafeSender;

// Implements the child-process end of the pipe used to deliver callbacks. It
// is owned by the IO thread. |callback_runner_| is used to post tasks back to
// the thread which owns the blink::WebIDBCallbacks.
class IndexedDBCallbacksImpl : public indexed_db::mojom::Callbacks {
 public:
  class InternalState {
   public:
    InternalState(std::unique_ptr<blink::WebIDBCallbacks> callbacks,
                  scoped_refptr<ThreadSafeSender> thread_safe_sender);
    ~InternalState();

    void Error(int32_t code, const base::string16& message);
    void SuccessStringList(const std::vector<base::string16>& value);
    void Blocked(int64_t existing_version);
    void UpgradeNeeded(int32_t database_id,
                       int64_t old_version,
                       blink::WebIDBDataLoss data_loss,
                       const std::string& data_loss_message,
                       const content::IndexedDBDatabaseMetadata& metadata);
    void SuccessDatabase(int32_t database_id,
                         const content::IndexedDBDatabaseMetadata& metadata);
    void SuccessInteger(int64_t value);

   private:
    std::unique_ptr<blink::WebIDBCallbacks> callbacks_;
    scoped_refptr<ThreadSafeSender> thread_safe_sender_;

    DISALLOW_COPY_AND_ASSIGN(InternalState);
  };

  IndexedDBCallbacksImpl(std::unique_ptr<blink::WebIDBCallbacks> callbacks,
                         scoped_refptr<ThreadSafeSender> thread_safe_sender);
  ~IndexedDBCallbacksImpl() override;

  // indexed_db::mojom::Callbacks implementation:
  void Error(int32_t code, const base::string16& message) override;
  void SuccessStringList(const std::vector<base::string16>& value) override;
  void Blocked(int64_t existing_version) override;
  void UpgradeNeeded(
      int32_t database_id,
      int64_t old_version,
      blink::WebIDBDataLoss data_loss,
      const std::string& data_loss_message,
      const content::IndexedDBDatabaseMetadata& metadata) override;
  void SuccessDatabase(
      int32_t database_id,
      const content::IndexedDBDatabaseMetadata& metadata) override;
  void SuccessInteger(int64_t value) override;

 private:
  // |internal_state_| is owned by the thread on which |callback_runner_|
  // executes tasks and must be destroyed there.
  InternalState* internal_state_;
  scoped_refptr<base::SingleThreadTaskRunner> callback_runner_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBCallbacksImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_INDEXED_DB_CALLBACKS_IMPL_H_
