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

const char kMojoShellInstanceIdentity[] = "mojo_shell_instance_identity";

class InstanceIdentity : public base::SupportsUserData::Data {
 public:
  explicit InstanceIdentity(const mojo::Identity& identity)
      : identity_(identity) {}
  ~InstanceIdentity() override {}

  mojo::Identity get() const { return identity_; }

 private:
  mojo::Identity identity_;

  DISALLOW_COPY_AND_ASSIGN(InstanceIdentity);
};

void SetMojoIdentity(RenderProcessHost* render_process_host,
                     const mojo::Identity& identity) {
  render_process_host->SetUserData(kMojoShellInstanceIdentity,
                                   new InstanceIdentity(identity));
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

void OnConnectionComplete(mojo::shell::mojom::ConnectResult result) {}

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

  mojo::shell::mojom::PIDReceiverPtr pid_receiver;
  mojo::InterfaceRequest<mojo::shell::mojom::PIDReceiver> request =
      GetProxy(&pid_receiver);
  new PIDSender(render_process_host, std::move(pid_receiver));

  mojo::shell::mojom::ShellClientFactoryPtr factory;
  factory.Bind(mojo::InterfacePtrInfo<mojo::shell::mojom::ShellClientFactory>(
      std::move(request_pipe), 0u));

  mojo::Identity target("exe:chrome_renderer",
                        mojo::shell::mojom::kInheritUserID,
                        base::StringPrintf("%d_%d", child_process_id,
                                           instance_id));
  shell->CreateInstance(std::move(factory),
                        mojo::shell::mojom::Identity::From(target),
                        std::move(request), base::Bind(&OnConnectionComplete));

  // Store the Identity on the RPH so client code can access it later via
  // GetMojoIdentity().
  SetMojoIdentity(render_process_host, target);

  return pipe_token;
}

mojo::Identity GetMojoIdentity(RenderProcessHost* render_process_host) {
  InstanceIdentity* instance_identity = static_cast<InstanceIdentity*>(
      render_process_host->GetUserData(kMojoShellInstanceIdentity));
  return instance_identity ? instance_identity->get() : mojo::Identity();
}

}  // namespace content
