// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_shell_client_host.h"

#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/mojo/mojo_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/mojo_shell_connection.h"
#include "ipc/ipc_sender.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"

namespace content {
namespace {

const char kMojoShellInstanceURL[] = "mojo_shell_instance_url";
const char kMojoPlatformFile[] = "mojo_platform_file";

base::PlatformFile PlatformFileFromScopedPlatformHandle(
    mojo::edk::ScopedPlatformHandle handle) {
  return handle.release().handle;
}

class InstanceURL : public base::SupportsUserData::Data {
 public:
  InstanceURL(const std::string& instance_url) : instance_url_(instance_url) {}
  ~InstanceURL() override {}

  std::string get() const { return instance_url_; }

 private:
  std::string instance_url_;

  DISALLOW_COPY_AND_ASSIGN(InstanceURL);
};

class InstanceShellHandle : public base::SupportsUserData::Data {
 public:
  InstanceShellHandle(base::PlatformFile shell_handle)
      : shell_handle_(shell_handle) {}
  ~InstanceShellHandle() override {}

  base::PlatformFile get() const { return shell_handle_; }

 private:
  base::PlatformFile shell_handle_;

  DISALLOW_COPY_AND_ASSIGN(InstanceShellHandle);
};

void SetMojoApplicationInstanceURL(RenderProcessHost* render_process_host,
                                   const std::string& instance_url) {
  render_process_host->SetUserData(kMojoShellInstanceURL,
                                   new InstanceURL(instance_url));
}

void SetMojoPlatformFile(RenderProcessHost* render_process_host,
                         base::PlatformFile platform_file) {
  render_process_host->SetUserData(kMojoPlatformFile,
                                   new InstanceShellHandle(platform_file));
}

class PIDSender : public RenderProcessHostObserver {
 public:
  PIDSender(
      RenderProcessHost* host,
      mojo::shell::mojom::PIDReceiverPtr pid_receiver)
      : host_(host),
        pid_receiver_(std::move(pid_receiver)) {
    pid_receiver_.set_connection_error_handler([this]() { delete this; });
    DCHECK(!host_->IsReady());
    host_->AddObserver(this);
  }
  ~PIDSender() override {
    if (host_)
      host_->RemoveObserver(this);
  }

 private:
  // Overridden from RenderProcessHostObserver:
  void RenderProcessReady(RenderProcessHost* host) override {
    pid_receiver_->SetPID(base::GetProcId(host->GetHandle()));
    delete this;
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    DCHECK_EQ(host_, host);
    host_ = nullptr;
  }

  RenderProcessHost* host_;
  mojo::shell::mojom::PIDReceiverPtr pid_receiver_;

  DISALLOW_COPY_AND_ASSIGN(PIDSender);
};

}  // namespace

void RegisterChildWithExternalShell(int child_process_id,
                                    RenderProcessHost* render_process_host) {
  // Some process types get created before the main message loop.
  if (!MojoShellConnection::Get())
    return;

  // Create the channel to be shared with the target process.
  mojo::edk::HandlePassingInformation handle_passing_info;
  mojo::edk::PlatformChannelPair platform_channel_pair;

  // Give one end to the shell so that it can create an instance.
  mojo::edk::ScopedPlatformHandle parent_pipe =
      platform_channel_pair.PassServerHandle();

  // Send the other end to the child via Chrome IPC.
  base::PlatformFile client_file = PlatformFileFromScopedPlatformHandle(
      platform_channel_pair.PassClientHandle());
  SetMojoPlatformFile(render_process_host, client_file);

  mojo::ScopedMessagePipeHandle request_pipe =
      mojo::edk::CreateMessagePipe(std::move(parent_pipe));

  mojo::shell::mojom::ApplicationManagerPtr application_manager;
  MojoShellConnection::Get()->GetShell()->ConnectToService(
      "mojo:shell", &application_manager);

  // The content of the URL/qualifier we pass is actually meaningless, it's only
  // important that they're unique per process.
  // TODO(beng): We need to specify a restrictive CapabilityFilter here that
  //             matches the needs of the target process. Figure out where that
  //             specification is best determined (not here, this is a common
  //             chokepoint for all process types) and how to wire it through.
  //             http://crbug.com/555393
  std::string url =
      base::StringPrintf("exe:chrome_renderer%d", child_process_id);

  mojo::shell::mojom::PIDReceiverPtr pid_receiver;
  mojo::InterfaceRequest<mojo::shell::mojom::PIDReceiver> request =
      GetProxy(&pid_receiver);
  new PIDSender(render_process_host, std::move(pid_receiver));

  application_manager->CreateInstanceForHandle(
      mojo::ScopedHandle(mojo::Handle(request_pipe.release().value())),
      url,
      CreateCapabilityFilterForRenderer(),
      std::move(request));

  // Store the URL on the RPH so client code can access it later via
  // GetMojoApplicationInstanceURL().
  SetMojoApplicationInstanceURL(render_process_host, url);
}

std::string GetMojoApplicationInstanceURL(
    RenderProcessHost* render_process_host) {
  InstanceURL* instance_url = static_cast<InstanceURL*>(
      render_process_host->GetUserData(kMojoShellInstanceURL));
  return instance_url ? instance_url->get() : std::string();
}

void SendExternalMojoShellHandleToChild(
    base::ProcessHandle process_handle,
    RenderProcessHost* render_process_host) {
  InstanceShellHandle* client_file = static_cast<InstanceShellHandle*>(
      render_process_host->GetUserData(kMojoPlatformFile));
  if (!client_file)
    return;
  render_process_host->Send(new MojoMsg_BindExternalMojoShellHandle(
      IPC::GetFileHandleForProcess(client_file->get(), process_handle, true)));
}

}  // namespace content
