// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "network/public/cpp/web_socket_read_queue.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

namespace mojo {

struct WebSocketReadQueue::Operation {
  Operation(uint32_t num_bytes,
            const base::Callback<void(const char*)>& callback)
      : num_bytes_(num_bytes), callback_(callback), current_num_bytes_(0) {}

  uint32_t num_bytes_;
  base::Callback<void(const char*)> callback_;

  // If the initial read doesn't return enough data, this array is used to
  // accumulate data from multiple reads.
  scoped_ptr<char[]> data_buffer_;
  // The number of bytes accumulated so far.
  uint32_t current_num_bytes_;
};

WebSocketReadQueue::WebSocketReadQueue(DataPipeConsumerHandle handle)
    : handle_(handle), is_busy_(false), weak_factory_(this) {
}

WebSocketReadQueue::~WebSocketReadQueue() {
}

void WebSocketReadQueue::Read(
    uint32_t num_bytes,
    const base::Callback<void(const char*)>& callback) {
  Operation* op = new Operation(num_bytes, callback);
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
    const void* buffer = nullptr;
    uint32_t buffer_size = 0;
    MojoResult result = BeginReadDataRaw(handle_, &buffer, &buffer_size,
                                         MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      Wait();
      return;
    }

    // http://crbug.com/490193 This should run callback as well. May need to
    // change the callback signature.
    if (result != MOJO_RESULT_OK)
      return;

    uint32_t bytes_read = buffer_size < op->num_bytes_ - op->current_num_bytes_
                              ? buffer_size
                              : op->num_bytes_ - op->current_num_bytes_;

    // If this is not the initial read, or this is the initial read but doesn't
    // return enough data, copy the data into |op->data_buffer_|.
    if (op->data_buffer_ ||
        bytes_read < op->num_bytes_ - op->current_num_bytes_) {
      if (!op->data_buffer_) {
        DCHECK_EQ(0u, op->current_num_bytes_);
        op->data_buffer_.reset(new char[op->num_bytes_]);
      }

      memcpy(op->data_buffer_.get() + op->current_num_bytes_, buffer,
             bytes_read);
    }
    op->current_num_bytes_ += bytes_read;
    DataPipeConsumerHandle handle = handle_;
    base::WeakPtr<WebSocketReadQueue> self(weak_factory_.GetWeakPtr());

    if (op->current_num_bytes_ >= op->num_bytes_) {
      DCHECK_EQ(op->current_num_bytes_, op->num_bytes_);
      const char* returned_buffer = op->data_buffer_
                                        ? op->data_buffer_.get()
                                        : static_cast<const char*>(buffer);

      // Ensure |op| is deleted, whether or not |this| goes away.
      scoped_ptr<Operation> op_deleter(op);
      queue_.weak_erase(queue_.begin());

      // This call may delete |this|. In that case, |self| will be invalidated.
      // It may re-enter Read() too. Because |is_busy_| is true during the whole
      // process, TryToRead() won't be re-entered.
      op->callback_.Run(returned_buffer);
    }

    EndReadDataRaw(handle, bytes_read);

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
