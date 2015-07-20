// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_IPC_SERVER_H_
#define CHROME_SERVICE_SERVICE_IPC_SERVER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sender.h"

namespace base {

class DictionaryValue;
class HistogramDeltaSerialization;
class WaitableEvent;

}  // namespace base

// This class handles IPC commands for the service process.
class ServiceIPCServer : public IPC::Listener, public IPC::Sender {
 public:
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
  };

  ServiceIPCServer(
      Client* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const IPC::ChannelHandle& handle,
      base::WaitableEvent* shutdown_event);
  ~ServiceIPCServer() override;

  bool Init();

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

  bool is_ipc_client_connected() const { return ipc_client_connected_; }

 private:
  friend class MockServiceIPCServer;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32 peer_pid) override;
  void OnChannelError() override;

  // IPC message handlers.
  void OnEnableCloudPrintProxyWithRobot(
      const std::string& robot_auth_code,
      const std::string& robot_email,
      const std::string& user_email,
      const base::DictionaryValue& user_settings);
  void OnGetCloudPrintProxyInfo();
  void OnGetHistograms();
  void OnGetPrinters();
  void OnDisableCloudPrintProxy();

  void OnShutdown();
  void OnUpdateAvailable();

  // Helper method to create the sync channel.
  void CreateChannel();

  Client* client_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  IPC::ChannelHandle channel_handle_;
  scoped_ptr<IPC::SyncChannel> channel_;
  base::WaitableEvent* shutdown_event_;

  // Indicates whether an IPC client is currently connected to the channel.
  bool ipc_client_connected_;

  // Calculates histograms deltas.
  scoped_ptr<base::HistogramDeltaSerialization> histogram_delta_serializer_;

  DISALLOW_COPY_AND_ASSIGN(ServiceIPCServer);
};

#endif  // CHROME_SERVICE_SERVICE_IPC_SERVER_H_
