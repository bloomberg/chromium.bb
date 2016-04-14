// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_child_connection.h"

#include <stdint.h>
#include <utility>

#include "content/public/common/mojo_shell_connection.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace content {

MojoChildConnection::MojoChildConnection(const std::string& application_name,
                                         const std::string& instance_id)
    : shell_client_token_(mojo::edk::GenerateRandomToken()) {
  mojo::ScopedMessagePipeHandle shell_client_pipe =
      mojo::edk::CreateParentMessagePipe(shell_client_token_);

  // Some process types get created before the main message loop. In this case
  // the shell request pipe will simply be closed, and the child can detect
  // this.
  if (!MojoShellConnection::Get())
    return;

  shell::mojom::ShellClientPtr client;
  client.Bind(mojo::InterfacePtrInfo<shell::mojom::ShellClient>(
      std::move(shell_client_pipe), 0u));
  shell::mojom::PIDReceiverRequest pid_receiver_request =
      GetProxy(&pid_receiver_);

  shell::Identity target(application_name, shell::mojom::kInheritUserID,
                         instance_id);
  shell::Connector::ConnectParams params(target);
  params.set_client_process_connection(std::move(client),
                                       std::move(pid_receiver_request));
  connection_ = MojoShellConnection::Get()->GetConnector()->Connect(&params);
}

MojoChildConnection::~MojoChildConnection() {}

void MojoChildConnection::SetProcessHandle(base::ProcessHandle handle) {
  DCHECK(pid_receiver_.is_bound());
  pid_receiver_->SetPID(base::GetProcId(handle));
  pid_receiver_.reset();
}

}  // namespace content
