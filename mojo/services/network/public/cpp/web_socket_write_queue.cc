// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "network/public/cpp/web_socket_write_queue.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"

namespace mojo {

struct WebSocketWriteQueue::Operation {
  uint32_t num_bytes_;
  base::Callback<void(const char*)> callback_;

  const char* data_;
  // Only initialized if the initial Write fails. This saves a copy in
  // the common case.
  std::vector<char> data_copy_;
};

WebSocketWriteQueue::WebSocketWriteQueue(DataPipeProducerHandle handle)
    : handle_(handle), is_busy_(false), weak_factory_(this) {
}

WebSocketWriteQueue::~WebSocketWriteQueue() {
}

void WebSocketWriteQueue::Write(const char* data,
                                uint32_t num_bytes,
                                base::Callback<void(const char*)> callback) {
  Operation* op = new Operation;
  op->num_bytes_ = num_bytes;
  op->callback_ = callback;
  op->data_ = data;
  queue_.push_back(op);

  if (!is_busy_) {
    is_busy_ = true;
    // This call may reset |is_busy_| to false.
    TryToWrite();
  }

  if (is_busy_) {
    // If we have to wait, make a local copy of the data so we know it will
    // live until we need it.
    op->data_copy_.resize(num_bytes);
    memcpy(&op->data_copy_[0], data, num_bytes);
    op->data_ = &op->data_copy_[0];
  }
}

void WebSocketWriteQueue::TryToWrite() {
  DCHECK(is_busy_);
  DCHECK(!queue_.empty());
  do {
    Operation* op = queue_[0];
    uint32_t bytes_written = op->num_bytes_;
    MojoResult result = WriteDataRaw(
        handle_, op->data_, &bytes_written, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      Wait();
      return;
    }

    // Ensure |op| is deleted, whether or not |this| goes away.
    scoped_ptr<Operation> op_deleter(op);
    queue_.weak_erase(queue_.begin());

    // http://crbug.com/490193 This should run callback as well. May need to
    // change the callback signature.
    if (result != MOJO_RESULT_OK)
      return;

    base::WeakPtr<WebSocketWriteQueue> self(weak_factory_.GetWeakPtr());

    // This call may delete |this|. In that case, |self| will be invalidated.
    // It may re-enter Write() too. Because |is_busy_| is true during the whole
    // process, TryToWrite() won't be re-entered.
    op->callback_.Run(op->data_);

    if (!self)
      return;
  } while (!queue_.empty());
  is_busy_ = false;
}

void WebSocketWriteQueue::Wait() {
  DCHECK(is_busy_);
  handle_watcher_.Start(handle_,
                        MOJO_HANDLE_SIGNAL_WRITABLE,
                        MOJO_DEADLINE_INDEFINITE,
                        base::Bind(&WebSocketWriteQueue::OnHandleReady,
                                   base::Unretained(this)));
}

void WebSocketWriteQueue::OnHandleReady(MojoResult result) {
  DCHECK(is_busy_);
  TryToWrite();
}

}  // namespace mojo
