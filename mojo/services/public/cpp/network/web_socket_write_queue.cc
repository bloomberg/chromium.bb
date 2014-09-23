// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/network/web_socket_write_queue.h"

#include "base/bind.h"

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
    : handle_(handle), is_waiting_(false) {
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

  MojoResult result = MOJO_RESULT_SHOULD_WAIT;
  if (!is_waiting_)
    result = TryToWrite();

  // If we have to wait, make a local copy of the data so we know it will
  // live until we need it.
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    op->data_copy_.resize(num_bytes);
    memcpy(&op->data_copy_[0], data, num_bytes);
    op->data_ = &op->data_copy_[0];
  }
}

MojoResult WebSocketWriteQueue::TryToWrite() {
  Operation* op = queue_[0];
  uint32_t bytes_written = op->num_bytes_;
  MojoResult result = WriteDataRaw(
      handle_, op->data_, &bytes_written, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    Wait();
    return result;
  }

  // Ensure |op| is deleted, whether or not |this| goes away.
  scoped_ptr<Operation> op_deleter(op);
  queue_.weak_erase(queue_.begin());
  if (result != MOJO_RESULT_OK)
    return result;

  op->callback_.Run(op->data_);  // may delete |this|
  return result;
}

void WebSocketWriteQueue::Wait() {
  is_waiting_ = true;
  handle_watcher_.Start(handle_,
                        MOJO_HANDLE_SIGNAL_WRITABLE,
                        MOJO_DEADLINE_INDEFINITE,
                        base::Bind(&WebSocketWriteQueue::OnHandleReady,
                                   base::Unretained(this)));
}

void WebSocketWriteQueue::OnHandleReady(MojoResult result) {
  is_waiting_ = false;
  TryToWrite();
}

}  // namespace mojo
