// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_PUBLIC_CPP_WEB_SOCKET_WRITE_QUEUE_H_
#define MOJO_SERVICES_NETWORK_PUBLIC_CPP_WEB_SOCKET_WRITE_QUEUE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/message_pump/handle_watcher.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mojo {

// This class simplifies the handling of multiple Writes on a DataPipe. It
// writes each chunk all at once (or waits until the pipe is ready before
// writing), calling the callback when finished. Callbacks are owned by this
// class, and are guaranteed not to be called after this class is destroyed.
// See also: WebSocketReadQueue
class WebSocketWriteQueue {
 public:
  explicit WebSocketWriteQueue(DataPipeProducerHandle handle);
  ~WebSocketWriteQueue();

  void Write(const char* data,
             uint32_t num_bytes,
             base::Callback<void(const char*)> callback);

 private:
  struct Operation;

  void TryToWrite();
  void Wait();
  void OnHandleReady(MojoResult result);

  DataPipeProducerHandle handle_;
  common::HandleWatcher handle_watcher_;
  ScopedVector<Operation> queue_;
  bool is_busy_;
  base::WeakPtrFactory<WebSocketWriteQueue> weak_factory_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_PUBLIC_CPP_WEB_SOCKET_WRITE_QUEUE_H_
