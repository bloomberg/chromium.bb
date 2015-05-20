// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "network/public/cpp/web_socket_read_queue.h"

#include "base/bind.h"
#include "base/logging.h"

namespace mojo {

struct WebSocketReadQueue::Operation {
  uint32_t num_bytes_;
  base::Callback<void(const char*)> callback_;
};

WebSocketReadQueue::WebSocketReadQueue(DataPipeConsumerHandle handle)
    : handle_(handle), is_busy_(false), weak_factory_(this) {
}

WebSocketReadQueue::~WebSocketReadQueue() {
}

void WebSocketReadQueue::Read(uint32_t num_bytes,
                              base::Callback<void(const char*)> callback) {
  Operation* op = new Operation;
  op->num_bytes_ = num_bytes;
  op->callback_ = callback;
  queue_.push_back(op);

  if (is_busy_)
    return;

  is_busy_ = true;
  TryToRead();
}

void WebSocketReadQueue::TryToRead() {
  DCHECK(is_busy_);
  DCHECK(!queue_.empty());
  do {
    Operation* op = queue_[0];
    const void* buffer = NULL;
    uint32_t bytes_read = op->num_bytes_;
    MojoResult result = BeginReadDataRaw(
        handle_, &buffer, &bytes_read, MOJO_READ_DATA_FLAG_ALL_OR_NONE);
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

    uint32_t num_bytes = op_deleter->num_bytes_;
    DCHECK_LE(num_bytes, bytes_read);
    DataPipeConsumerHandle handle = handle_;

    base::WeakPtr<WebSocketReadQueue> self(weak_factory_.GetWeakPtr());

    // This call may delete |this|. In that case, |self| will be invalidated.
    // It may re-enter Read() too. Because |is_busy_| is true during the whole
    // process, TryToRead() won't be re-entered.
    op->callback_.Run(static_cast<const char*>(buffer));

    EndReadDataRaw(handle, num_bytes);

    if (!self)
      return;
  } while (!queue_.empty());
  is_busy_ = false;
}

void WebSocketReadQueue::Wait() {
  DCHECK(is_busy_);
  handle_watcher_.Start(
      handle_,
      MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_DEADLINE_INDEFINITE,
      base::Bind(&WebSocketReadQueue::OnHandleReady, base::Unretained(this)));
}

void WebSocketReadQueue::OnHandleReady(MojoResult result) {
  DCHECK(is_busy_);
  TryToRead();
}

}  // namespace mojo
