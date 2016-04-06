// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_THREAD_PIPE_MANAGER_H_
#define BLIMP_NET_THREAD_PIPE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_net_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blimp {

class BlimpMessageProcessor;
class BlimpMessageThreadPipe;
class BrowserConnectionHandler;
class IoThreadPipeManager;

// This class is used on the UI thread for registering features and setting up
// BlimpMessageThreadPipes for communicating with |connection_handler| on the
// IO thread.
class BLIMP_NET_EXPORT ThreadPipeManager {
 public:
  // Caller is responsible for ensuring that |connection_handler| outlives
  // |this|.
  explicit ThreadPipeManager(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      BrowserConnectionHandler* connection_handler);

  ~ThreadPipeManager();

  // Registers a message processor |incoming_processor| which will receive all
  // messages of the |type| specified. Returns a BlimpMessageProcessor object
  // for sending messages of type |type|.
  std::unique_ptr<BlimpMessageProcessor> RegisterFeature(
      BlimpMessage::Type type,
      BlimpMessageProcessor* incoming_processor);

 private:
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // Container for BlimpMessageThreadPipes that are destroyed on IO thread.
  std::unique_ptr<IoThreadPipeManager> io_pipe_manager_;

  // Pipes for routing messages from the IO to the UI thread.
  // Incoming messages are only routed to the UI thread since all features run
  // there.
  std::vector<std::unique_ptr<BlimpMessageThreadPipe>> incoming_pipes_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPipeManager);
};

}  // namespace blimp

#endif  // BLIMP_NET_THREAD_PIPE_MANAGER_H_
