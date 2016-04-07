// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_child_connection.h"

#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/mojo/constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/mojo_shell_connection.h"
#include "ipc/ipc_sender.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"

namespace content {
namespace {

const char kMojoRenderProcessHostConnection[] =
    "mojo_render_process_host_connection";

class RenderProcessHostConnection : public base::SupportsUserData::Data {
 public:
  explicit RenderProcessHostConnection(scoped_ptr<mojo::Connection> connection)
      : connection_(std::move(connection)) {}
  ~RenderProcessHostConnection() override {}

  mojo::Connection* get() const { return connection_.get(); }

 private:
  scoped_ptr<mojo::Connection> connection_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostConnection);
};

void SetMojoConnection(RenderProcessHost* render_process_host,
                       scoped_ptr<mojo::Connection> connection) {
  render_process_host->SetUserData(
      kMojoRenderProcessHostConnection,
      new RenderProcessHostConnection(std::move(connection)));
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

std::string MojoConnectToChild(int child_process_id,
                               int instance_id,
                               RenderProcessHost* render_process_host) {
  // Generate a token and create a pipe which is bound to it. This pipe is
  // passed to the shell if one is available.
  std::string pipe_token = mojo::edk::GenerateRandomToken();
  mojo::ScopedMessagePipeHandle shell_client_pipe =
      mojo::edk::CreateParentMessagePipe(pipe_token);

  // Some process types get created before the main message loop. In this case
  // the shell request pipe will simply be closed, and the child can detect
  // this.
  if (!MojoShellConnection::Get())
    return pipe_token;

  mojo::shell::mojom::ShellClientPtr client;
  client.Bind(mojo::InterfacePtrInfo<mojo::shell::mojom::ShellClient>(
      std::move(shell_client_pipe), 0u));
  mojo::shell::mojom::PIDReceiverPtr pid_receiver;
  mojo::shell::mojom::PIDReceiverRequest pid_receiver_request =
      GetProxy(&pid_receiver);
  // PIDSender manages its own lifetime.
  new PIDSender(render_process_host, std::move(pid_receiver));

  mojo::Identity target(kRendererMojoApplicationName,
                        mojo::shell::mojom::kInheritUserID,
                        base::StringPrintf("%d_%d", child_process_id,
                                           instance_id));
  mojo::Connector::ConnectParams params(target);
  params.set_client_process_connection(std::move(client),
                                       std::move(pid_receiver_request));
  scoped_ptr<mojo::Connection> connection =
      MojoShellConnection::Get()->GetConnector()->Connect(&params);

  // Store the connection on the RPH so client code can access it later via
  // GetMojoConnection().
  SetMojoConnection(render_process_host, std::move(connection));

  return pipe_token;
}

mojo::Connection* GetMojoConnection(RenderProcessHost* render_process_host) {
  RenderProcessHostConnection* connection =
      static_cast<RenderProcessHostConnection*>(
          render_process_host->GetUserData(kMojoRenderProcessHostConnection));
  return connection ? connection->get() : nullptr;
}

}  // namespace content
