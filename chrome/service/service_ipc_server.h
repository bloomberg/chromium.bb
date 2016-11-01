// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_IPC_SERVER_H_
#define CHROME_SERVICE_SERVICE_IPC_SERVER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace base {

class HistogramDeltaSerialization;
class WaitableEvent;

}  // namespace base

// This class handles IPC commands for the service process.
class ServiceIPCServer : public IPC::Listener, public IPC::Sender {
 public:
  class MessageHandler {
   public:
    virtual ~MessageHandler() {}
    // Must return true if the message is handled.
    virtual bool HandleMessage(const IPC::Message& message) = 0;
  };

  class Client {
   public:
    virtual ~Client() {}

    // Called when the service process must shut down.
    virtual void OnShutdown() = 0;

    // Called when a product update is available.
    virtual void OnUpdateAvailable() = 0;

    // Called when the IPC channel is closed. A return value of true indicates
    // that the IPC server should continue listening for new connections.
    virtual bool OnIPCClientDisconnect() = 0;

    // Called to create a message pipe to use for an IPC Channel connection.
    virtual mojo::ScopedMessagePipeHandle CreateChannelMessagePipe() = 0;
  };

  ServiceIPCServer(
      Client* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      base::WaitableEvent* shutdown_event);
  ~ServiceIPCServer() override;

  bool Init();

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

  // Registers a MessageHandler with the ServiceIPCServer. When an IPC message
  // is received that is not handled by the ServiceIPCServer itself, the
  // handlers will be called to handle the message in first-add first-call order
  // until it is handled or there are no more handlers.
  void AddMessageHandler(std::unique_ptr<MessageHandler> handler);

  bool is_ipc_client_connected() const { return ipc_client_connected_; }

 private:
  friend class ServiceIPCServerTest;
  friend class MockServiceIPCServer;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // IPC message handlers.
  void OnGetHistograms();
  void OnShutdown();
  void OnUpdateAvailable();

  // Helper method to create the sync channel.
  void CreateChannel();

  Client* client_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  std::unique_ptr<IPC::SyncChannel> channel_;
  base::WaitableEvent* shutdown_event_;
  ScopedVector<MessageHandler> message_handlers_;

  // Indicates whether an IPC client is currently connected to the channel.
  bool ipc_client_connected_ = false;

  // Calculates histograms deltas.
  std::unique_ptr<base::HistogramDeltaSerialization>
      histogram_delta_serializer_;

  DISALLOW_COPY_AND_ASSIGN(ServiceIPCServer);
};

#endif  // CHROME_SERVICE_SERVICE_IPC_SERVER_H_
