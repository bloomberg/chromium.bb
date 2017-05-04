// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/pending_process_connection.h"

#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"

namespace mojo {
namespace edk {

PendingProcessConnection::PendingProcessConnection()
    : PendingProcessConnection(Type::kBrokerIntroduction) {}

PendingProcessConnection::PendingProcessConnection(Type type)
    : type_(type), process_token_(GenerateRandomToken()) {
  DCHECK(internal::g_core);

  // TODO(rockot): Support other process connection types.
  DCHECK_EQ(Type::kBrokerIntroduction, type_);
}

PendingProcessConnection::~PendingProcessConnection() {
  if (has_message_pipes_ && !connected_) {
    DCHECK(internal::g_core);
    internal::g_core->ChildLaunchFailed(process_token_);
  }
}

ScopedMessagePipeHandle PendingProcessConnection::CreateMessagePipe(
    std::string* token) {
  has_message_pipes_ = true;
  DCHECK(internal::g_core);
  DCHECK_EQ(Type::kBrokerIntroduction, type_);
  *token = GenerateRandomToken();
  return internal::g_core->CreateParentMessagePipe(*token, process_token_);
}

ScopedMessagePipeHandle PendingProcessConnection::CreateMessagePipe(
    const std::string& name) {
  // TODO(rockot): Implement this.
  NOTREACHED();
  return ScopedMessagePipeHandle();
}

void PendingProcessConnection::Connect(
    base::ProcessHandle process,
    ConnectionParams connection_params,
    const ProcessErrorCallback& error_callback) {
  // It's now safe to avoid cleanup in the destructor, as the lifetime of any
  // associated resources is effectively bound to the |channel| passed to
  // AddChild() below.
  DCHECK(!connected_);
  connected_ = true;

  DCHECK(internal::g_core);
  internal::g_core->AddChild(process, std::move(connection_params),
                             process_token_, error_callback);
}

}  // namespace edk
}  // namespace mojo
