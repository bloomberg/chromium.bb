// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_PUBLIC_CPP_WEB_SOCKET_READ_QUEUE_H_
#define MOJO_SERVICES_NETWORK_PUBLIC_CPP_WEB_SOCKET_READ_QUEUE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/message_pump/handle_watcher.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mojo {

// This class simplifies the handling of multiple Reads on a DataPipe. It reads
// the data in the expected chunk size, calling the callback once a full chunk
// is ready. Callbacks are owned by this class, and are guaranteed not to be
// called after this class is destroyed.
// See also: WebSocketWriteQueue
class WebSocketReadQueue {
 public:
  explicit WebSocketReadQueue(DataPipeConsumerHandle handle);
  ~WebSocketReadQueue();

  void Read(uint32_t num_bytes,
            const base::Callback<void(const char*)>& callback);

 private:
  struct Operation;

  void TryToRead();
  void Wait();
  void OnHandleReady(MojoResult result);

  DataPipeConsumerHandle handle_;
  common::HandleWatcher handle_watcher_;
  ScopedVector<Operation> queue_;
  bool is_busy_;
  base::WeakPtrFactory<WebSocketReadQueue> weak_factory_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_PUBLIC_CPP_WEB_SOCKET_READ_QUEUE_H_
