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
#include "services/shell/public/interfaces/service.mojom.h"

namespace content {

MojoChildConnection::MojoChildConnection(const std::string& application_name,
                                         const std::string& instance_id,
                                         const std::string& child_token,
                                         shell::Connector* connector)
    : service_token_(mojo::edk::GenerateRandomToken()) {
  mojo::ScopedMessagePipeHandle service_pipe =
      mojo::edk::CreateParentMessagePipe(service_token_, child_token);

  shell::mojom::ServicePtr service;
  service.Bind(mojo::InterfacePtrInfo<shell::mojom::Service>(
      std::move(service_pipe), 0u));
  shell::mojom::PIDReceiverRequest pid_receiver_request =
      GetProxy(&pid_receiver_);

  shell::Identity target(application_name, shell::mojom::kInheritUserID,
                         instance_id);
  shell::Connector::ConnectParams params(target);
  params.set_client_process_connection(std::move(service),
                                       std::move(pid_receiver_request));

  // In some unit testing scenarios a null connector is passed.
  if (!connector)
    return;

  connection_ = connector->Connect(&params);
#if defined(OS_ANDROID)
  service_registry_android_ = ServiceRegistryAndroid::Create(
      connection_->GetInterfaceRegistry(), connection_->GetRemoteInterfaces());
#endif
}

MojoChildConnection::~MojoChildConnection() {}

void MojoChildConnection::SetProcessHandle(base::ProcessHandle handle) {
  DCHECK(pid_receiver_.is_bound());
  pid_receiver_->SetPID(base::GetProcId(handle));
  pid_receiver_.reset();
}

}  // namespace content
