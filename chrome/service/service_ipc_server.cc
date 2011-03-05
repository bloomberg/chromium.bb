// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
      g_service_process->io_thread()->message_loop(), true,
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
  IPC_BEGIN_MESSAGE_MAP(ServiceIPCServer, msg)
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableCloudPrintProxy,
                        OnEnableCloudPrintProxy)
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableCloudPrintProxyWithTokens,
                        OnEnableCloudPrintProxyWithTokens)
    IPC_MESSAGE_HANDLER(ServiceMsg_DisableCloudPrintProxy,
                        OnDisableCloudPrintProxy)
    IPC_MESSAGE_HANDLER(ServiceMsg_IsCloudPrintProxyEnabled,
                        OnIsCloudPrintProxyEnabled)
#if defined(ENABLE_REMOTING)
    IPC_MESSAGE_HANDLER(ServiceMsg_SetRemotingHostCredentials,
                        OnSetRemotingHostCredentials)
    IPC_MESSAGE_HANDLER(ServiceMsg_EnableRemotingHost,
                        OnEnableRemotingHost)
    IPC_MESSAGE_HANDLER(ServiceMsg_DisableRemotingHost,
                        OnDisableRemotingHost)
    IPC_MESSAGE_HANDLER(ServiceMsg_GetRemotingHostInfo,
                        OnGetRemotingHostInfo)
#endif  // defined(ENABLE_REMOTING)
    IPC_MESSAGE_HANDLER(ServiceMsg_Shutdown, OnShutdown);
    IPC_MESSAGE_HANDLER(ServiceMsg_UpdateAvailable, OnUpdateAvailable);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceIPCServer::OnEnableCloudPrintProxy(const std::string& lsid) {
  g_service_process->GetCloudPrintProxy()->EnableForUser(lsid);
}

void ServiceIPCServer::OnEnableCloudPrintProxyWithTokens(
    const std::string& cloud_print_token, const std::string& talk_token) {
  // TODO(sanjeevr): Implement this.
  NOTIMPLEMENTED();
}

void ServiceIPCServer::OnIsCloudPrintProxyEnabled() {
  std::string email;
  bool is_enabled = g_service_process->GetCloudPrintProxy()->IsEnabled(&email);
  channel_->Send(new ServiceHostMsg_CloudPrintProxy_IsEnabled(is_enabled,
                                                              email));
}

#if defined(ENABLE_REMOTING)
void ServiceIPCServer::OnSetRemotingHostCredentials(
    const std::string& login,
    const std::string& auth_token) {
  g_service_process->remoting_host_manager()->SetCredentials(
      login, auth_token);
}

void ServiceIPCServer::OnEnableRemotingHost() {
  g_service_process->remoting_host_manager()->Enable();
  SendRemotingHostInfo();
}

void ServiceIPCServer::OnDisableRemotingHost() {
  g_service_process->remoting_host_manager()->Disable();
  SendRemotingHostInfo();
}

void ServiceIPCServer::OnGetRemotingHostInfo() {
  SendRemotingHostInfo();
}

void ServiceIPCServer::SendRemotingHostInfo() {
  remoting::ChromotingHostInfo host_info;
  g_service_process->remoting_host_manager()->GetHostInfo(&host_info);
  channel_->Send(new ServiceHostMsg_RemotingHost_HostInfo(host_info));
}
#endif  // defined(ENABLE_REMOTING)

void ServiceIPCServer::OnDisableCloudPrintProxy() {
  g_service_process->GetCloudPrintProxy()->DisableForUser();
}

void ServiceIPCServer::OnShutdown() {
  g_service_process->Shutdown();
}

void ServiceIPCServer::OnUpdateAvailable() {
  g_service_process->SetUpdateAvailable();
}
