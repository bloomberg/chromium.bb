// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/network/web_socket_read_queue.h"

#include "base/bind.h"

namespace mojo {

struct WebSocketReadQueue::Operation {
  uint32_t num_bytes_;
  base::Callback<void(const char*)> callback_;
};

WebSocketReadQueue::WebSocketReadQueue(DataPipeConsumerHandle handle)
    : handle_(handle), is_waiting_(false) {
}

WebSocketReadQueue::~WebSocketReadQueue() {
}

void WebSocketReadQueue::Read(uint32_t num_bytes,
                              base::Callback<void(const char*)> callback) {
  Operation* op = new Operation;
  op->num_bytes_ = num_bytes;
  op->callback_ = callback;
  queue_.push_back(op);

  if (!is_waiting_)
    TryToRead();
}

void WebSocketReadQueue::TryToRead() {
  Operation* op = queue_[0];
  const void* buffer = NULL;
  uint32_t bytes_read = op->num_bytes_;
  MojoResult result = BeginReadDataRaw(
      handle_, &buffer, &bytes_read, MOJO_READ_DATA_FLAG_ALL_OR_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    EndReadDataRaw(handle_, bytes_read);
    Wait();
    return;
  }

  // Ensure |op| is deleted, whether or not |this| goes away.
  scoped_ptr<Operation> op_deleter(op);
  queue_.weak_erase(queue_.begin());
  if (result != MOJO_RESULT_OK)
    return;
  DataPipeConsumerHandle handle = handle_;
  op->callback_.Run(static_cast<const char*>(buffer));  // may delete |this|
  EndReadDataRaw(handle, bytes_read);
}

void WebSocketReadQueue::Wait() {
  is_waiting_ = true;
  handle_watcher_.Start(
      handle_,
      MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_DEADLINE_INDEFINITE,
      base::Bind(&WebSocketReadQueue::OnHandleReady, base::Unretained(this)));
}

void WebSocketReadQueue::OnHandleReady(MojoResult result) {
  is_waiting_ = false;
  TryToRead();
}

}  // namespace mojo
