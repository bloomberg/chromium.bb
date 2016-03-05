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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/mojo_shell_connection.h"
#include "ipc/ipc_sender.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace content {
namespace {

const char kMojoShellInstanceURL[] = "mojo_shell_instance_url";

class InstanceURL : public base::SupportsUserData::Data {
 public:
  explicit InstanceURL(const std::string& instance_url)
      : instance_url_(instance_url) {}
  ~InstanceURL() override {}

  std::string get() const { return instance_url_; }

 private:
  std::string instance_url_;

  DISALLOW_COPY_AND_ASSIGN(InstanceURL);
};

void SetMojoApplicationInstanceURL(RenderProcessHost* render_process_host,
                                   const std::string& instance_url) {
  render_process_host->SetUserData(kMojoShellInstanceURL,
                                   new InstanceURL(instance_url));
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

std::string RegisterChildWithExternalShell(
    int child_process_id,
    int instance_id,
    RenderProcessHost* render_process_host) {
  // Generate a token and create a pipe which is bound to it. This pipe is
  // passed to the shell if one is available.
  std::string pipe_token = mojo::edk::GenerateRandomToken();
  mojo::ScopedMessagePipeHandle request_pipe =
      mojo::edk::CreateParentMessagePipe(pipe_token);

  // Some process types get created before the main message loop. In this case
  // the shell request pipe will simply be closed, and the child can detect
  // this.
  if (!MojoShellConnection::Get())
    return pipe_token;

  mojo::shell::mojom::ShellPtr shell;
  MojoShellConnection::Get()->GetConnector()->ConnectToInterface(
      "mojo:shell", &shell);

  // The content of the URL/qualifier we pass is actually meaningless, it's only
  // important that they're unique per process.
  // TODO(beng): We need to specify a restrictive CapabilityFilter here that
  //             matches the needs of the target process. Figure out where that
  //             specification is best determined (not here, this is a common
  //             chokepoint for all process types) and how to wire it through.
  //             http://crbug.com/555393
  std::string url = base::StringPrintf(
      "exe:chrome_renderer%d_%d", child_process_id, instance_id);

  mojo::shell::mojom::PIDReceiverPtr pid_receiver;
  mojo::InterfaceRequest<mojo::shell::mojom::PIDReceiver> request =
      GetProxy(&pid_receiver);
  new PIDSender(render_process_host, std::move(pid_receiver));

  mojo::shell::mojom::ShellClientFactoryPtr factory;
  factory.Bind(mojo::InterfacePtrInfo<mojo::shell::mojom::ShellClientFactory>(
      std::move(request_pipe), 0u));

  shell->CreateInstanceForFactory(std::move(factory), url,
                                  mojo::shell::mojom::Connector::kUserInherit,
                                  CreateCapabilityFilterForRenderer(),
                                  std::move(request));

  // Store the URL on the RPH so client code can access it later via
  // GetMojoApplicationInstanceURL().
  SetMojoApplicationInstanceURL(render_process_host, url);

  return pipe_token;
}

std::string GetMojoApplicationInstanceURL(
    RenderProcessHost* render_process_host) {
  InstanceURL* instance_url = static_cast<InstanceURL*>(
      render_process_host->GetUserData(kMojoShellInstanceURL));
  return instance_url ? instance_url->get() : std::string();
}

}  // namespace content
