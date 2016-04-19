// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_ipc_server.h"

#include "base/metrics/histogram_delta_serialization.h"
#include "build/build_config.h"
#include "chrome/common/service_messages.h"
#include "ipc/ipc_logging.h"

ServiceIPCServer::ServiceIPCServer(
    Client* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const IPC::ChannelHandle& channel_handle,
    base::WaitableEvent* shutdown_event)
    : client_(client),
      io_task_runner_(io_task_runner),
      channel_handle_(channel_handle),
      shutdown_event_(shutdown_event),
      ipc_client_connected_(false) {
  DCHECK(client);
  DCHECK(shutdown_event);
}

bool ServiceIPCServer::Init() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(this);
#endif
  CreateChannel();
  return true;
}

void ServiceIPCServer::CreateChannel() {
  channel_.reset();  // Tear down the existing channel, if any.
  channel_ = IPC::SyncChannel::Create(
      channel_handle_,
      IPC::Channel::MODE_NAMED_SERVER,
      this /* listener */,
      io_task_runner_,
      true /* create_pipe_now */,
      shutdown_event_);
}

ServiceIPCServer::~ServiceIPCServer() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(NULL);
#endif
}

void ServiceIPCServer::OnChannelConnected(int32_t peer_pid) {
  DCHECK(!ipc_client_connected_);
  ipc_client_connected_ = true;
}

void ServiceIPCServer::OnChannelError() {
  // When an IPC client (typically a browser process) disconnects, the pipe is
  // closed and we get an OnChannelError. If we want to keep servicing requests,
  // we will recreate the channel if necessary.
  bool client_was_connected = ipc_client_connected_;
  ipc_client_connected_ = false;
  if (client_was_connected) {
    if (client_->OnIPCClientDisconnect()) {
#if defined(OS_WIN)
      // On Windows, once an error on a named pipe occurs, the named pipe is no
      // longer valid and must be re-created. This is not the case on Mac or
      // Linux.
      CreateChannel();
#endif
    }
  } else {
    // If the client was never even connected we had an error connecting.
    if (!ipc_client_connected_) {
      LOG(ERROR) << "Unable to open service ipc channel "
                 << "named: " << channel_handle_.name;
    }
  }
}

bool ServiceIPCServer::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

void ServiceIPCServer::AddMessageHandler(
    std::unique_ptr<MessageHandler> handler) {
  message_handlers_.push_back(handler.release());
}

bool ServiceIPCServer::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  // When we get a message, always mark the IPC client as connected. The
  // ChannelProxy::Context is only letting OnChannelConnected get called once,
  // so on Mac and Linux, we never would set ipc_client_connected_ to true
  // again on subsequent connections.
  ipc_client_connected_ = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceIPCServer, msg)
    IPC_MESSAGE_HANDLER(ServiceMsg_GetHistograms, OnGetHistograms)
    IPC_MESSAGE_HANDLER(ServiceMsg_Shutdown, OnShutdown);
    IPC_MESSAGE_HANDLER(ServiceMsg_UpdateAvailable, OnUpdateAvailable);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled) {
    // Make a copy of the handlers to prevent modification during iteration.
    std::vector<MessageHandler*> temp_handlers = message_handlers_.get();
    for (const auto& handler : temp_handlers) {
      handled = handler->HandleMessage(msg);
      if (handled)
        break;
    }
  }

  return handled;
}

void ServiceIPCServer::OnGetHistograms() {
  if (!histogram_delta_serializer_) {
    histogram_delta_serializer_.reset(
        new base::HistogramDeltaSerialization("ServiceProcess"));
  }
  std::vector<std::string> deltas;
  // "false" to PerpareAndSerializeDeltas() indicates to *not* include
  // histograms held in persistent storage on the assumption that they will be
  // visible to the recipient through other means.
  histogram_delta_serializer_->PrepareAndSerializeDeltas(&deltas, false);
  channel_->Send(new ServiceHostMsg_Histograms(deltas));
}

void ServiceIPCServer::OnShutdown() {
  client_->OnShutdown();
}

void ServiceIPCServer::OnUpdateAvailable() {
  client_->OnUpdateAvailable();
}
