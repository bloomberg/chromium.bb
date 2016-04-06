// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/thread_pipe_manager.h"

#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_message_thread_pipe.h"
#include "blimp/net/browser_connection_handler.h"

namespace blimp {

// IoThreadPipeManager is created on the UI thread, and then used and destroyed
// on the IO thread.
// It works with |connection_handler| to register features on the IO thread,
// and manages IO-thread-side BlimpMessageThreadPipes.
class IoThreadPipeManager {
 public:
  explicit IoThreadPipeManager(BrowserConnectionHandler* connection_handler);
  virtual ~IoThreadPipeManager();

  // Connects message pipes between the specified feature and the network layer,
  // using |incoming_proxy| as the incoming message processor, and connecting
  // |outgoing_pipe| to the actual message sender.
  void RegisterFeature(BlimpMessage::Type type,
                       std::unique_ptr<BlimpMessageThreadPipe> outgoing_pipe,
                       std::unique_ptr<BlimpMessageProcessor> incoming_proxy);

 private:
  BrowserConnectionHandler* connection_handler_;

  // Container for the feature-specific MessageProcessors.
  // IO-side proxy for sending messages to UI thread.
  std::vector<std::unique_ptr<BlimpMessageProcessor>> incoming_proxies_;

  // Containers for the MessageProcessors used to write feature-specific
  // messages to the network, and the thread-pipe endpoints through which
  // they are used from the UI thread.
  std::vector<std::unique_ptr<BlimpMessageProcessor>>
      outgoing_message_processors_;
  std::vector<std::unique_ptr<BlimpMessageThreadPipe>> outgoing_pipes_;

  DISALLOW_COPY_AND_ASSIGN(IoThreadPipeManager);
};

IoThreadPipeManager::IoThreadPipeManager(
    BrowserConnectionHandler* connection_handler)
    : connection_handler_(connection_handler) {
  DCHECK(connection_handler_);
}

IoThreadPipeManager::~IoThreadPipeManager() {}

void IoThreadPipeManager::RegisterFeature(
    BlimpMessage::Type type,
    std::unique_ptr<BlimpMessageThreadPipe> outgoing_pipe,
    std::unique_ptr<BlimpMessageProcessor> incoming_proxy) {
  // Registers |incoming_proxy| as the message processor for incoming
  // messages with |type|. Sets the returned outgoing message processor as the
  // target of the |outgoing_pipe|.
  std::unique_ptr<BlimpMessageProcessor> outgoing_message_processor =
      connection_handler_->RegisterFeature(type, incoming_proxy.get());
  outgoing_pipe->set_target_processor(outgoing_message_processor.get());

  // This object manages the lifetimes of the pipe, proxy and target processor.
  incoming_proxies_.push_back(std::move(incoming_proxy));
  outgoing_pipes_.push_back(std::move(outgoing_pipe));
  outgoing_message_processors_.push_back(std::move(outgoing_message_processor));
}

ThreadPipeManager::ThreadPipeManager(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    BrowserConnectionHandler* connection_handler)
    : io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      io_pipe_manager_(new IoThreadPipeManager(connection_handler)) {}

ThreadPipeManager::~ThreadPipeManager() {
  io_task_runner_->DeleteSoon(FROM_HERE, io_pipe_manager_.release());
}

std::unique_ptr<BlimpMessageProcessor> ThreadPipeManager::RegisterFeature(
    BlimpMessage::Type type,
    BlimpMessageProcessor* incoming_processor) {
  // Creates an outgoing pipe and a proxy for forwarding messages
  // from features on the UI thread to network components on the IO thread.
  std::unique_ptr<BlimpMessageThreadPipe> outgoing_pipe(
      new BlimpMessageThreadPipe(io_task_runner_));
  std::unique_ptr<BlimpMessageProcessor> outgoing_proxy =
      outgoing_pipe->CreateProxy();

  // Creates an incoming pipe and a proxy for receiving messages
  // from network components on the IO thread.
  std::unique_ptr<BlimpMessageThreadPipe> incoming_pipe(
      new BlimpMessageThreadPipe(ui_task_runner_));
  incoming_pipe->set_target_processor(incoming_processor);
  std::unique_ptr<BlimpMessageProcessor> incoming_proxy =
      incoming_pipe->CreateProxy();

  // Finishes registration on IO thread.
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&IoThreadPipeManager::RegisterFeature,
                            base::Unretained(io_pipe_manager_.get()), type,
                            base::Passed(std::move(outgoing_pipe)),
                            base::Passed(std::move(incoming_proxy))));

  incoming_pipes_.push_back(std::move(incoming_pipe));
  return outgoing_proxy;
}

}  // namespace blimp
