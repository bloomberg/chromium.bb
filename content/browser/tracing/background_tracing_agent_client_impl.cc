// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_agent_client_impl.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/common/child_process_host_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void BackgroundTracingAgentClientImpl::Create(
    int child_process_id,
    mojo::PendingRemote<tracing::mojom::BackgroundTracingAgent> pending_agent) {
  uint64_t tracing_process_id =
      ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
          child_process_id);

  mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentClient> client;
  auto receiver = client.InitWithNewPipeAndPassReceiver();

  mojo::Remote<tracing::mojom::BackgroundTracingAgent> agent(
      std::move(pending_agent));
  agent->Initialize(tracing_process_id, std::move(client));

  // Lifetime bound to the agent, which means it is bound to the lifetime of
  // the child process. Will be cleaned up when the process exits.
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new BackgroundTracingAgentClientImpl(std::move(agent))),
      std::move(receiver));
}

BackgroundTracingAgentClientImpl::~BackgroundTracingAgentClientImpl() {
  BackgroundTracingManagerImpl::GetInstance()->RemoveAgent(agent_.get());
}

void BackgroundTracingAgentClientImpl::OnInitialized() {
  BackgroundTracingManagerImpl::GetInstance()->AddAgent(agent_.get());
}

void BackgroundTracingAgentClientImpl::OnTriggerBackgroundTrace(
    const std::string& name) {
  BackgroundTracingManagerImpl::GetInstance()->OnHistogramTrigger(name);
}

void BackgroundTracingAgentClientImpl::OnAbortBackgroundTrace() {
  BackgroundTracingManagerImpl::GetInstance()->AbortScenario();
}

BackgroundTracingAgentClientImpl::BackgroundTracingAgentClientImpl(
    mojo::Remote<tracing::mojom::BackgroundTracingAgent> agent)
    : agent_(std::move(agent)) {
  DCHECK(agent_);
}

}  // namespace content
