// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_ipc_server.h"

#include "chrome/common/service_messages.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_process.h"
#include "ipc/ipc_logging.h"

ServiceIPCServer::ServiceIPCServer(const IPC::ChannelHandle& channel_handle)
    : channel_handle_(channel_handle), client_connected_(false) {
}

bool ServiceIPCServer::Init() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(this);
#endif
  sync_message_filter_ =
      new IPC::SyncMessageFilter(g_service_process->shutdown_event());
  CreateChannel();
  return true;
}

void ServiceIPCServer::CreateChannel() {
  channel_.reset(NULL); // Tear down the existing channel, if any.
  channel_.reset(new IPC::SyncChannel(channel_handle_,
      IPC::Channel::MODE_NAMED_SERVER, this,
      g_service_process->io_thread()->message_loop_proxy(), true,
      g_service_process->shutdown_event()));
  DCHECK(sync_message_filter_.get());
  channel_->AddFilter(sync_message_filter_.get());
}

ServiceIPCServer::~ServiceIPCServer() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(NULL);
#endif

  channel_->RemoveFilter(sync_message_filter_.get());

  // The ChannelProxy object caches a pointer to the IPC thread, so need to
  // reset it as it's not guaranteed to outlive this object.
  // NOTE: this also has the side-effect of not closing the main IPC channel to
  // the browser process.  This is needed because this is the signal that the
  // browser uses to know that this process has died, so we need it to be alive
  // until this process is shut down, and the OS closes the handle
  // automatically.  We used to watch the object handle on Windows to do this,
  // but it wasn't possible to do so on POSIX.
  channel_->ClearIPCMessageLoop();
}

void ServiceIPCServer::OnChannelConnected(int32 peer_pid) {
  DCHECK(!client_connected_);
  client_connected_ = true;
}

void ServiceIPCServer::OnChannelError() {
  // When a client (typically a browser process) disconnects, the pipe is
  // closed and we get an OnChannelError. Since we want to keep servicing
  // client requests, we will recreate the channel.
  bool client_was_connected = client_connected_;
  client_connected_ = false;
  // TODO(sanjeevr): Instead of invoking the service process for such handlers,
  // define a Client interface that the ServiceProcess can implement.
  if (client_was_connected) {
    if (g_service_process->HandleClientDisconnect()) {
#if defined(OS_WIN)
      // On Windows, once an error on a named pipe occurs, the named pipe is no
      // longer valid and must be re-created. This is not the case on Mac or
      // Linux.
      CreateChannel();
#endif
    }
  } else {
    // If the client was never even connected we had an error connecting.
    if (!client_connected_) {
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

bool ServiceIPCServer::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  // When we get a message, always mark the client as connected. The
  // ChannelProxy::Context is only letting OnChannelConnected get called once,
  // so on the Mac and Linux, we never would set client_connected_ to true
  // again on subsequent connections.
  client_connected_ = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceIPCServer, msg)
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableCloudPrintProxy,
                        OnEnableCloudPrintProxy)
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableCloudPrintProxyWithRobot,
                        OnEnableCloudPrintProxyWithRobot)
    IPC_MESSAGE_HANDLER(ServiceMsg_DisableCloudPrintProxy,
                        OnDisableCloudPrintProxy)
    IPC_MESSAGE_HANDLER(ServiceMsg_GetCloudPrintProxyInfo,
                        OnGetCloudPrintProxyInfo)
    IPC_MESSAGE_HANDLER(ServiceMsg_Shutdown, OnShutdown);
    IPC_MESSAGE_HANDLER(ServiceMsg_UpdateAvailable, OnUpdateAvailable);
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableVirtualDriver,
                        OnEnableVirtualDriver);
    IPC_MESSAGE_HANDLER(ServiceMsg_DisableVirtualDriver,
                        OnDisableVirtualDriver);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceIPCServer::OnEnableCloudPrintProxy(const std::string& lsid) {
  g_service_process->GetCloudPrintProxy()->EnableForUser(lsid);
}

void ServiceIPCServer::OnEnableCloudPrintProxyWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email) {
  g_service_process->GetCloudPrintProxy()->EnableForUserWithRobot(
      robot_auth_code,
      robot_email,
      user_email);
}

void ServiceIPCServer::OnGetCloudPrintProxyInfo() {
  cloud_print::CloudPrintProxyInfo info;
  g_service_process->GetCloudPrintProxy()->GetProxyInfo(&info);
  channel_->Send(new ServiceHostMsg_CloudPrintProxy_Info(info));
}

void ServiceIPCServer::OnDisableCloudPrintProxy() {
  // User disabled CloudPrint proxy explicitly. Delete printers
  // registered from this proxy and disable proxy.
  g_service_process->GetCloudPrintProxy()->
      UnregisterPrintersAndDisableForUser();
}

void ServiceIPCServer::OnShutdown() {
  g_service_process->Shutdown();
}

void ServiceIPCServer::OnUpdateAvailable() {
  g_service_process->SetUpdateAvailable();
}

void ServiceIPCServer::OnEnableVirtualDriver() {
  g_service_process->EnableVirtualPrintDriver();
}

void ServiceIPCServer::OnDisableVirtualDriver() {
  g_service_process->DisableVirtualPrintDriver();
}

